#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t memory[SETTINGS_BYTES], used[SETTINGS_BYTES / 32], bad[SETTINGS_BYTES / 32];
static unsigned writes, fail_at, erase_fail;
int settings_read(uint32_t offset, uint32_t record[8])
{ assert(offset < SETTINGS_BYTES && !(offset & 31)); if (bad[offset/32]) return 0; memcpy(record, memory+offset, 32); return 1; }
int settings_program(uint32_t offset, const uint32_t record[8])
{
    assert(offset < SETTINGS_BYTES && !(offset & 31) && !used[offset/32]);
    used[offset/32] = 1; ++writes;
    if (fail_at && writes == fail_at) { memcpy(memory+offset, record, 16); bad[offset/32] = 1; return 0; }
    memcpy(memory+offset, record, 32); return 1;
}
int settings_erase(void)
{ if (erase_fail) return 0; memset(memory, 255, sizeof(memory)); memset(used, 0, sizeof(used)); memset(bad, 0, sizeof(bad)); writes = 0; return 1; }
int main(void)
{
    eeg_settings s, loaded;
    unsigned i, cut;
    for (cut = 1; cut <= 3; ++cut) {
        settings_erase(); assert(!settings_load(&loaded) && loaded.rate == 500 && loaded.gains[127] == 8);
        settings_defaults(&s); s.rate = 2000;
        for (i = 0; i < 128; ++i) s.gains[i] = (uint8_t)(1u << (i % 8));
        assert(settings_save(&s) == 1 && writes == 3);
        assert(settings_save(&s) == 1 && writes == 3);
        assert(settings_load(&loaded) && !memcmp(&s, &loaded, sizeof(s)));
        s.rate = 1000; fail_at = writes + cut; assert(settings_save(&s) == 0); fail_at = 0;
        assert(settings_load(&loaded) && loaded.rate == 2000);
        assert(settings_save(&s) == 1 && used[6]);
        assert(settings_load(&loaded) && loaded.rate == 1000);
        memory[192+12] ^= 1; assert(settings_load(&loaded) && loaded.rate == 2000);
    }
    s.rate = 512; assert(settings_save(&s) == 0);
    s.rate = 500; s.gains[127] = 3; assert(settings_save(&s) == 0); s.gains[127] = 128;
    for (i = 3; i < SETTINGS_BYTES/96; ++i) { s.rate = (i & 1) ? 500 : 1000; assert(settings_save(&s) == 1); }
    s.rate = 2000; assert(settings_save(&s) == -1);
    erase_fail = 1; assert(!settings_factory()); assert(settings_load(&loaded));
    erase_fail = 0; assert(settings_factory());
    assert(settings_load(&loaded) && loaded.rate == 500 && loaded.gains[127] == 8);
    puts("Settings: all 128 gains, no-op save, all three torn-word cuts, CRC fallback, bounds/full journal, factory passed.");
    return 0;
}
