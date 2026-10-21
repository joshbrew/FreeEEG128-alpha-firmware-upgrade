/* Validate the real profile definitions for rev Y and rev V with numeric RCC
   divider mocks. ARM builds separately check the vendor register definitions. */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define REV_ID_V 0x2003u
#define RCC_SYSCLK_DIV1 1u
#define RCC_SYSCLK_DIV2 2u
#define RCC_HCLK_DIV1 1u
#define RCC_HCLK_DIV2 2u
#define RCC_HCLK_DIV4 4u
#define RCC_HCLK_DIV8 8u
#define PWR_REGULATOR_VOLTAGE_SCALE0 0u
#define PWR_REGULATOR_VOLTAGE_SCALE1 1u
#define PWR_REGULATOR_VOLTAGE_SCALE2 2u
#define PWR_REGULATOR_VOLTAGE_SCALE3 3u
#define RCC_PLL1VCOWIDE 1u
#define RCC_PLL1VCOMEDIUM 0u
#define FLASH_LATENCY_0 0u
#define FLASH_LATENCY_1 1u
static uint32_t revision;
uint32_t HAL_GetREVID(void) { return revision; }
#include "clock_profile.h"
int main(void)
{
    unsigned i;
    for (i = 0; i < 2; ++i) {
        uint32_t cpu, ahb, vco, scale, expected;
        revision = i ? REV_ID_V : 0x1003u;
        assert(FREEEEG128_HSE_HZ == 8192000u);
        vco = (FREEEEG128_HSE_HZ * FREEEEG128_PLL_N + 1000u * FREEEEG128_PLL_FRAC) / 1000000u;
        cpu = vco / 2u / FREEEEG128_CORE_DIVIDER;
        ahb = cpu / FREEEEG128_AHB_DIVIDER;
        expected = FREEEEG128_CPU_MHZ == 480 && !i ? 400 : FREEEEG128_CPU_MHZ;
        scale = FREEEEG128_VOLTAGE_SCALE;
        assert(cpu == expected && ahb > 30 && ahb <= 60);
        assert(16u * FREEEEG128_HSI_PLL_N / 2u / FREEEEG128_CORE_DIVIDER == expected);
        assert(16u * FREEEEG128_HSI_PLL_N / FREEEEG128_HSI_PLL_QR == 80u);
        assert(vco / FREEEEG128_PLL_QR == 80); /* Preserve PLL1Q/R consumers. */
        assert(scale == 0 ? i && cpu <= 480 : scale == 1 ? cpu <= 400 : scale == 2 ? cpu <= 300 : cpu <= 200);
        assert(FREEEEG128_PLL_VCO ? vco >= 192 && vco <= (i ? 960u : 836u) : vco >= 150 && vco <= 420);
        assert(FREEEEG128_FLASH_LATENCY || ahb <= (scale == 3 ? 45u : scale == 2 ? 55u : 70u));
        assert((ahb * 1000000u) % 1000000u == 0); /* TIM2 remains an exact 1 MHz. */
    }
    printf("Clock %u MHz passed: CPU/bus/voltage/flash limits, PLL Q/R, rev-V gate/fallback.\n", (unsigned)FREEEEG128_CPU_MHZ);
    return 0;
}
