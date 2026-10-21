#include "uf2_update.h"
#include <string.h>
#include <stdio.h>

static uint8_t seen[UF2_MAX_BLOCKS / 8];
static uf2_record session;
static uint32_t received, erased;
static int active, ready, failed;
static const char *message;

static uint32_t word(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

uint32_t uf2_crc32(const void *input, size_t length)
{
    const uint8_t *data = input;
    uint32_t crc = 0xffffffffu;
    size_t i;
    unsigned bit;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

int uf2_record_valid(const uf2_record *r)
{
    return r->magic == UF2_RECORD_MAGIC && r->layout == UF2_LAYOUT_VERSION && r->board == UF2_BOARD &&
           r->length >= UF2_BOARD_OFFSET + sizeof(uf2_board_header) && r->length <= UF2_APP_MAX &&
           r->length % UF2_BLOCK_SIZE == 0 && r->length_inverse == ~r->length &&
           r->crc_inverse == ~r->image_crc && uf2_crc32(r, 28) == r->record_crc;
}

int uf2_image_valid(uint32_t base, const uf2_record *r)
{
    const uint8_t *image;
    const uf2_board_header *board;
    uint32_t sp, entry;
    if (!uf2_record_valid(r)) return 0;
    image = uf2_flash_read(base, r->length);
    if (!image || uf2_crc32(image, r->length) != r->image_crc) return 0;
    board = (const uf2_board_header *)(image + UF2_BOARD_OFFSET);
    if (board->magic != UF2_BOARD_MAGIC || board->layout != UF2_LAYOUT_VERSION ||
        board->board != UF2_BOARD || board->app_base != UF2_APP_BASE || board->app_limit != UF2_APP_LIMIT)
        return 0;
    sp = word(image); entry = word(image + 4);
    /* This build links stack into 128 KB DTCM. Never jump into an arbitrary RAM/flash region. */
    return sp > 0x20000000u && sp <= 0x20020000u && !(sp & 7) &&
           (entry & 1) && (entry & ~1u) >= UF2_APP_BASE && (entry & ~1u) < UF2_APP_BASE + r->length;
}

int uf2_application_valid(void)
{
    uf2_record r;
    const uint8_t *raw = uf2_flash_read(UF2_APP_META, sizeof(r));
    if (!raw) return 0;
    memcpy(&r, raw, sizeof(r));
    return uf2_image_valid(UF2_APP_BASE, &r);
}

int uf2_install_pending(void)
{
    uf2_record r;
    const uint8_t *raw = uf2_flash_read(UF2_STAGE_META, sizeof(r));
    uint32_t offset;
    unsigned sectors, i;
    if (!raw) return -1;
    memcpy(&r, raw, sizeof(r));
    if (!uf2_record_valid(&r)) return 0; /* No committed upload. */
    if (!uf2_image_valid(UF2_STAGE_BASE, &r)) return -1;
    raw = uf2_flash_read(UF2_APP_META, sizeof(r));
    if (raw && !memcmp(raw, &r, sizeof(r)) && uf2_image_valid(UF2_APP_BASE, &r)) return 0;
    /* Invalidate the old app FIRST. A reset at any following write retries the intact stage. */
    if (!uf2_flash_erase(1, 7)) return -1;
    sectors = (r.length + UF2_SECTOR_SIZE - 1) / UF2_SECTOR_SIZE;
    for (i = 0; i < sectors; ++i) if (!uf2_flash_erase(1, i + 1)) return -1;
    for (offset = 0; offset < r.length; offset += UF2_BLOCK_SIZE) {
        raw = uf2_flash_read(UF2_STAGE_BASE + offset, UF2_BLOCK_SIZE);
        if (!raw || !uf2_flash_program(UF2_APP_BASE + offset, raw, UF2_BLOCK_SIZE)) return -1;
    }
    if (!uf2_image_valid(UF2_APP_BASE, &r)) return -1;
    /* Last write is the new application's durable commit record. */
    if (!uf2_flash_program(UF2_APP_META, (const uint8_t *)&r, sizeof(r))) return -1;
    return uf2_application_valid() ? 1 : -1;
}

void uf2_update_init(void)
{
    memset(seen, 0, sizeof(seen)); memset(&session, 0, sizeof(session));
    received = erased = 0; active = ready = failed = 0; message = "Ready for FreeEEG128 .uf2";
}

static int reject(const char *why)
{
    message = why;
    /* A malformed candidate must never allow an earlier partial session to commit. */
    if (active && !ready) failed = 1;
    return -1;
}

int uf2_accept_sector(const uint8_t data[512])
{
    uf2_record r;
    uint32_t address, block, count, sector, offset;
    const uint8_t *written;
    if (word(data) != 0x0a324655u || word(data + 4) != 0x9e5d5157u) return 0;
    if (word(data + 508) != 0x0ab16f30u || word(data + 8) != 0x2000u ||
        word(data + 16) != UF2_BLOCK_SIZE || word(data + 28) != UF2_FAMILY)
        return reject("Rejected UF2 format/family");
    memcpy(&r, data + 288, sizeof(r)); /* Board manifest in UF2's unused padding, repeated in every block. */
    if (!uf2_record_valid(&r)) return reject("Rejected board manifest");
    address = word(data + 12); block = word(data + 20); count = word(data + 24);
    if (count != r.length / UF2_BLOCK_SIZE || block >= count ||
        address != UF2_APP_BASE + block * UF2_BLOCK_SIZE)
        return reject("Rejected block address/count");
    if (failed) return -1;
    if (active && memcmp(&r, &session, sizeof(r))) return reject("Mixed update images; reset and retry");
    if (!active) {
        /* A previous staged commit cannot survive into a new partial upload. */
        if (!uf2_flash_erase(2, 7)) return reject("Stage metadata erase failed");
        session = r; active = 1;
    }
    offset = block * UF2_BLOCK_SIZE;
    if (seen[block / 8] & (1u << (block % 8))) {
        written = uf2_flash_read(UF2_STAGE_BASE + offset, UF2_BLOCK_SIZE);
        return written && !memcmp(written, data + 32, UF2_BLOCK_SIZE) ? 1 : reject("Conflicting duplicate block");
    }
    sector = offset / UF2_SECTOR_SIZE;
    if (!(erased & (1u << sector))) {
        if (!uf2_flash_erase(2, sector)) return reject("Stage sector erase failed");
        erased |= 1u << sector;
    }
    if (!uf2_flash_program(UF2_STAGE_BASE + offset, data + 32, UF2_BLOCK_SIZE)) return reject("Stage write failed");
    written = uf2_flash_read(UF2_STAGE_BASE + offset, UF2_BLOCK_SIZE);
    if (!written || memcmp(written, data + 32, UF2_BLOCK_SIZE)) return reject("Stage readback failed");
    seen[block / 8] |= (uint8_t)(1u << (block % 8)); ++received;
    message = "Receiving image";
    if (received == count) {
        if (!uf2_image_valid(UF2_STAGE_BASE, &session)) return reject("Image CRC/vector/board check failed");
        if (!uf2_flash_program(UF2_STAGE_META, (const uint8_t *)&session, sizeof(session)))
            return reject("Stage commit failed");
        written = uf2_flash_read(UF2_STAGE_META, sizeof(session));
        if (!written || memcmp(written, &session, sizeof(session))) return reject("Stage commit readback failed");
        ready = 1; message = "Verified; restarting to install";
    }
    return 1;
}

int uf2_update_ready(void) { return ready; }

void uf2_status(char *output, size_t capacity)
{
    snprintf(output, capacity, "FreeEEG128 UF2 loader v1\r\n%s\r\nBlocks: %lu / %lu\r\n"
             "Application: 0x08020000; maximum 768 KB\r\nReset after a rejected/partial copy to retry.\r\n",
             message, (unsigned long)received, (unsigned long)(session.length / UF2_BLOCK_SIZE));
}
