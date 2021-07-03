/*
 * radio.h
 *
 *  Created on: 10 апр. 2019 г.
 *      Author: andru
 */

#ifndef RADIO_H_
#define RADIO_H_

#define NRF_READ_BUFFERS	12
#define NRF_SEND_BUFFERS	12

// processes priority also check nrf52_radio.h
#define RADIO_RECEIVE_PRIO	(NORMALPRIO + 2)
#define RADIO_SEND_PRIO		(NORMALPRIO + 2)
#define RADIO_EVENT_PRIO	(NORMALPRIO + 3)
#define RADIO_PARSE_PRIO	(NORMALPRIO + 1)


void radio_start(void);
void radio_stop(void);

void send_vbat(address_t addr, msg_error_t error);
void send_cmd_error(address_t addr, msg_error_t error);
void send_cfg_value(address_t addr, uint32_t value);
void send_sensor_value(uint8_t addr, int32_t value, int8_t power);
void send_sensor_error(uint8_t addr, uint8_t error);
void send_msg_wait(void);

extern volatile uint8_t	msg_received;

#endif /* RADIO_H_ */
