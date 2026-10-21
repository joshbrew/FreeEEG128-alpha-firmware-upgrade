#include "clock_start.h"
#include "clock_profile.h"
#include "runtime.h"

static void internal_pll(RCC_OscInitTypeDef *osc)
{
    osc->PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc->PLL.PLLM = 4u; /* Nominal 64 MHz / 4 = 16 MHz input. */
    osc->PLL.PLLN = FREEEEG128_HSI_PLL_N;
    osc->PLL.PLLQ = osc->PLL.PLLR = FREEEEG128_HSI_PLL_QR;
    osc->PLL.PLLFRACN = 0;
}

int clock_start_pll(void)
{
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_DIV1;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.HSI48State = RCC_HSI48_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLP = 2u;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    osc.PLL.PLLVCOSEL = FREEEEG128_PLL_VCO;
#ifndef FREEEEG128_UF2_BOOTLOADER
    /* Unlike FreeEEG32's passive 16 MHz crystal, this PCB supplies an active
       8.192 MHz oscillator to OSC_IN. Preserve bypass mode and fractional PLL. */
    osc.OscillatorType |= RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_BYPASS;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 1u;
    osc.PLL.PLLN = FREEEEG128_PLL_N;
    osc.PLL.PLLQ = osc.PLL.PLLR = FREEEEG128_PLL_QR;
    osc.PLL.PLLFRACN = FREEEEG128_PLL_FRAC;
    if (HAL_RCC_OscConfig(&osc) == HAL_OK) return 1;
    /* HSE startup failed. USB still has HSI48+CRS and USART1 retains HSI.
       This is a recovery path; it cannot replace a missing ADC oscillator. */
    osc.HSEState = RCC_HSE_OFF;
#endif
    internal_pll(&osc);
    return HAL_RCC_OscConfig(&osc) == HAL_OK;
}

void clock_configure_pll2(RCC_PeriphCLKInitTypeDef *config)
{
    int external = (RCC->PLLCKSELR & RCC_PLLCKSELR_PLLSRC) == RCC_PLLCKSELR_PLLSRC_HSE;
    config->PLL2.PLL2M = external ? 1u : 4u;
    config->PLL2.PLL2N = external ? 40u : 20u;
    config->PLL2.PLL2P = config->PLL2.PLL2Q = config->PLL2.PLL2R = 10u;
    config->PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
    config->PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    config->PLL2.PLL2FRACN = 0;
    /* 8.192 MHz SPI normally; nominal 8 MHz on HSI. Never reuse M=1 on
       HSI=64 MHz: that would overclock PLL2 and all four ADC SPI buses. */
}

const char *runtime_clock_source(void)
{
    switch (RCC->PLLCKSELR & RCC_PLLCKSELR_PLLSRC) {
    case RCC_PLLCKSELR_PLLSRC_HSE: return "HSE";
    case RCC_PLLCKSELR_PLLSRC_CSI: return "CSI";
    default: return "HSI";
    }
}
