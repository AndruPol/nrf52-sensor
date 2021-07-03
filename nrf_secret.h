/*
 * nrf_secret.h
 *
 *  Created on: 24 июня 2019 г.
 *      Author: andru
 */

#ifndef NRF_SECRET_H_
#define NRF_SECRET_H_

#define NRF_ADDR_LEN	5		// NRF24 ESB device address length

#define NRF_CHANNEL     0x50
const uint8_t NRF_CLTADDR[NRF_ADDR_LEN] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
const uint8_t NRF_SRVADDR[NRF_ADDR_LEN] = { 0x05, 0x04, 0x03, 0x02, 0x01 };


#endif /* NRF_SECRET_H_ */
