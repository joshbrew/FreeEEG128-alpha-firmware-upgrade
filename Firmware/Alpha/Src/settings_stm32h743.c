#include "stm32h7xx_hal.h"
#include "settings.h"
#include <string.h>
extern int settings_probe_word(uint32_t address);
extern const uint8_t settings_probe_load, settings_probe_failed;

static int supported(void)
{ return *(const uint16_t *)FLASHSIZE_BASE == 2048 && !(FLASH->OPTSR_CUR & FLASH_OPTSR_SWAP_BANK_OPT); }

/* Recover only a precise ECC fault at our explicit probe load. All other faults halt. */
void settings_bus_fault(uint32_t *frame)
{
    if ((frame[6] & ~1u) == (uint32_t)&settings_probe_load &&
        ((FLASH->SR1 | FLASH->SR2) & FLASH_SR_DBECCERR)) {
        FLASH->CCR1 = FLASH_CCR_CLR_DBECCERR | FLASH_CCR_CLR_SNECCERR;
        FLASH->CCR2 = FLASH_CCR_CLR_DBECCERR | FLASH_CCR_CLR_SNECCERR;
        SCB->CFSR = SCB_CFSR_PRECISERR_Msk | SCB_CFSR_BFARVALID_Msk;
        frame[6] = (uint32_t)&settings_probe_failed;
        __DSB(); __ISB(); return;
    }
    for (;;) { __NOP(); }
}
int settings_read(uint32_t offset, uint32_t record[8])
{
    unsigned i;
    if (!supported() || (offset & 31) || offset >= SETTINGS_BYTES) return 0;
    for (i = 0; i < 32; i += 4) if (!settings_probe_word(SETTINGS_BASE + offset + i)) return 0;
    memcpy(record, (const void *)(SETTINGS_BASE + offset), 32);
    return 1;
}
int settings_program(uint32_t offset, const uint32_t record[8])
{
    uint32_t aligned[8] __attribute__((aligned(32)));
    HAL_StatusTypeDef result;
    if (!supported() || (offset & 31) || offset >= SETTINGS_BYTES) return 0;
    memcpy(aligned, record, 32);
    if (HAL_FLASH_Unlock() != HAL_OK) return 0;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK2);
    result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, SETTINGS_BASE + offset, (uint32_t)aligned);
    HAL_FLASH_Lock(); return result == HAL_OK;
}
int settings_erase(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error;
    HAL_StatusTypeDef result;
    if (!supported()) return 0;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS; erase.Banks = FLASH_BANK_2;
    erase.Sector = 6; erase.NbSectors = 1; erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASH_Unlock() != HAL_OK) return 0;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK2);
    result = HAL_FLASHEx_Erase(&erase, &error);
    HAL_FLASH_Lock(); return result == HAL_OK;
}
