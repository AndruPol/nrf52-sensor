/*
    Copyright (C) 2016 Stephane D'Alu

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef _MCUCONF_H_
#define _MCUCONF_H_

/*
 * Board setting
 */

/*
 * HAL driver system settings.
 */
#define NRF5_SERIAL_USE_UART0          TRUE
#define NRF5_SERIAL_USE_HWFLOWCTRL	   FALSE
#define NRF5_SERIAL_UART0_PRIORITY     3

#define NRF5_RNG_USE_RNG0 		       FALSE

#define NRF5_GPT_USE_TIMER0 		   FALSE
#define NRF5_GPT_USE_TIMER1 		   FALSE
#define NRF5_GPT_USE_TIMER2 		   FALSE
#define NRF5_GPT_USE_TIMER3 		   FALSE
#define NRF5_GPT_USE_TIMER4 		   FALSE

#define NRF5_QEI_USE_QDEC0             FALSE
#define NRF5_QEI_USE_LED               FALSE

#define WDG_USE_TIMEOUT_CALLBACK       TRUE

#define NRF5_PWM_USE_PWM0              FALSE
#define NRF5_PWM_USE_PWM1              FALSE
#define NRF5_PWM_USE_PWM2              FALSE
#define NRF5_PWM_PWM0_PRIORITY         6
#define NRF5_PWM_PWM1_PRIORITY         6
#define NRF5_PWM_PWM2_PRIORITY         6

#define NRF5_ICU_USE_TIMER0            FALSE
#define NRF5_ICU_USE_TIMER1            FALSE
#define NRF5_ICU_USE_TIMER2            TRUE		//1
#define NRF5_ICU_USE_TIMER3            FALSE
#define NRF5_ICU_USE_TIMER4            FALSE

#define NRF5_ICU_GPIOTE_IRQ_PRIORITY   2
#define NRF5_ICU_TIMER0_IRQ_PRIORITY   4
#define NRF5_ICU_TIMER1_IRQ_PRIORITY   4
#define NRF5_ICU_TIMER2_IRQ_PRIORITY   3
#define NRF5_ICU_TIMER3_IRQ_PRIORITY   4
#define NRF5_ICU_TIMER4_IRQ_PRIORITY   4

#define NRF5_I2C_USE_I2C0              TRUE		//2
#define NRF5_I2C_USE_I2C1              FALSE
#define NRF5_I2C_I2C0_IRQ_PRIORITY     3
#define NRF5_I2C_I2C1_IRQ_PRIORITY     3

#define NRF5_ST_USE_RTC0               FALSE
#define NRF5_ST_USE_RTC1               TRUE
#define NRF5_ST_USE_TIMER0             FALSE

#define NRF52832_QFAA                  TRUE

#endif /* _MCUCONF_H_ */
