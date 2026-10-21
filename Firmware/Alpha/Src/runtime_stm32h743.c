#include "stm32h7xx_hal.h"
#include "runtime.h"
#include "main.h"
#include "ads131_board.h"
#ifdef FREEEEG128_UF2_APPLICATION
#include "boot_request.h"
#endif

uint32_t runtime_init(void)
{
    uint32_t flags = RCC->RSR;
#ifdef FREEEEG128_UF2_APPLICATION
    uf2_backup_access();
    if (RTC->BKP27R == UF2_RESET_FLAGS_MAGIC) flags = RTC->BKP26R;
    RTC->BKP27R = 0;
#endif
    __HAL_RCC_CLEAR_RESET_FLAGS();
    /* Instruction caching reduces flash fetch work. Leave D-cache disabled:
       acquisition DMA buffers still require explicit coherency before enabling it. */
    SCB_EnableICache();
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
    SCB->SCR &= ~(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk);
    /* Do not enter Stop or gate acquisition/USB clocks. No periodic wake timer
       is added: existing DRDY, DMA, USB, UART and SysTick IRQs wake the core. */
    __HAL_RCC_DMA1_CLK_SLEEP_ENABLE(); __HAL_RCC_DMA2_CLK_SLEEP_ENABLE();
    __HAL_RCC_SPI2_CLK_SLEEP_ENABLE(); __HAL_RCC_SPI3_CLK_SLEEP_ENABLE();
    __HAL_RCC_SPI4_CLK_SLEEP_ENABLE(); __HAL_RCC_SPI5_CLK_SLEEP_ENABLE();
    /* USB is OTG_HS (USB1) with the embedded FS PHY; the absent ULPI PHY clock must be off in Sleep. */
    __HAL_RCC_USB1_OTG_HS_CLK_SLEEP_ENABLE(); __HAL_RCC_USB1_OTG_HS_ULPI_CLK_SLEEP_DISABLE();
    __HAL_RCC_USART1_CLK_SLEEP_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE(); __HAL_RCC_TIM2_CLK_SLEEP_ENABLE();
    /* All supported profiles use APB1 /2 with TIMPRE=0: timer clock = 2*PCLK1. */
    TIM2->CR1 = 0; TIM2->PSC = (2u * HAL_RCC_GetPCLK1Freq() / 1000000u) - 1u;
    TIM2->ARR = ~0u; TIM2->EGR = TIM_EGR_UG; TIM2->SR = 0; TIM2->CNT = 0;
    TIM2->CR1 = TIM_CR1_CEN;
    return flags;
}
uint32_t runtime_micros(void) { return TIM2->CNT; }
/* FREEEEG128_IDLE_WFI defaults to 0 in runtime.h. */
void runtime_sleep(void)
{
#if FREEEEG128_IDLE_WFI
    __DSB(); __WFI();
#endif
}

/* Register sequence follows ST's H7 IWDG driver. The peripheral/LSI continue
 * during ordinary Sleep. No IRQ feeds it and no option bytes are changed.
 * A system reset ends software activation; the UF2 loader does not start it.
 * https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/master/Src/stm32h7xx_hal_iwdg.c
 */
int runtime_watchdog_start(void)
{
    uint32_t start;
    __HAL_DBGMCU_FREEZE_IWDG1();
    IWDG1->KR = 0xccccu;
    IWDG1->KR = 0x5555u;
    IWDG1->PR = 6u; /* /256; 4096 counts at nominal 32 kHz = 32.768 s. */
    IWDG1->RLR = 0xfffu;
    start = HAL_GetTick();
    while (IWDG1->SR & (IWDG_SR_PVU | IWDG_SR_RVU | IWDG_SR_WVU))
        if ((uint32_t)(HAL_GetTick() - start) >= 1000u) return 0;
    IWDG1->WINR = 0xfffu; /* No early-refresh window. */
    IWDG1->KR = 0xaaaau;
    return 1;
}
void runtime_watchdog_feed(void) { IWDG1->KR = 0xaaaau; }
