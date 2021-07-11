/*
 * radio.c
 *
 *  Created on: 10 апр. 2019 г.
 *      Author: andru
 */

#include <stdint.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "nrf52_radio.h"
#include "aes.h"
#include "aes_secret.h"
#include "main.h"
#include "crc8.h"
#include "radio.h"

#define DEBUG	FALSE

static MESSAGE_T nrf_read_buf[NRF_READ_BUFFERS];
static MESSAGE_T *read_free[NRF_READ_BUFFERS];
static MESSAGE_T *read_fill[NRF_READ_BUFFERS];
static mailbox_t mb_read_free, mb_read_fill;

static MESSAGE_T nrf_send_buf[NRF_SEND_BUFFERS];
static MESSAGE_T *send_free[NRF_SEND_BUFFERS];
static MESSAGE_T *send_fill[NRF_SEND_BUFFERS];
static mailbox_t mb_send_free, mb_send_fill;

static binary_semaphore_t nrf_send, nrf_receive;
static volatile eventflags_t nrf_flags;

volatile uint8_t msg_received = false;

static nrf52_config_t radiocfg = {
        .protocol = NRF52_PROTOCOL_ESB,
        .mode = NRF52_MODE_PRX,
        .bitrate = NRF52_BITRATE_1MBPS,
        .crc = NRF52_CRC_8BIT,
        .tx_power = NRF52_TX_POWER_4DBM,
        .tx_mode = NRF52_TXMODE_MANUAL,
        .selective_auto_ack = true,
        .retransmit = { 750, 3 },
        .payload_length = MSGLEN,
        .address = {
//                .base_addr_p0 = NRF_SRV_BASE,
//                .base_addr_p1 = NRF_CLT_BASE,
                .pipe_prefixes = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 },
                .num_pipes = 2,
                .addr_length = NRF_ADDR_LEN,
                .rx_pipes = 1 << NRF_RX_PIPE,
//                .rf_channel = NRF_CHANNEL,
        },
};

// NRF52 events process thread
static thread_t *radio_event_thd;
static THD_WORKING_AREA(waNRFEventThread, 256);
static THD_FUNCTION(NRFEventThread, arg) {
    (void)arg;

    event_listener_t el;
    chEvtRegisterMask(&RFD1.eventsrc, &el, EVENT_MASK(0));

    chRegSetThreadName("nrfEvent");

    while (!chThdShouldTerminateX()) {
    	chEvtWaitAny(EVENT_MASK(0));
    	nrf_flags = chEvtGetAndClearFlags(&el);
    	if (nrf_flags & NRF52_EVENT_TX_SUCCESS) {
    		chBSemSignal(&nrf_send);
    	}
    	if (nrf_flags & NRF52_EVENT_TX_FAILED) {
    		chBSemSignal(&nrf_send);
    	}
    	if (nrf_flags & NRF52_EVENT_RX_RECEIVED) {
    		chBSemSignal(&nrf_receive);
    	}
    }
    chThdExit((msg_t) 0);
}

static thread_t *radio_send_thd;
static THD_WORKING_AREA(waNRFSendThread, 384);
static THD_FUNCTION(nrfSendThread, arg) {
	(void)arg;

	chRegSetThreadName("nrfSend");

	chMBObjectInit(&mb_send_free, (msg_t*) send_free, NRF_SEND_BUFFERS);
	chMBObjectInit(&mb_send_fill, (msg_t*) send_fill, NRF_SEND_BUFFERS);

	// fill free buffers mailbox
	for (uint8_t i=0; i < NRF_SEND_BUFFERS; i++)
		chMBPostTimeout(&mb_send_free, (msg_t) &nrf_send_buf[i], TIME_IMMEDIATE);

	while (!chThdShouldTerminateX()) {
		uint8_t buf[MSGLEN];
		void *pbuf;
		nrf52_payload_t tx_payload = {
			.pipe = NRF_TX_PIPE,
			.length = MSGLEN,
		};

		if (chMBFetchTimeout(&mb_send_fill, (msg_t *) &pbuf, TIME_INFINITE) == MSG_OK) {
			memcpy(buf, pbuf, MSGLEN);
			chMBPostTimeout(&mb_send_free, (msg_t) pbuf, TIME_IMMEDIATE);
		} else {
			continue;
		}

		if (buf[0] != config.deviceid) continue;

		uint8_t aes[MSGLEN];
		buf[MSGLEN-1] = CRC8(buf, MSGLEN-1);
		AES128_ECB_encrypt(buf, aes_key, aes);
		memcpy(&tx_payload.data, aes, MSGLEN);

		uint8_t sendcnt = NRF_SEND_MAX;
		while (--sendcnt) {
			radio_stop_rx();
			radio_write_payload(&tx_payload);
			radio_start_tx();
			if (chBSemWaitTimeout(&nrf_send, TIME_MS2I(NRF_SEND_MS)) == MSG_OK) {
				if (nrf_flags == NRF52_EVENT_TX_SUCCESS) {
					break;
				}
			}
		}

		if (sendcnt == 0) {
			// set nRF52 send error
			nrf_flags = NRF52_EVENT_TX_FAILED;
		}
		radio_start_rx();
	}
	chThdExit((msg_t) 0);
}

static thread_t *radio_receive_thd;
static THD_WORKING_AREA(waNRFReceiveThread, 256);
static THD_FUNCTION(nrfReceiveThread, arg) {
  (void)arg;

  chRegSetThreadName("nrfReceive");

  chMBObjectInit(&mb_read_free, (msg_t*) read_free, NRF_READ_BUFFERS);
  chMBObjectInit(&mb_read_fill, (msg_t*) read_fill, NRF_READ_BUFFERS);

  // fill free buffers mailbox
  for (uint8_t i=0; i < NRF_READ_BUFFERS; i++)
	  chMBPostTimeout(&mb_read_free, (msg_t) &nrf_read_buf[i], TIME_IMMEDIATE);

  while (!chThdShouldTerminateX()) {
	  nrf52_payload_t rx_payload;

	  chBSemWait(&nrf_receive);

	  memset(rx_payload.data, 0, MSGLEN);

	  if (radio_read_rx_payload(&rx_payload) != NRF52_SUCCESS) continue;
	  if (rx_payload.pipe != NRF_RX_PIPE) continue;

	  void *pbuf;
	  if (chMBFetchTimeout(&mb_read_free, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK) {
		  memcpy(pbuf, rx_payload.data, MSGLEN);
		  chMBPostTimeout(&mb_read_fill, (msg_t) pbuf, TIME_IMMEDIATE);
	  } else {
		  continue;
	  }
  }
  chThdExit((msg_t) 0);
}

// message type SENSOR_INFO
static void parseMSGInfo(MESSAGE_T *msg) {

	if (msg->msgtype != MSG_INFO)
		return;

	switch (msg->address) {
	default:	//UNKNOWN SENSOR
	  break;
	}
}

// message type SENSOR_DATA
static void parseMSGData(MESSAGE_T *msg) {

	if (msg->msgtype != MSG_DATA)
		return;

	switch (msg->address) {
	default:	//UNKNOWN SENSOR
	  break;
	}
}

// message type SENSOR_ERROR
static void parseMSGError(MESSAGE_T *msg) {

	if (msg->msgtype != MSG_ERROR)
		return;

	switch (msg->address) {
	default:	// UNKNOWN ERROR
	  break;
	}
}

// message type SENSOR_CMD
static void parseMSGCmd(MESSAGE_T *msg) {

	if (msg->msgtype != MSG_CMD)
		return;

	msg_received = true;

	switch (msg->address) {
	case ADDR_DEVICE:
		if (msg->command == CMD_CFGWRITE) {
			if (msg->data.i32 < 1 || msg->data.i32 > 255) {
			    send_cmd_error(ADDR_DEVICE, ERR_BAD_PARAM);
			    break;
			}
			config.deviceid = (uint8_t) msg->data.i32;
			write_config = true;
		}
		send_cfg_value(ADDR_DEVICE, config.deviceid);
		break;
	case ADDR_CFG_CHANNEL:
		if (msg->command == CMD_CFGWRITE) {
			if (msg->data.i32 < 0 || msg->data.i32 > 100) {
			    send_cmd_error(ADDR_CFG_CHANNEL, ERR_BAD_PARAM);
			    break;
			}
			config.channel = (uint8_t) msg->data.i32;
			write_config = true;
		}
		send_cfg_value(ADDR_CFG_CHANNEL, config.channel);
		break;
	case ADDR_CFG_SLEEP:
		if (msg->command == CMD_CFGWRITE) {
			if (msg->data.i32 < 0 || msg->data.i32 > 36000) {
			    send_cmd_error(ADDR_CFG_SLEEP, ERR_BAD_PARAM);
			    break;
			}
			config.sleep = (uint16_t) msg->data.i32;
			write_config = true;
		}
		send_cfg_value(ADDR_CFG_SLEEP, config.sleep);
		break;
	case ADDR_CFG_DHT:
		if (msg->command == CMD_CFGWRITE) {
			if (msg->data.i32 < 0 || msg->data.i32 > 1) {
			    send_cmd_error(ADDR_CFG_DHT, ERR_BAD_PARAM);
			    break;
			}
			config.dht_en = (bool) msg->data.i32;
			write_config = true;
		}
		send_cfg_value(ADDR_CFG_DHT, config.dht_en);
		break;
	case ADDR_CFG_HEATER:
		if (msg->command == CMD_CFGWRITE) {
			if (msg->data.i32 < 0 || msg->data.i32 > 10) {
			    send_cmd_error(ADDR_CFG_HEATER, ERR_BAD_PARAM);
			    break;
			}
			config.heater = (uint8_t) msg->data.i32;
			write_config = true;
		}
		send_cfg_value(ADDR_CFG_HEATER, config.heater);
		break;
	default:
      send_cmd_error(ADDR_DEVICE, ERR_BAD_ADDR);
	  break;
	}
}

static thread_t *radio_parse_thd;
static THD_WORKING_AREA(waNRFParseThread, 256);
static THD_FUNCTION(nrfParseThread, arg) {
  (void)arg;

  chRegSetThreadName("nrfParse");

  while (!chThdShouldTerminateX()) {
	  void *pbuf;
	  uint8_t buf[MSGLEN];
	  MESSAGE_T rcvmsg;

	  if (chMBFetchTimeout(&mb_read_fill, (msg_t *) &pbuf, TIME_INFINITE) == MSG_OK) {
		  memcpy(&buf, pbuf, MSGLEN);
		  chMBPostTimeout(&mb_read_free, (msg_t) pbuf, TIME_IMMEDIATE);
	  } else {
		  continue;
	  }

	  AES128_ECB_decrypt(buf, aes_key, (uint8_t *) &rcvmsg);

	  if (rcvmsg.crc != CRC8((uint8_t *) &rcvmsg, MSGLEN-1))
		  continue;
	  if (rcvmsg.deviceid != config.deviceid)
		  continue;

	  switch (rcvmsg.msgtype) {
	  case MSG_INFO:
		  parseMSGInfo(&rcvmsg);
		  break;
	  case MSG_DATA:
		  parseMSGData(&rcvmsg);
		  break;
	  case MSG_ERROR:
		  parseMSGError(&rcvmsg);
		  break;
	  case MSG_CMD:
		  parseMSGCmd(&rcvmsg);
		  break;
	  default:
		  break;
	  }

  } // while
  chThdExit((msg_t) 0);
}

void radio_start(void) {
  config.clt_addr[NRF_ADDR_LEN-1] = config.deviceid;
  radiocfg.address.pipe_prefixes[NRF_RX_PIPE] = config.clt_addr[0];
  radiocfg.address.pipe_prefixes[NRF_TX_PIPE] = config.srv_addr[0];
  memcpy(radiocfg.address.base_addr_p0, &config.clt_addr[1], NRF_ADDR_LEN-1);
  memcpy(radiocfg.address.base_addr_p1, &config.srv_addr[1], NRF_ADDR_LEN-1);
  radiocfg.address.rf_channel = config.channel;
  radio_init(&radiocfg);

  chBSemObjectInit(&nrf_receive, TRUE);
  chBSemObjectInit(&nrf_send, TRUE);

  // NRF52 message receive thread
  radio_receive_thd = chThdCreateStatic(waNRFReceiveThread, sizeof(waNRFReceiveThread), RADIO_RECEIVE_PRIO, nrfReceiveThread, NULL);

  // NRF52 received messages parse thread
  radio_parse_thd = chThdCreateStatic(waNRFParseThread, sizeof(waNRFParseThread), RADIO_PARSE_PRIO, nrfParseThread, NULL);

  // NRF52 message send thread
  radio_send_thd = chThdCreateStatic(waNRFSendThread, sizeof(waNRFSendThread), RADIO_SEND_PRIO, nrfSendThread, NULL);

  // NRF52 events polling thread
  radio_event_thd = chThdCreateStatic(waNRFEventThread, sizeof(waNRFEventThread), RADIO_EVENT_PRIO, NRFEventThread, NULL);

  radio_start_rx();
}

void radio_stop(void) {
  radio_disable();

  MESSAGE_T msg;
  memset((void *) &msg, 0, MSGLEN);

  chThdTerminate(radio_parse_thd);
  void *pbuf;
  if (chMBFetchTimeout(&mb_read_free, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK) {
	  memcpy(pbuf, &msg, MSGLEN);
      chMBPostTimeout(&mb_read_fill, (msg_t) pbuf, TIME_IMMEDIATE);
  }
  chThdWait(radio_parse_thd);

  chThdTerminate(radio_send_thd);
  if (chMBFetchTimeout(&mb_send_free, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK) {
	  memcpy(pbuf, &msg, MSGLEN);
      chMBPostTimeout(&mb_send_fill, (msg_t) pbuf, TIME_IMMEDIATE);
  }
  chThdWait(radio_send_thd);

  chThdTerminate(radio_receive_thd);
  chBSemSignal(&nrf_receive);
  chThdWait(radio_receive_thd);

  chThdTerminate(radio_event_thd);
  RFD1.flags = 0;
  chEvtBroadcastFlags(&RFD1.eventsrc, (eventflags_t) 0);
  chThdWait(radio_event_thd);
}

static void msg_header(MESSAGE_T *msg) {
  memset(msg, 0, MSGLEN);
  msg->firmware = FIRMWARE;
  msg->deviceid = config.deviceid;
  msg->addrnum = ADDRNUM;
  msg->cmdparam = CMD_WAIT;
}

void send_vbat(address_t addr, msg_error_t error) {
  MESSAGE_T sndmsg;

  msg_header(&sndmsg);
  sndmsg.address = addr;
  sndmsg.msgtype = MSG_DATA;
  sndmsg.datatype = VAL_i32;
  sndmsg.data.i32 = error;
  if (error == ERR_VBAT_LOW) {
	  sndmsg.msgtype = MSG_ERROR;
	  sndmsg.error = error;
  }
  void *pbuf;
  if (chMBFetchTimeout(&mb_send_free, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK) {
	  memcpy(pbuf, &sndmsg, MSGLEN);
      chMBPostTimeout(&mb_send_fill, (msg_t) pbuf, TIME_IMMEDIATE);
  }
}

void send_cmd_error(address_t addr, msg_error_t error) {
  MESSAGE_T sndmsg;

  msg_header(&sndmsg);
  sndmsg.msgtype = MSG_ERROR;
  sndmsg.address = addr;
  sndmsg.error = error;
  sndmsg.data.i32 = error;
  void *pbuf;
  if (chMBFetchTimeout(&mb_send_free, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK) {
	  memcpy(pbuf, &sndmsg, MSGLEN);
      chMBPostTimeout(&mb_send_fill, (msg_t) pbuf, TIME_IMMEDIATE);
  }
}

void send_cfg_value(address_t addr, uint32_t value) {
  MESSAGE_T sndmsg;

  msg_header(&sndmsg);
  sndmsg.msgtype = MSG_INFO;
  sndmsg.address = addr;
  sndmsg.data.i32 = value;
  void *pbuf;
  if (chMBFetchTimeout(&mb_send_free, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK) {
	  memcpy(pbuf, &sndmsg, MSGLEN);
      chMBPostTimeout(&mb_send_fill, (msg_t) pbuf, TIME_IMMEDIATE);
  }
}

void send_sensor_value(uint8_t addr, int32_t value, int8_t power) {
  MESSAGE_T sndmsg;

  msg_header(&sndmsg);
  sndmsg.msgtype = MSG_DATA;
  sndmsg.address = addr;
  sndmsg.datatype = VAL_i32;
  sndmsg.data.i32 = value;
  sndmsg.datapower = power;
  void *pbuf;
  if (chMBFetchTimeout(&mb_send_free, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK) {
	  memcpy(pbuf, &sndmsg, MSGLEN);
      chMBPostTimeout(&mb_send_fill, (msg_t) pbuf, TIME_IMMEDIATE);
  }
}

void send_sensor_error(uint8_t addr, uint8_t error) {
  MESSAGE_T sndmsg;

  msg_header(&sndmsg);
  sndmsg.msgtype = MSG_ERROR;
  sndmsg.address = addr;
  sndmsg.error = error;
  sndmsg.data.i32 = error;
  void *pbuf;
  if (chMBFetchTimeout(&mb_send_free, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK) {
	  memcpy(pbuf, &sndmsg, MSGLEN);
      chMBPostTimeout(&mb_send_fill, (msg_t) pbuf, TIME_IMMEDIATE);
  }
}

void send_msg_wait(void) {
  MESSAGE_T sndmsg;

#if DEBUG
	chprintf((BaseSequentialStream *) &SD1, "msg: waiting..\r\n");
#endif
  msg_header(&sndmsg);
  sndmsg.msgtype = MSG_CMD;
  sndmsg.command = CMD_MSGWAIT;
  void *pbuf;
  if (chMBFetchTimeout(&mb_send_free, (msg_t *) &pbuf, TIME_IMMEDIATE) == MSG_OK) {
	  memcpy(pbuf, &sndmsg, MSGLEN);
      chMBPostTimeout(&mb_send_fill, (msg_t) pbuf, TIME_IMMEDIATE);
  }
  msg_received = false;
}

