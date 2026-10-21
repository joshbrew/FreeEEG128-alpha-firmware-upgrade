#include "usbd_msc.h"
#include "uf2_disk.h"
#include "uf2_update.h"
#include "stm32h7xx_hal.h"

uint32_t uf2_last_write;
static int8_t init(uint8_t lun) { (void)lun; return 0; }
static int8_t capacity(uint8_t lun, uint32_t *count, uint16_t *size)
{ (void)lun; *count = UF2_DISK_SECTORS; *size = 512; return 0; }
static int8_t ready(uint8_t lun) { (void)lun; return 0; }
static int8_t protected(uint8_t lun) { (void)lun; return 0; }
static int8_t read_blocks(uint8_t lun, uint8_t *buffer, uint32_t lba, uint16_t count)
{
    unsigned i; (void)lun;
    if (lba >= UF2_DISK_SECTORS || count > UF2_DISK_SECTORS - lba) return -1;
    for (i = 0; i < count; ++i) if (uf2_disk_read(lba + i, buffer + i * 512)) return -1;
    return 0;
}
static int8_t write_blocks(uint8_t lun, uint8_t *buffer, uint32_t lba, uint16_t count)
{
    unsigned i; (void)lun;
    if (lba >= UF2_DISK_SECTORS || count > UF2_DISK_SECTORS - lba) return -1;
    for (i = 0; i < count; ++i) if (uf2_disk_write(lba + i, buffer + i * 512)) return -1;
    uf2_last_write = HAL_GetTick();
    return 0;
}
static int8_t max_lun(void) { return 0; }
static int8_t inquiry[36] = {
    0x00, 0x80, 0x02, 0x02, 31, 0, 0, 0,
    'F','r','e','e','E','E','G',' ',
    'F','r','e','e','E','E','G','3','2',' ','U','F','2',' ',' ',' ',
    '0','0','0','1'
};
USBD_StorageTypeDef uf2_storage = { init, capacity, ready, protected, read_blocks, write_blocks, max_lun, inquiry };
