#include "stm32h7xx_hal.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_msc.h"
#include "uf2_update.h"
#include "boot_request.h"

extern USBD_StorageTypeDef uf2_storage;
extern PCD_HandleTypeDef hpcd_USB_OTG_HS;
extern uint32_t uf2_last_write;
USBD_HandleTypeDef hUsbDeviceHS;
void SystemClock_Config(void); /* Reused from the base 80 MHz firmware at build time. */

void HAL_MspInit(void) { __HAL_RCC_SYSCFG_CLK_ENABLE(); }
void SysTick_Handler(void) { HAL_IncTick(); }
void HardFault_Handler(void) { for (;;) { __NOP(); } }
void Error_Handler(void) { for (;;) { __NOP(); } }

static void jump_application(void)
{
    uint32_t sp = *(const uint32_t *)UF2_APP_BASE;
    uint32_t entry = *(const uint32_t *)(UF2_APP_BASE + 4);
    unsigned i;
    HAL_RCC_DeInit();
    __disable_irq();
    SysTick->CTRL = 0; SysTick->LOAD = 0; SysTick->VAL = 0;
    for (i = 0; i < 8; ++i) { NVIC->ICER[i] = 0xffffffffu; NVIC->ICPR[i] = 0xffffffffu; }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    SCB->VTOR = UF2_APP_BASE;
    __DSB(); __ISB();
    /* No C stack access is permitted after replacing MSP. Reset_Handler starts in thread mode. */
    __asm volatile ("movs r2, #0\nmsr control, r2\nisb\nmsr msp, %0\ncpsie i\nbx %1" :: "r"(sp), "r"(entry) : "r2", "memory");
    __builtin_unreachable();
}

static int want_loader(uint32_t reset_flags)
{
    int requested, twice;
    uf2_backup_access();
    requested = RTC->BKP31R == UF2_REQUEST_MAGIC && RTC->BKP30R == ~UF2_REQUEST_MAGIC;
    RTC->BKP31R = RTC->BKP30R = 0;
    twice = !(reset_flags & (RCC_RSR_PORRSTF | RCC_RSR_BORRSTF)) &&
        RTC->BKP29R == UF2_DOUBLE_MAGIC && RTC->BKP28R == ~UF2_DOUBLE_MAGIC;
    RTC->BKP29R = RTC->BKP28R = 0;
    if (requested || twice) return 1;
    RTC->BKP28R = ~UF2_DOUBLE_MAGIC; RTC->BKP29R = UF2_DOUBLE_MAGIC;
    HAL_Delay(600); /* Two reset presses within this window enter the drive. */
    RTC->BKP29R = RTC->BKP28R = 0;
    return 0;
}

int main(void)
{
    uint32_t reset_flags = RCC->RSR;
    int recovery;
    GPIO_InitTypeDef gpio = {0};
    HAL_Init();
    SystemClock_Config();
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
    /* Forward the original reset flags before clearing them in the resident loader. */
    uf2_backup_access(); RTC->BKP26R = reset_flags; RTC->BKP27R = UF2_RESET_FLAGS_MAGIC;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    recovery = want_loader(reset_flags);
    /* Installation occurs before USB initialization. Loader sector 0 is never erased. */
    if (uf2_install_pending() < 0) recovery = 1;
    if (!recovery && uf2_application_valid()) jump_application();
    uf2_update_init();
    /* Match the application's isolator full-speed-select output. */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_0; gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(GPIOC, &gpio);
    HAL_PWREx_EnableUSBVoltageDetector();
    __HAL_RCC_CRS_CLK_ENABLE();
    RCC_CRSInitTypeDef crs = {0};
    crs.Prescaler = RCC_CRS_SYNC_DIV1; crs.Source = RCC_CRS_SYNC_SOURCE_USB1; /* OTG_HS */
    crs.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
    crs.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
    crs.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
    crs.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;
    HAL_RCCEx_CRSConfig(&crs);
    if (USBD_Init(&hUsbDeviceHS, &HS_Desc, 1) != USBD_OK ||
        USBD_RegisterClass(&hUsbDeviceHS, &USBD_MSC) != USBD_OK ||
        USBD_MSC_RegisterStorage(&hUsbDeviceHS, &uf2_storage) != USBD_OK) Error_Handler();
    /* Run the USB stack in main so flash erases never execute in a USB ISR.
       Endpoints NAK while the flash operation completes; SysTick stays enabled. */
    HAL_NVIC_DisableIRQ(OTG_HS_IRQn);
    HAL_NVIC_DisableIRQ(OTG_HS_EP1_OUT_IRQn);
    HAL_NVIC_DisableIRQ(OTG_HS_EP1_IN_IRQn);
    if (USBD_Start(&hUsbDeviceHS) != USBD_OK) Error_Handler();
    for (;;) {
        HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS);
        if (uf2_update_ready() && HAL_GetTick() - uf2_last_write >= 1500) {
            USBD_Stop(&hUsbDeviceHS);
            HAL_Delay(100);
            NVIC_SystemReset();
        }
    }
}
