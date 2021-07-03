/*
 *  DHT21/DHT22 driver
 */

#include "dht.h"

#include "ch.h"
#include "hal.h"

#define DHT_ICU				ICUD3
#define DHT_GPIO0			DHT0		/* GPIO0 and GPIO1 need to be connected to DHT OUT */
#define DHT_GPIO1			DHT1		/* GPIO0 and GPIO1 need to be connected to DHT OUT */
#define DHT_GPIOTE_CH0		0
#define DHT_GPIOTE_CH1		1
#define DHT_PPI_CH0			0
#define DHT_PPI_CH1			1

#define DHT_START_PULSE_MS 	2
#define DHT_PKT_TIMEOUT_MS 	10
#define DHT_PKT_SIZE 		5
#define DHT_BIT_TIMEOUT_MS  5
#define DHT_PERIOD_HIGH		80	// 80 uS
#define DHT_PERIOD_LOW		100	// 80 uS - long startup time?
#define DHT_BIT_START		60	// 50 uS
#define DHT_BIT0			26	// 26-28 uS
#define DHT_BIT1			70	// 70 uS

#define ERROR_DIV 4
#define PERIOD_OK(x, l, h) \
	((x)->low >= ((l) - (l) / ERROR_DIV) && \
	(x)->low < ((l) + (l) / ERROR_DIV) && \
	((x)->period - (x)->low) >= ((h) - (h) / ERROR_DIV) && \
	((x)->period - (x)->low) < ((h) + (h) / ERROR_DIV))
/*-----------------------------------------------------------------------------*/

static thread_t *DHTThread_p = NULL;
static THD_WORKING_AREA(waDHTThread, 192);
#define DHT_PRIO			(NORMALPRIO + 1)

typedef struct _icu_capture_t icu_capture_t;
struct _icu_capture_t {
	uint16_t period;
	uint16_t low;
};

typedef struct _dht_read_t dht_read_t;
struct _dht_read_t {
	dht_error_t error; 			/* out */
	uint8_t data[DHT_PKT_SIZE]; /* out */
};

static binary_semaphore_t icusem, cb_sem;
static volatile icu_capture_t icu_data;
static dht_read_t rd;

/* ICU width interrupt handler */
static void icu_width_cb(ICUDriver *icup) {
  icu_data.low = icuGetWidthX(icup);
}

/* ICU period interrupt handler */
static void icu_period_cb(ICUDriver *icup) {
  icu_data.period = icuGetPeriodX(icup);
  chSysLockFromISR();
  chBSemSignalI(&cb_sem);
  chSysUnlockFromISR();
}

/* ICU overflow interrupt handler */
static void icu_overflow_cb(ICUDriver *icup) {
  (void)icup;
  icu_data.low = 0;
  chSysLockFromISR();
  chBSemSignalI(&cb_sem);
  chSysUnlockFromISR();
}

/* ICU config */
static const ICUConfig icucfg = {
  .frequency = ICU_FREQUENCY_1MHZ,
  .width_cb = icu_width_cb,
  .period_cb = icu_period_cb,
  .overflow_cb = icu_overflow_cb,
  .iccfgp = {
	{
      .ioline = { DHT_GPIO0, DHT_GPIO1 },
	  .mode = ICU_INPUT_ACTIVE_LOW,
	  .gpiote_channel = { DHT_GPIOTE_CH0, DHT_GPIOTE_CH1 },
	  .ppi_channel = { DHT_PPI_CH0, DHT_PPI_CH1 },
    },
  },
};

/*
 * DHT read thread.
 */
static THD_FUNCTION(DHTThread, arg) {
  (void)arg;
  chRegSetThreadName("DHTThd");

  chBSemObjectInit(&cb_sem, TRUE);

  while (!chThdShouldTerminateX()) {
	/* wait for read request */
	dht_read_t *req;
	thread_t *tp;

	tp = chMsgWait();
	req = (dht_read_t *) chMsgGet(tp);
	chMsgRelease(tp, (msg_t) req);

	if (!req) continue;

	icu_data.low = icu_data.period = 0;

	// set DHT pin low on 2ms
	palSetLineMode(IOPORT1_DHT0, PAL_MODE_OUTPUT_OPENDRAIN);
	palSetLineMode(IOPORT1_DHT1, PAL_MODE_OUTPUT_OPENDRAIN);
	palClearLine(IOPORT1_DHT0);
	palClearLine(IOPORT1_DHT1);

	chThdSleepMilliseconds(DHT_START_PULSE_MS);

	palSetLineMode(IOPORT1_DHT0, PAL_MODE_INPUT);
	palSetLineMode(IOPORT1_DHT1, PAL_MODE_INPUT);

	icuStartCapture(&DHT_ICU);
	icuEnableNotifications(&DHT_ICU);

	// IRQ timeout or receive timeout
	if(chBSemWaitTimeout(&cb_sem, TIME_MS2I(DHT_BIT_TIMEOUT_MS)) == MSG_TIMEOUT) {
		req->error = DHT_IRQ_TIMEOUT;
		goto reply;
	}

	if(!icu_data.period) {
		req->error = DHT_TIMEOUT;
		goto reply;
	}

	/* start sequence received */
	if(!PERIOD_OK(&icu_data, DHT_PERIOD_LOW, DHT_PERIOD_HIGH)) {
		req->error = DHT_DECODE_ERROR;
		goto reply;
	}

	for (uint8_t i=0; i < DHT_PKT_SIZE; i++) {
		uint8_t mask = 0x80;
		uint8_t byte = 0;
		while(mask) {
			if (chBSemWaitTimeout(&cb_sem, TIME_MS2I(DHT_BIT_TIMEOUT_MS)) == MSG_TIMEOUT) {
				req->error = DHT_IRQ_TIMEOUT;
				goto reply;
			}
			/* next bit received */
			if (PERIOD_OK(&icu_data, DHT_BIT_START, DHT_BIT1)) {
				byte |= mask; /* 1 */
			} else if(!PERIOD_OK(&icu_data, DHT_BIT_START, DHT_BIT0)) {
				req->error = DHT_DECODE_ERROR;
				goto reply;
			}
			mask >>= 1;
		}
		req->data[i] = byte;
	}

	req->error = DHT_OK;

reply:
	icuDisableNotifications(&DHT_ICU);
	icuStopCapture(&DHT_ICU);
	chBSemSignal(&icusem);
  } //while

  chThdExit((msg_t) 0);
}

dht_error_t dht_read(int16_t *temperature, uint16_t *humidity) {
	dht_read_t *rd_p = &rd;

	chBSemWait(&icusem); /* to be sure */

	chMsgSend(DHTThread_p, (msg_t) rd_p);

	/* wait for reply */
	if(chBSemWaitTimeout(&icusem, TIME_MS2I(DHT_PKT_TIMEOUT_MS)) == MSG_TIMEOUT) {
		chBSemReset(&icusem, FALSE);
		return DHT_RCV_TIMEOUT;
	}
	chBSemReset(&icusem, FALSE);

	if(rd.error != DHT_OK) {
		return rd.error;
	}

	/* compute checksum */
	uint8_t checksum = 0;
	for (uint8_t i = 0; i < DHT_PKT_SIZE-1; i++)
		checksum += rd.data[i];

	if (checksum != rd.data[DHT_PKT_SIZE-1]) {
		return DHT_CHECKSUM_ERROR;
	}

	if (rd.data[1] == 0 && rd.data[3] == 0)
	{	// DHT11
		*humidity = ((uint16_t) rd.data[0]);
		*temperature = ((int16_t)rd.data[2]);
	}
	else
	{	// DHT21/22
		/* read 16 bit humidity value */
		*humidity = ((uint16_t) rd.data[0] << 8) | rd.data[1];

		/* read 16 bit temperature value */
		int val = ((uint16_t) rd.data[2] << 8) | rd.data[3];

		*temperature = val & 0x8000 ? -(val & ~0x8000) : val;
	}
	return DHT_OK;
}

void dht_init(void) {
	chBSemObjectInit(&icusem, FALSE);
	palSetLineMode(IOPORT1_DHT0, PAL_MODE_INPUT);
	palSetLineMode(IOPORT1_DHT1, PAL_MODE_INPUT);
	icuStart(&DHT_ICU, &icucfg);

	if (!DHTThread_p)
		DHTThread_p = chThdCreateStatic(waDHTThread, sizeof(waDHTThread), DHT_PRIO, DHTThread, NULL);
}

void dht_stop(void) {
	icuStop(&DHT_ICU);
	palSetLineMode(IOPORT1_DHT0, PAL_MODE_UNCONNECTED);
	palSetLineMode(IOPORT1_DHT1, PAL_MODE_UNCONNECTED);

	chThdTerminate(DHTThread_p);
	chMsgSend(DHTThread_p, (msg_t) 0);
	chThdWait(DHTThread_p);
}
