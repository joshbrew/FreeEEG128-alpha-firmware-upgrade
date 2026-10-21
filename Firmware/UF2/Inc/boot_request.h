#ifndef FREEEEG128_BOOT_REQUEST_H
#define FREEEEG128_BOOT_REQUEST_H
#include "stm32h7xx_hal.h"
#define UF2_REQUEST_MAGIC 0x55463221u
#define UF2_DOUBLE_MAGIC  0x44525354u
#define UF2_RESET_FLAGS_MAGIC 0x52535433u
static inline void uf2_backup_access(void)
{
    __HAL_RCC_RTC_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}
static inline void uf2_request_bootloader(void)
{
    uf2_backup_access();
    RTC->BKP30R = ~UF2_REQUEST_MAGIC;
    RTC->BKP31R = UF2_REQUEST_MAGIC;
    __DSB();
}
#endif
