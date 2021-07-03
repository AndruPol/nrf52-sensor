/*
 * nrf52_flash.h
 *
 *  Created on: 02.01.2021
 *      Author: andru
 */

#ifndef NRF52_FLASH_H_
#define NRF52_FLASH_H_

typedef enum {
	PAGE_UNDEF,
	PAGE_ERASED,
	PAGE_RECEIVED,
	PAGE_VALID,
} page_state_t;

#define NUMPAGES	2

#include "main.h"

#define DATALEN		CFGLEN
#define RECORDLEN 	((sizeof(flash_record_t) % 4 > 0) ? (sizeof(flash_record_t) / 4 + 1) * 4 : sizeof(flash_record_t))

typedef struct _flash_page_t flash_page_t;
struct _flash_page_t {
  uint32_t recordid;
  uint16_t pageno;
  uint16_t last_record;
  page_state_t state;
};

typedef struct _flash_record_t flash_record_t;
struct _flash_record_t {
  uint32_t magic;
  uint32_t id;
  uint8_t data[DATALEN];
  uint8_t crc;
};

typedef struct _flash_info_t flash_info_t;
struct _flash_info_t {
  uint16_t active_page;
  uint16_t records_on_page;
  uint32_t last_id;
  flash_page_t pages[NUMPAGES];
};

bool initFlash(void);
bool eraseFlash(void);
bool readFlash(uint8_t *data);
bool writeFlash(uint8_t *data);

#endif /* NRF52_FLASH_H_ */
