#ifndef FREEEEG128_UF2_DISK_H
#define FREEEEG128_UF2_DISK_H
#include <stdint.h>
#define UF2_DISK_SECTORS 4096u
int uf2_disk_read(uint32_t lba, uint8_t output[512]);
int uf2_disk_write(uint32_t lba, const uint8_t data[512]);
#endif
