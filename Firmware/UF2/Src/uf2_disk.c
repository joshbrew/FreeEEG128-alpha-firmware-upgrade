#include "uf2_disk.h"
#include "uf2_update.h"
#include <string.h>
#include <stdio.h>

/* Synthetic 2 MB FAT12 superfloppy. FAT/root writes are acknowledged but never
   persisted. Detect UF2 sectors by content, independent of file allocation/order. */
#define FAT_SECTORS 12u
#define ROOT_LBA 25u
#define DATA_LBA 29u
static const char info[] = "UF2 Bootloader v1.0\r\nModel: FreeEEG128 alpha STM32H743ZI\r\n"
    "Board-ID: FreeEEG128-alpha-H743ZI-v1\r\nCopy the FreeEEG128 application .uf2 here.\r\n"
    "The drive restarts after a complete verified image.\r\n";
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }
static void entry(uint8_t *p, const char name[11], uint8_t attr, uint16_t cluster, uint32_t size)
{
    memcpy(p, name, 11); p[11] = attr; put16(p + 26, cluster); put32(p + 28, size);
    put16(p + 24, 0x5d41); /* Stable valid FAT date: 2026-10-01. */
}
int uf2_disk_read(uint32_t lba, uint8_t out[512])
{
    if (lba >= UF2_DISK_SECTORS) return -1;
    memset(out, 0, 512);
    if (lba == 0) {
        out[0] = 0xeb; out[1] = 0x3c; out[2] = 0x90; memcpy(out + 3, "UF2 UF2 ", 8);
        put16(out + 11, 512); out[13] = 1; put16(out + 14, 1); out[16] = 2;
        put16(out + 17, 64); put16(out + 19, UF2_DISK_SECTORS); out[21] = 0xf8;
        put16(out + 22, FAT_SECTORS); put16(out + 24, 1); put16(out + 26, 1);
        out[36] = 0x80; out[38] = 0x29; put32(out + 39, UF2_BOARD);
        memcpy(out + 43, "FREEEEG128 ", 11); memcpy(out + 54, "FAT12   ", 8);
        out[510] = 0x55; out[511] = 0xaa;
    } else if (lba == 1 || lba == 1 + FAT_SECTORS) {
        /* FAT[0]=FF8, FAT[1]=FFF, FAT[2]=FFF (INFO), FAT[3]=FFF (STATUS). */
        out[0] = 0xf8; memset(out + 1, 0xff, 5);
    } else if (lba == ROOT_LBA) {
        entry(out, "FREEEEG128 ", 0x08, 0, 0);
        entry(out + 32, "INFO_UF2TXT", 0x20, 2, sizeof(info) - 1);
        entry(out + 64, "STATUS  TXT", 0x20, 3, 512);
    } else if (lba == DATA_LBA) memcpy(out, info, sizeof(info) - 1);
    else if (lba == DATA_LBA + 1) {
        memset(out, ' ', 512); uf2_status((char *)out, 512);
        { size_t used = strlen((char *)out); memset(out + used, ' ', 512 - used); }
        /* Windows reads the fixed-size status file; do not expose stack bytes. */
    }
    return 0;
}
int uf2_disk_write(uint32_t lba, const uint8_t data[512])
{
    if (lba >= UF2_DISK_SECTORS) return -1;
    return uf2_accept_sector(data) < 0 ? -1 : 0;
}
