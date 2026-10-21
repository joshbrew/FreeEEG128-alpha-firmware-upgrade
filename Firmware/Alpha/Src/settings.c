#include "settings.h"
#include <string.h>
#define SETTINGS_MAGIC 0x38474643u
#define RECORD_BYTES 96u

static uint32_t crc32(const void *data, unsigned length)
{
    const uint8_t *p = data;
    uint32_t crc = ~0u;
    unsigned bit;
    while (length--) {
        crc ^= *p++;
        for (bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
void settings_defaults(eeg_settings *s)
{ s->rate = 500; memset(s->gains, 8, sizeof(s->gains)); }
static int valid_settings(const eeg_settings *s)
{
    unsigned ch;
    if (s->rate != 250 && s->rate != 500 && s->rate != 1000 && s->rate != 2000) return 0;
    for (ch = 0; ch < 128; ++ch) {
        unsigned g = s->gains[ch];
        if (!g || g > 128 || (g & (g - 1))) return 0;
    }
    return 1;
}
/* Three 256-bit flash words per record. The last word commits the CRC and
   inverse after the first two have been written. Torn/ECC records are consumed,
   never rewritten; the preceding valid record survives interrupted SAVE. */
static int scan(eeg_settings *s, uint32_t *next, uint32_t *sequence)
{
    uint32_t offset, words[24];
    int found = 0;
    settings_defaults(s); *next = 0; *sequence = 0;
    for (offset = 0; offset + RECORD_BYTES <= SETTINGS_BYTES; offset += RECORD_BYTES) {
        unsigned i, blank = 1;
        if (!settings_read(offset, words) || !settings_read(offset + 32, words + 8) ||
            !settings_read(offset + 64, words + 16)) { *next = offset + RECORD_BYTES; continue; }
        for (i = 0; i < 24; ++i) if (words[i] != ~0u) blank = 0;
        if (blank) continue;
        *next = offset + RECORD_BYTES;
        if (words[0] != SETTINGS_MAGIC || words[1] != 1 ||
            words[22] != crc32(words, 88) || words[23] != ~words[22]) continue;
        {
            eeg_settings candidate;
            candidate.rate = words[3];
            for (i = 0; i < 128; ++i) {
                unsigned code = (words[4 + i / 8] >> (4 * (i % 8))) & 15;
                candidate.gains[i] = code <= 7 ? (uint8_t)(1u << code) : 0;
            }
            if (!valid_settings(&candidate)) continue;
            *s = candidate; *sequence = words[2]; found = 1;
        }
    }
    return found;
}
int settings_load(eeg_settings *s)
{ uint32_t next, sequence; return scan(s, &next, &sequence); }
int settings_save(const eeg_settings *s)
{
    eeg_settings old;
    uint32_t next, sequence, words[24] = {SETTINGS_MAGIC, 1}, check[8];
    unsigned i;
    int found;
    if (!valid_settings(s)) return 0;
    found = scan(&old, &next, &sequence);
    if (found && old.rate == s->rate && !memcmp(old.gains, s->gains, sizeof(old.gains))) return 1;
    if (next + RECORD_BYTES > SETTINGS_BYTES) return -1;
    words[2] = sequence + 1; words[3] = s->rate;
    for (i = 0; i < 128; ++i) {
        unsigned code = 0;
        while ((1u << code) != s->gains[i]) ++code;
        words[4 + i / 8] |= code << (4 * (i % 8));
    }
    words[22] = crc32(words, 88); words[23] = ~words[22];
    for (i = 0; i < 3; ++i)
        if (!settings_program(next + i * 32, words + i * 8) ||
            !settings_read(next + i * 32, check) || memcmp(words + i * 8, check, 32)) return 0;
    return 1;
}
int settings_factory(void)
{
    eeg_settings defaults;
    settings_defaults(&defaults);
    return settings_erase() && settings_save(&defaults) == 1;
}
