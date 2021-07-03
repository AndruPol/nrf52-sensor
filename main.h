/*
 * main.h
 *
 *  Created on: 24.12.2020
 *      Author: andru
 */

#ifndef MAIN_H_
#define MAIN_H_

#include "ch.h"

#define FIRMWARE        101     // fw version
#define MAGIC           0xAE68  // eeprom magic data
#define DEVICEID        6		// default device id

#define NRF_ADDR_LEN	5
#define NRF_SEND_MAX	5		// max send attempts
#define NRF_RX_PIPE		0		// rx pipe
#define NRF_TX_PIPE		1		// tx pipe
#define NRF_SEND_MS		3		// timeout packet send, mS

#include "packet.h"

#define ADDRNUM			9
typedef enum {
	ADDR_DEVICE,		// VBAT critical & device status
	ADDR_SI7021_TEMP,	// SI7021 temperature
	ADDR_SI7021_HUM,	// SI7021 humidity
	ADDR_DHT_TEMP,		// DHT temperature
	ADDR_DHT_HUM,		// DHT humidity
	ADDR_CFG_SLEEP,		// sleep, S
	ADDR_CFG_CHANNEL,	// RF channel
	ADDR_CFG_DHT,		// DHT sensor enable
	ADDR_CFG_HEATER,	// SI7021 heater enable time
} address_t;

typedef enum {
	ERR_NO_ERROR,
	ERR_VBAT_LOW,
	ERR_BAD_ADDR,
	ERR_BAD_CMD,
	ERR_BAD_PARAM,
	ERR_CFG_WRITE,
} msg_error_t;

#define CFGLEN	sizeof(config_t)
typedef struct config config_t;
struct config {
	uint16_t magic;					// magic word
	uint8_t deviceid;				// this device id
	uint8_t channel;				// this device NRF channel
	uint8_t clt_addr[NRF_ADDR_LEN];	// NRF24 ESB this address [deviceid,1..4]
	uint8_t srv_addr[NRF_ADDR_LEN];	// NRF24 ESB server address
	uint16_t sleep;					// time, sec for device sleep
	bool dht_en;					// DHT enable flag
	uint8_t heater;					// enable SI7021 heater time
};

extern config_t config;
extern bool write_config;

#endif /* MAIN_H_ */
