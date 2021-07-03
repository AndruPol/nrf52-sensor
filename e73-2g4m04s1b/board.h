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

#ifndef _BOARD_H_
#define _BOARD_H_

/* Board identifier. */
#define BOARD_NRF52_EBYTE_E73
#define BOARD_NAME             "nRF52 EBYTE E73-2G4M04S1B"

/* Board oscillators-related settings. */
#define NRF5_XTAL_VALUE        32000000
#define NRF5_HFCLK_SOURCE      NRF5_HFCLK_HFXO
#define NRF5_LFCLK_SOURCE      NRF5_LFCLK_XTAL

#define NRF5_HFCLK_HFINT       0
#define NRF5_HFCLK_HFXO        1

#define NRF5_LFCLK_RC          0
#define NRF5_LFCLK_XTAL        1
#define NRF5_LFCLK_SYNTH       2

/*
 * GPIO pins. 
 */
/* Defined by board */

/* Our definitions */
#define UART_RTS        5U
#define UART_TX         6U
#define UART_CTS        7U
#define UART_RX         8U

#define I2C_SDA        16U
#define I2C_SCL        17U
#define DHT0           19U
#define DHT1           20U

#define LED            25U
#define PWR             6U

/* Analog input */

/*
 * IO pins assignments.
 */
/* Defined by board */

/* Our definitions */
#define IOPORT1_RESET          21U

#define IOPORT1_I2C_SDA        16U
#define IOPORT1_I2C_SCL        17U
#define IOPORT1_DHT0           19U
#define IOPORT1_DHT1           20U

#define IOPORT1_LED            25U
#define IOPORT1_PWR             6U

/* Analog inpupt */

/*
 * IO lines assignments.
 */
/* Board defined */

/* Our definitions */
#define LINE_RESET     PAL_LINE(IOPORT1, IOPORT1_RESET)

#define LINE_DHT0      PAL_LINE(IOPORT1, IOPORT1_DHT0)
#define LINE_DHT1      PAL_LINE(IOPORT1, IOPORT1_DHT1)
#define LINE_I2C_SCL   PAL_LINE(IOPORT1, IOPORT1_I2C_SCL)
#define LINE_I2C_SDA   PAL_LINE(IOPORT1, IOPORT1_I2C_SDA)

#define LINE_LED       PAL_LINE(IOPORT1, IOPORT1_LED)
#define LINE_PWR       PAL_LINE(IOPORT1, IOPORT1_PWR)

/* Analog line */

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif
  void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif /* _FROM_ASM_ */

#endif /* _BOARD_H_ */
