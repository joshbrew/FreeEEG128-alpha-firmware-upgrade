#ifndef FREEEEG128_CLOCK_START_H
#define FREEEEG128_CLOCK_START_H
#include "stm32h7xx_hal.h"
/* Application: external 8.192 MHz first, then internal HSI on failure.
   Loader: internal HSI only so USB recovery does not require the board clock. */
int clock_start_pll(void);
/* PLL2 shares PLL1's input selector. Keep its SPI clock bounded on both paths. */
void clock_configure_pll2(RCC_PeriphCLKInitTypeDef *config);
#endif
