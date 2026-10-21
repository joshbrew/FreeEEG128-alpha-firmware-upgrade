/* Execute the actual updater against a strict flash model, including torn writes. */
#include "uf2_update.h"
#include "uf2_disk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FLASH_BASE 0x08000000u
#define FLASH_BYTES 0x200000u
#define IMAGE_SIZE 1536u
static uint8_t flash[FLASH_BYTES], programmed[FLASH_BYTES / 32], ecc_bad[FLASH_BYTES / 32];
static uint8_t image[IMAGE_SIZE], blocks[IMAGE_SIZE / 256][512];
static uf2_record record;
static unsigned operations, fail_at, boot_touched;

static void put(uint8_t *p, uint32_t v)
{ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
const uint8_t *uf2_flash_read(uint32_t address, uint32_t length)
{
    uint32_t i;
    assert(address >= FLASH_BASE && length <= FLASH_BYTES && address - FLASH_BASE <= FLASH_BYTES - length);
    for (i = 0; i < length; i += 32) if (ecc_bad[(address - FLASH_BASE + i) / 32]) return NULL;
    return flash + address - FLASH_BASE;
}
int uf2_flash_erase(unsigned bank, unsigned sector)
{
    unsigned offset = (bank - 1) * 0x100000 + sector * UF2_SECTOR_SIZE;
    unsigned count = UF2_SECTOR_SIZE;
    assert(bank == 1 || bank == 2); assert(sector <= 7);
    assert(!(bank == 2 && sector == 6)); /* Saved settings must survive every update. */
    if (bank == 1 && sector == 0) ++boot_touched;
    if (++operations == fail_at) {
        memset(flash + offset, 0xff, count / 2);
        memset(ecc_bad + offset / 32, 1, count / 32);
        return 0;
    }
    memset(flash + offset, 0xff, count);
    memset(programmed + offset / 32, 0, count / 32);
    memset(ecc_bad + offset / 32, 0, count / 32);
    return 1;
}
int uf2_flash_program(uint32_t address, const uint8_t *data, uint32_t length)
{
    unsigned i, offset = address - FLASH_BASE;
    assert(address >= UF2_APP_BASE && address + length <= 0x08200000u);
    assert(!(address & 31) && !(length & 31));
    assert(address + length <= 0x081c0000u || address >= 0x081e0000u);
    for (i = 0; i < length; i += 32) {
        assert(!programmed[(offset + i) / 32]); /* H7 cannot safely reprogram an ECC flash word. */
        if (++operations == fail_at) {
            memcpy(flash + offset + i, data + i, 16);
            ecc_bad[(offset + i) / 32] = 1;
            return 0;
        }
        memcpy(flash + offset + i, data + i, 32); programmed[(offset + i) / 32] = 1;
    }
    return 1;
}
static void fixtures(void)
{
    unsigned i;
    uf2_board_header board = { UF2_BOARD_MAGIC, UF2_LAYOUT_VERSION, UF2_BOARD, UF2_APP_BASE, UF2_APP_LIMIT, {0} };
    for (i = 0; i < IMAGE_SIZE; ++i) image[i] = (uint8_t)(i * 37);
    put(image, 0x20020000u); put(image + 4, UF2_APP_BASE + 0x501);
    memcpy(image + UF2_BOARD_OFFSET, &board, sizeof(board));
    record.magic = UF2_RECORD_MAGIC; record.layout = UF2_LAYOUT_VERSION; record.board = UF2_BOARD;
    record.length = IMAGE_SIZE; record.image_crc = uf2_crc32(image, IMAGE_SIZE);
    record.length_inverse = ~record.length; record.crc_inverse = ~record.image_crc;
    record.record_crc = uf2_crc32(&record, 28);
    for (i = 0; i < IMAGE_SIZE / 256; ++i) {
        uint8_t *b = blocks[i];
        memset(b, 0, 512); put(b, 0x0a324655); put(b + 4, 0x9e5d5157);
        put(b + 8, 0x2000); put(b + 12, UF2_APP_BASE + i * 256); put(b + 16, 256);
        put(b + 20, i); put(b + 24, IMAGE_SIZE / 256); put(b + 28, UF2_FAMILY);
        memcpy(b + 32, image + i * 256, 256); memcpy(b + 288, &record, sizeof(record)); put(b + 508, 0x0ab16f30);
    }
}
static void reset_flash(void)
{
    memset(flash, 0xff, sizeof(flash)); memset(programmed, 0, sizeof(programmed)); memset(ecc_bad, 0, sizeof(ecc_bad));
    memset(flash, 0x55, UF2_SECTOR_SIZE); /* Immutable loader sentinel. */
    memset(flash + 0x1c0000, 0x33, UF2_SECTOR_SIZE); /* Saved-settings sentinel. */
    operations = fail_at = boot_touched = 0; uf2_update_init();
}
static void upload(void)
{
    unsigned i;
    static const unsigned order[] = {4, 0, 5, 2, 1, 3};
    for (i = 0; i < 6; ++i) assert(uf2_accept_sector(blocks[order[i]]) == 1);
    assert(uf2_update_ready());
}
static void factory_application(void)
{
    memcpy(flash + UF2_APP_BASE - FLASH_BASE, image, IMAGE_SIZE);
    memcpy(flash + UF2_APP_META - FLASH_BASE, &record, sizeof(record));
}
int main(void)
{
    unsigned i, point, upload_operations, install_operations;
    uint8_t bad[512], disk[512];
    fixtures(); reset_flash();
    assert(uf2_crc32("123456789", 9) == 0xcbf43926);
    assert(!uf2_application_valid() && uf2_install_pending() == 0);
    memset(bad, 0, sizeof(bad)); assert(uf2_accept_sector(bad) == 0 && operations == 0);
    memcpy(bad, blocks[0], 512); put(bad + 28, 0x12345678);
    assert(uf2_accept_sector(bad) == -1 && operations == 0);
    memcpy(bad, blocks[0], 512); put(bad + 12, 0x08000000);
    assert(uf2_accept_sector(bad) == -1 && operations == 0);
    memcpy(bad, blocks[0], 512); put(bad + 20, 0xffffffff);
    assert(uf2_accept_sector(bad) == -1 && operations == 0);
    memcpy(bad, blocks[0], 512); put(bad + 24, 0xffffffff);
    assert(uf2_accept_sector(bad) == -1 && operations == 0);
    assert(uf2_accept_sector(blocks[0]) == 1);
    i = operations; assert(uf2_accept_sector(blocks[0]) == 1 && operations == i);
    memcpy(bad, blocks[0], 512); bad[32] ^= 1;
    assert(uf2_accept_sector(bad) == -1);
    for (i = 1; i < 6; ++i) assert(uf2_accept_sector(blocks[i]) == -1);
    assert(!uf2_update_ready());
    reset_flash(); upload(); upload_operations = operations;
    assert(uf2_install_pending() == 1 && uf2_application_valid());
    assert(!memcmp(flash + UF2_APP_BASE - FLASH_BASE, image, IMAGE_SIZE));
    i = operations; assert(uf2_install_pending() == 0 && operations == i);
    install_operations = i - upload_operations;
    /* Missing blocks can never produce a staged commit. */
    reset_flash();
    for (i = 0; i < 5; ++i) assert(uf2_accept_sector(blocks[i]) == 1);
    assert(!uf2_update_ready() && uf2_install_pending() == 0);
    /* Payload corruption is detected only after all blocks arrive; the app is untouched. */
    reset_flash(); factory_application();
    for (i = 0; i < 6; ++i) {
        memcpy(bad, blocks[i], 512); if (i == 3) bad[47] ^= 1;
        assert(uf2_accept_sector(bad) == (i == 5 ? -1 : 1));
    }
    assert(!uf2_update_ready() && uf2_application_valid());
    /* Fail at every erase or 32-byte flash-word write in the upload. */
    for (point = 1; point <= upload_operations; ++point) {
        reset_flash(); factory_application(); fail_at = point;
        for (i = 0; i < 6; ++i) if (uf2_accept_sector(blocks[i]) < 0) break;
        assert(i < 6 && uf2_application_valid());
        fail_at = 0; uf2_update_init();
        /* Torn staged metadata may request recovery, but it must never alter the app. */
        assert(uf2_install_pending() <= 0 && uf2_application_valid());
        assert(!boot_touched);
    }
    /* Fail at every installation write; a fresh reset must recover the intact stage. */
    for (point = 1; point <= install_operations; ++point) {
        reset_flash(); upload(); fail_at = operations + point;
        assert(uf2_install_pending() == -1);
        fail_at = 0; uf2_update_init();
        assert(uf2_install_pending() == 1 && uf2_application_valid());
        assert(!boot_touched);
        for (i = 0; i < UF2_SECTOR_SIZE; ++i) assert(flash[i] == 0x55);
    }
    assert(uf2_disk_read(0, disk) == 0 && disk[510] == 0x55 && disk[511] == 0xaa);
    assert(uf2_disk_read(25, disk) == 0 && !memcmp(disk + 32, "INFO_UF2TXT", 11));
    assert(uf2_disk_read(UF2_DISK_SECTORS, disk) == -1);
    assert(uf2_disk_write(UF2_DISK_SECTORS, blocks[0]) == -1);
    /* Exercise all six application sectors at the exact supported upper bound. */
    reset_flash();
    memcpy(flash + UF2_STAGE_BASE - FLASH_BASE, image, IMAGE_SIZE);
    { uf2_record largest = record;
      largest.length = UF2_APP_MAX;
      largest.image_crc = uf2_crc32(flash + UF2_STAGE_BASE - FLASH_BASE, largest.length);
      largest.length_inverse = ~largest.length; largest.crc_inverse = ~largest.image_crc;
      largest.record_crc = uf2_crc32(&largest, 28);
      memcpy(flash + UF2_STAGE_META - FLASH_BASE, &largest, sizeof(largest)); }
    assert(uf2_install_pending() == 1 && uf2_application_valid() && !boot_touched);
    for (i = 0; i < UF2_SECTOR_SIZE; ++i) assert(flash[0x1c0000+i] == 0x33);
    printf("UF2 C tests passed: reordered/duplicate/missing/malformed blocks, CRC, FAT12, %u upload and %u install power-cut points, ECC faults, loader preserved.\n",
           upload_operations, install_operations);
    return 0;
}
