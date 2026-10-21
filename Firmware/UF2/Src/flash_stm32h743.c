#include "stm32h7xx_hal.h"
#include "uf2_update.h"
#include <string.h>

extern int uf2_probe_word(uint32_t address);
extern const uint8_t uf2_probe_load, uf2_probe_failed;

static int hardware_supported(void)
{
    return *(const uint16_t *)FLASHSIZE_BASE == 2048 && !(FLASH->OPTSR_CUR & FLASH_OPTSR_SWAP_BANK_OPT);
}

/* ECC double errors after interrupted flash writes raise a precise BusFault on H7.
   Only recover our single explicit probe instruction, never an unrelated fault. */
void uf2_bus_fault(uint32_t *frame)
{
    if ((frame[6] & ~1u) == (uint32_t)&uf2_probe_load &&
        ((FLASH->SR1 | FLASH->SR2) & FLASH_SR_DBECCERR)) {
        FLASH->CCR1 = FLASH_CCR_CLR_DBECCERR | FLASH_CCR_CLR_SNECCERR;
        FLASH->CCR2 = FLASH_CCR_CLR_DBECCERR | FLASH_CCR_CLR_SNECCERR;
        SCB->CFSR = SCB_CFSR_PRECISERR_Msk | SCB_CFSR_BFARVALID_Msk;
        frame[6] = (uint32_t)&uf2_probe_failed;
        __DSB(); __ISB();
        return;
    }
    for (;;) { __NOP(); }
}

const uint8_t *uf2_flash_read(uint32_t address, uint32_t length)
{
    uint32_t offset;
    if (address < UF2_APP_BASE || address >= 0x08200000u || length > 0x08200000u - address ||
        (address & 3) || (length & 3)) return NULL;
    /* Keep caches disabled in the loader: no stale lines or speculative validation. */
    for (offset = 0; offset < length; offset += 4)
        if (!uf2_probe_word(address + offset)) return NULL;
    return (const uint8_t *)address;
}

int uf2_flash_erase(unsigned bank, unsigned sector)
{
    FLASH_EraseInitTypeDef config = {0};
    uint32_t error = 0;
    HAL_StatusTypeDef result;
    if (!hardware_supported() || (bank != 1 && bank != 2) || sector > 7 ||
        (bank == 1 && sector == 0) || (bank == 2 && sector == 6)) return 0;
    config.TypeErase = FLASH_TYPEERASE_SECTORS;
    config.Banks = bank == 1 ? FLASH_BANK_1 : FLASH_BANK_2;
    config.Sector = sector; config.NbSectors = 1; config.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASH_Unlock() != HAL_OK) return 0;
    result = HAL_FLASHEx_Erase(&config, &error);
    HAL_FLASH_Lock();
    return result == HAL_OK;
}

int uf2_flash_program(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t offset;
    uint32_t buffer[8] __attribute__((aligned(32)));
    int allowed = (address >= UF2_APP_BASE && address < UF2_APP_LIMIT && length <= UF2_APP_LIMIT - address) ||
        (address == UF2_APP_META && length == 32) ||
        (address >= UF2_STAGE_BASE && address < UF2_STAGE_BASE + UF2_APP_MAX &&
         length <= UF2_STAGE_BASE + UF2_APP_MAX - address) ||
        (address == UF2_STAGE_META && length == 32);
    if (!hardware_supported() || !allowed || (address & 31) || !length || (length & 31)) return 0;
    if (HAL_FLASH_Unlock() != HAL_OK) return 0;
    for (offset = 0; offset < length; offset += 32) {
        /* H743 requires one 256-bit flash-word write. Source may be unaligned USB data. */
        memcpy(buffer, data + offset, sizeof(buffer));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address + offset, (uint32_t)buffer) != HAL_OK) {
            HAL_FLASH_Lock(); return 0;
        }
    }
    HAL_FLASH_Lock();
    return 1;
}
