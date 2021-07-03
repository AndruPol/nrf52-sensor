/*
 * nrf52_pof.c
 *
 *  Created on: 03.01.2021
 *      Author: andru
 */

#include "ch.h"
#include "hal.h"

#include "nrf52_pof.h"

bool pof_warning = false;	// export POF flag

/**
 * @brief   POF common service routine.
 *
 */
static void serve_power_irq(void) {
  NRF_POWER_Type *p = NRF_POWER;

  if (p->EVENTS_POFWARN) {

	  pof_warning = true;

	  p->EVENTS_POFWARN = 0;
#if CORTEX_MODEL >= 4
	  (void)p->EVENTS_POFWARN;
#endif
  }
}

/**
 * @brief   POWER_CLOCK IRQ handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(Vector40) {

  OSAL_IRQ_PROLOGUE();

  serve_power_irq();

  OSAL_IRQ_EPILOGUE();
}

void pof_init(nrf52_pof_voltage_t v) {
  NRF_POWER_Type *p = NRF_POWER;

  pof_warning = false;

  // clear POF warning events
  p->EVENTS_POFWARN = 0;
#if CORTEX_MODEL >= 4
  (void)p->EVENTS_POFWARN;
#endif

  // POF interrupt enables
  p->INTENSET |= ((POWER_INTENSET_POFWARN_Enabled << POWER_INTENSET_POFWARN_Pos) & POWER_INTENSET_POFWARN_Msk);

  // POF configure
  p->POFCON = ((v << POWER_POFCON_THRESHOLD_Pos) & POWER_POFCON_THRESHOLD_Msk) |
		  	  ((POWER_POFCON_POF_Enabled << POWER_POFCON_POF_Pos) & POWER_POFCON_POF_Msk);
}
