/* Exercise the real clock startup and fallback paths without STM32 hardware. */
#include "clock_start.h"
#include "clock_profile.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Mock_RCC mock_rcc;
static uint32_t revision;
static unsigned calls, fail_hse, fail_hsi;
static RCC_OscInitTypeDef selected;
uint32_t HAL_GetREVID(void) { return revision; }
int HAL_RCC_OscConfig(RCC_OscInitTypeDef *config)
{
    ++calls;
    assert(config->HSIState == RCC_HSI_DIV1 && config->HSI48State == RCC_HSI48_ON);
    assert(config->HSICalibrationValue == RCC_HSICALIBRATION_DEFAULT);
    assert(config->PLL.PLLState == RCC_PLL_ON && config->PLL.PLLP == 2);
    if (config->PLL.PLLSource == RCC_PLLSOURCE_HSE) {
        assert(config->OscillatorType & RCC_OSCILLATORTYPE_HSE);
        assert(config->HSEState == RCC_HSE_BYPASS);
        if (fail_hse) return HAL_ERROR;
    } else {
        assert(config->PLL.PLLSource == RCC_PLLSOURCE_HSI && config->HSEState == RCC_HSE_OFF);
        assert(config->PLL.PLLM == 4 && config->PLL.PLLFRACN == 0);
#ifdef FREEEEG128_UF2_BOOTLOADER
        assert(!(config->OscillatorType & RCC_OSCILLATORTYPE_HSE));
#endif
        if (fail_hsi) return HAL_ERROR;
    }
    selected = *config;
    mock_rcc.PLLCKSELR = config->PLL.PLLSource;
    return HAL_OK;
}

static void frequencies(int external)
{
    RCC_PeriphCLKInitTypeDef periph = {0};
    uint32_t cpu, expected, spi;
    uint64_t input = external ? FREEEEG128_HSE_HZ : 64000000u;
    uint64_t vco = input * (selected.PLL.PLLN * 8192u + selected.PLL.PLLFRACN) / selected.PLL.PLLM / 8192u;
    cpu = (uint32_t)(vco / selected.PLL.PLLP / FREEEEG128_CORE_DIVIDER);
    expected = FREEEEG128_CPU_MHZ == 480 && revision != REV_ID_V ? 400 : FREEEEG128_CPU_MHZ;
    assert(cpu == expected * 1000000u);
    clock_configure_pll2(&periph);
    spi = (uint32_t)(input * periph.PLL2.PLL2N / periph.PLL2.PLL2M / periph.PLL2.PLL2Q / 4u);
    assert(spi == (external ? 8192000u : 8000000u));
    assert(periph.PLL2.PLL2P == 10 && periph.PLL2.PLL2R == 10 && !periph.PLL2.PLL2FRACN);
    assert(!strcmp(runtime_clock_source(), external ? "HSE" : "HSI"));
}

int main(void)
{
    unsigned i;
    for (i = 0; i < 2; ++i) {
        revision = i ? REV_ID_V : 0x1003u;
        calls = fail_hse = fail_hsi = 0;
        assert(clock_start_pll() && calls == 1);
#ifdef FREEEEG128_UF2_BOOTLOADER
        frequencies(0);
        fail_hse = 1; calls = 0;
        assert(clock_start_pll() && calls == 1); /* External-clock failure is irrelevant. */
        frequencies(0);
#else
        frequencies(1);
        fail_hse = 1; calls = 0;
        assert(clock_start_pll() && calls == 2);
        frequencies(0);
#endif
        fail_hsi = 1; calls = 0;
        assert(!clock_start_pll());
    }
    mock_rcc.PLLCKSELR = RCC_PLLCKSELR_PLLSRC_CSI;
    assert(!strcmp(runtime_clock_source(), "CSI"));
    printf("Clock startup %u MHz %s: source selection, oscillator failures, nominal CPU/SPI rates and revision fallback passed.\n",
           (unsigned)FREEEEG128_CPU_MHZ,
#ifdef FREEEEG128_UF2_BOOTLOADER
           "loader"
#else
           "application"
#endif
    );
    return 0;
}
