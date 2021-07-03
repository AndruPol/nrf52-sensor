#ifndef _DHT_H_
#define _DHT_H_

#include <stdint.h>

#define DHT_PWRUP		800	// DHT sensor power up, mS

typedef enum {
	DHT_OK,
	DHT_IRQ_TIMEOUT,
	DHT_TIMEOUT,
	DHT_RCV_TIMEOUT,
	DHT_DECODE_ERROR,
	DHT_CHECKSUM_ERROR,
} dht_error_t;


#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------*/
void dht_init(void);
void dht_stop(void);
/* temperature in 1/10 deg C, humidity in 1/10 % */
dht_error_t dht_read(int16_t *temperature, uint16_t *humidity);

#ifdef __cplusplus
}
#endif

#endif
