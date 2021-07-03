/*
 * si7021.c
 *
 *  Created on: 28/02/2021
 *      Author: andru
 *
 *      SI7021 - relative humidity and temperature driver
 *
 */

#define SI7021_ADDR 	0x40 // device address
#define SI7021_I2CTIME	10	 // i2c timeout, ms

// SI7021 commands
#define MEASURE_RH_HM	0xE5
#define MEASURE_RH_NHM	0xF5
#define MEASURE_T_HM	0xE3
#define MEASURE_T_NHM	0xF3
#define READ_T_PREV_RH	0xE0
#define RESET			0xFE
#define WRITE_RHT_USER	0xE6
#define READ_RHT_USER	0xE7
#define WRITE_HEATER	0x51
#define READ_HEATER		0x11
#define READ_EID_1BYTE	0xFA0F
#define READ_EID_2BYTE	0xFCC9
#define READ_FIRMWARE	0x84B8

// user register masks
#define USER_MASK_VDDS	(1 << 6)
#define USER_MASK_HTRE	(1 << 2)
#define USER_MASK_RES	0x81
#define HEATER_MASK		0x0F

#define USE_CRC			1

#include "ch.h"
#include "hal.h"

#include "si7021.h"
#include "main.h"

static uint8_t convtime;

static I2CConfig i2ccfg = {
  100000,
  I2C_SCL,
  I2C_SDA,
  FALSE,
  FALSE
};

#if USE_CRC
static uint8_t calcCRC(uint8_t *data) {
	uint8_t crc = 0;
	int8_t i, j;

	for (i = 0; i < 2; i++) {
	    crc ^= data[i];
	    for (j = 8; j > 0; j--) {
	       if (crc & 0x80)
	          crc = (crc << 1) ^ 0x131;
	      else
	          crc <<= 1;
	    }
	}

	return crc;
}
#endif

si7021error_t si7021_init(uint8_t res) {
	uint8_t txbuf[2], rxbuf;

	chThdSleepMilliseconds(SI7021_TIME_PWRUP);

    palSetLineMode(LINE_I2C_SCL, PAL_MODE_OUTPUT_OPENDRAIN);
    palSetLineMode(LINE_I2C_SDA, PAL_MODE_OUTPUT_OPENDRAIN);

	i2cStart(&I2CD1, &i2ccfg);

	txbuf[0] = READ_RHT_USER;

	if (i2cMasterTransmitTimeout(&I2CD1, SI7021_ADDR, txbuf, 1, &rxbuf, 1, TIME_MS2I(SI7021_I2CTIME)) != MSG_OK)
		return SI7021_I2CERROR;

	if (rxbuf & USER_MASK_VDDS)
		return SI7021_VDDS;

	switch (res) {
	case SI7021_RES_RH12_T14:
		convtime = SI7021_TIME_RH12_T14;
		break;
	case SI7021_RES_RH8_T12:
		convtime = SI7021_TIME_RH8_T12;
		break;
	case SI7021_RES_RH10_T13:
		convtime = SI7021_TIME_RH10_T13;
		break;
	case SI7021_RES_RH11_T11:
		convtime = SI7021_TIME_RH11_T11;
		break;
	default:
		return SI7021_PARAMERR;
	}

	txbuf[0] = WRITE_RHT_USER;
	txbuf[1] = rxbuf & ~USER_MASK_RES;
	txbuf[1] |= res;

	if (i2cMasterTransmitTimeout(&I2CD1, SI7021_ADDR, txbuf, 2, NULL, 0, TIME_MS2I(SI7021_I2CTIME)) != MSG_OK)
		return SI7021_I2CERROR;

	return SI7021_OK;
}

void si7021_stop(void) {
	i2cStop(&I2CD1);

	palSetLineMode(LINE_I2C_SCL, PAL_MODE_UNCONNECTED);
    palSetLineMode(LINE_I2C_SDA, PAL_MODE_UNCONNECTED);
}

// if return SI7021_OK: relative humidity * 10 (%), temperature * 10 (C)
si7021error_t si7021_read(uint16_t *humidity, int16_t *temperature) {
	uint8_t txbuf=0, rxbuf[3];
	int32_t data;

	txbuf = MEASURE_RH_HM;
	if (i2cMasterTransmitTimeout(&I2CD1, SI7021_ADDR, &txbuf, 1, rxbuf, 3, TIME_MS2I(convtime)) != MSG_OK)
		return SI7021_TIMEOUT;

#if USE_CRC
	if (rxbuf[2] != calcCRC(rxbuf))
		return SI7021_CRCERROR;
#endif

	data = (uint32_t) ((uint16_t)(rxbuf[0] << 8) | rxbuf[1]);
    data = ((data * 1250) >> 16) - 60;
    *humidity = (uint16_t) data;
    if (*humidity > 1000) *humidity = 1000;

	txbuf = READ_T_PREV_RH;
	if (i2cMasterTransmitTimeout(&I2CD1, SI7021_ADDR, &txbuf, 1, rxbuf, 2, TIME_MS2I(SI7021_I2CTIME)) != MSG_OK)
		return SI7021_I2CERROR;

	data = (int32_t) ((uint16_t)(rxbuf[0] << 8) | rxbuf[1]);
	data = ((data * 17572) >> 16) - 4685;
	*temperature = (int16_t) data / 10;

	return SI7021_OK;
}

si7021error_t si7021_heater_power(uint8_t res) {
	uint8_t txbuf[2];

	switch (res) {
	case SI7021_HEATER_3MA:
		txbuf[1] = SI7021_HEATER_3MA;
		break;
	case SI7021_HEATER_9MA:
		txbuf[1] = SI7021_HEATER_9MA;
		break;
	case SI7021_HEATER_15MA:
		txbuf[1] = SI7021_HEATER_15MA;
		break;
	case SI7021_HEATER_27MA:
		txbuf[1] = SI7021_HEATER_27MA;
		break;
	default:
		return SI7021_PARAMERR;
	}

	txbuf[0] = WRITE_HEATER;
	if (i2cMasterTransmitTimeout(&I2CD1, SI7021_ADDR, txbuf, 2, NULL, 0, TIME_MS2I(SI7021_I2CTIME)) != MSG_OK)
		return SI7021_I2CERROR;

	return SI7021_OK;
}

si7021error_t si7021_heater(uint8_t res) {
	uint8_t txbuf[2], rxbuf;

	txbuf[0] = READ_RHT_USER;
	if (i2cMasterTransmitTimeout(&I2CD1, SI7021_ADDR, txbuf, 1, &rxbuf, 1, TIME_MS2I(SI7021_I2CTIME)) != MSG_OK)
		return SI7021_I2CERROR;

	txbuf[1] = rxbuf & ~USER_MASK_HTRE;
	if (res)
		txbuf[1] |= USER_MASK_HTRE;

	txbuf[0] = WRITE_RHT_USER;
	if (i2cMasterTransmitTimeout(&I2CD1, SI7021_ADDR, txbuf, 2, NULL, 0, TIME_MS2I(SI7021_I2CTIME)) != MSG_OK)
		return SI7021_I2CERROR;

	return SI7021_OK;
}
