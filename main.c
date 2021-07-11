#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "shell.h"

#include "main.h"
#include "printfs.h"
#include "aes.h"
#include "aes_secret.h"
#include "crc8.h"
#include "radio.h"
#include "nrf52_radio.h"
#include "nrf52_flash.h"
#include "nrf52_pof.h"
#include "nrf_secret.h"
#include "si7021.h"
#include "dht.h"

#define DEBUG			0
#define UPDATE_CONFIG	0	// update config

#define SLEEP_TIME		30	// default sleep, S
#define WAIT_TIME		70  // wait gateway message, mS

config_t config;
bool write_config = false;
event_source_t event_src;

#if DEBUG
static const SerialConfig serial_config = {
	.speed = SERIAL_DEFAULT_BITRATE,
    .tx_pad  = UART_TX,
    .rx_pad  = UART_RX,
};
#endif

static WDGConfig WDGcfg = {
  .pause_on_sleep = 0,
  .pause_on_halt  = 0,
  .timeout_ms     = 1000,
};

static void dosleep(uint16_t time) {
  for (uint32_t i=0; i<32; i++)
  {
	  // Put all other pins into default configuration, minimum power consumption
	  NRF_P0->PIN_CNF[i] = GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos;
	  NRF_P0->LATCH = (1 << i);
  }
  NRF_P0->LATCH = NRF_P0->LATCH;

  chThdSetPriority(HIGHPRIO);

  // switch to HFINT clock source
  NRF_CLOCK->TASKS_HFCLKSTOP = 1;
  while (NRF_CLOCK->HFCLKSTAT && CLOCK_HFCLKSTAT_STATE_Pos == CLOCK_HFCLKSTAT_STATE_Running);

  // disable SysTick RTC
  NRF_RTC1->TASKS_STOP  = 1;
  nvicDisableVector(RTC1_IRQn);

  WDGcfg.timeout_ms = 1000 * time;
  wdgStart(&WDGD1, &WDGcfg);

  // waiting watchdog reset
  while (true) {
	  __WFE();
	  __SEV();
	  __WFE();
  }
}

// called on kernel panic
void halt(void){
    port_disable();
    while(true) {
    	dosleep(SLEEP_TIME);
    }
}

static void default_config(void) {
    config.magic = MAGIC;
    config.deviceid = DEVICEID;
    config.channel = NRF_CHANNEL;
    memcpy(config.clt_addr, &NRF_CLTADDR, NRF_ADDR_LEN);
    memcpy(config.srv_addr, &NRF_SRVADDR, NRF_ADDR_LEN);
    config.dht_en = true;
    config.sleep = SLEEP_TIME;
    config.heater = 0;
}


/*
 * application main entry
 */
int main(void) {
    
	halInit();
	chSysInit();

#if DEBUG
    sdStart(&SD1, &serial_config);
    chprintf((BaseSequentialStream *) &SD1, "start:\r\n");

    NRF_POWER_Type *p = NRF_POWER;
    chprintf((BaseSequentialStream *) &SD1, "reset reason:%ld\r\n", p->RESETREAS);
#endif

    if (!initFlash()) {
        halt();
    }

    if (!readFlash((uint8_t *) &config) || config.magic != MAGIC) {
        default_config();
        if (!writeFlash((uint8_t *) &config)) {
            halt();
        }
    }

#if UPDATE_CONFIG
    default_config();
    if (!writeFlash((uint8_t *) &config)) {
        halt();
    }
#endif

    pof_init(POF_V22);

    int8_t si_rslt, dht_rslt;
  	int16_t si_temp, dht_temp;
  	uint16_t si_hum, dht_hum;

	while (true) {

	  if (pof_warning) {
#if DEBUG
		  chprintf((BaseSequentialStream *) &SD1, "POF warn %d\r\n", pof_warning);
#endif
		  radio_start();
		  send_vbat(ADDR_DEVICE, ERR_VBAT_LOW);
		  goto SLEEP;
	  }

	  palSetLineMode(LINE_PWR, PAL_MODE_OUTPUT_PUSHPULL);
	  palSetLine(LINE_PWR);

      si_rslt = si7021_init(SI7021_RES_RH10_T13);
      if (si_rslt == SI7021_OK) {
    	si_rslt = si7021_read(&si_hum, &si_temp);
    	if (si_rslt == SI7021_OK && config.heater > 0 && si_hum >= 1000) {
  		  if (si7021_heater_power(SI7021_HEATER_3MA) == SI7021_OK) {
  			si7021_heater(1);
  			chThdSleepSeconds(config.heater);
  			si7021_heater(0);
  		  }
    	}
      }
      si7021_stop();

	  if (config.dht_en) {
    	chThdSleepMilliseconds(DHT_PWRUP);
		dht_init();
      	dht_rslt = dht_read(&dht_temp, &dht_hum);
      	dht_stop();
      }

	  palSetLineMode(LINE_PWR, PAL_MODE_UNCONNECTED);

	  radio_start();

      if (si_rslt == SI7021_OK) {
#if DEBUG
    	chprintf((BaseSequentialStream *) &SD1, "SI7021 temp: %d, hum: %d\r\n", si_temp, si_hum);
#endif
		send_sensor_value(ADDR_SI7021_TEMP, si_temp, 1);
		send_sensor_value(ADDR_SI7021_HUM, si_hum, 1);
      } else {
		send_sensor_error(ADDR_SI7021_TEMP, si_rslt);
		send_sensor_error(ADDR_SI7021_HUM, si_rslt);
      }

	  if (config.dht_en) {
		  if (dht_rslt == DHT_OK) {
#if DEBUG
			  chprintf((BaseSequentialStream *) &SD1, "DHT %d, %d\r\n", dht_temp, dht_hum);
#endif
			  send_sensor_value(ADDR_DHT_TEMP, dht_temp, 1);
			  send_sensor_value(ADDR_DHT_HUM, dht_hum, 1);
		  } else {
			  send_sensor_error(ADDR_DHT_TEMP, dht_rslt);
			  send_sensor_error(ADDR_DHT_HUM, dht_rslt);
		  }
	  }

	  if (pof_warning)
		send_vbat(ADDR_DEVICE, ERR_VBAT_LOW);
	  else
		send_vbat(ADDR_DEVICE, ERR_NO_ERROR);

      do {
    	  send_msg_wait();
    	  chThdSleepMilliseconds(WAIT_TIME);
      } while (msg_received);

      if (write_config) {
          if (!writeFlash((uint8_t *) &config)) {
        	  send_cmd_error(ADDR_DEVICE, ERR_CFG_WRITE);
          }
          write_config = false;
      }

SLEEP:
	  pof_stop();
	  radio_stop();

	  if (config.sleep > 0)
		  dosleep(config.sleep);
	  else
		  dosleep(SLEEP_TIME);
    }
}

