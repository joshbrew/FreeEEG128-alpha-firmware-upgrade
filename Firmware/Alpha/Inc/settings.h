#ifndef FREEEEG128_SETTINGS_H
#define FREEEEG128_SETTINGS_H
#include <stdint.h>
#define SETTINGS_BASE 0x081c0000u /* Bank 2 sector 6, excluded from UF2 staging. */
#define SETTINGS_BYTES 0x20000u
typedef struct { uint32_t rate; uint8_t gains[128]; } eeg_settings;
void settings_defaults(eeg_settings *settings);
/* load: 1 = saved, 0 = defaults. save: 1 = success, 0 = I/O, -1 = full. */
int settings_load(eeg_settings *settings);
int settings_save(const eeg_settings *settings);
int settings_factory(void);
/* Hardware adapter. A failed ECC read returns 0 without exposing corrupt data. */
int settings_read(uint32_t offset, uint32_t record[8]);
int settings_program(uint32_t offset, const uint32_t record[8]);
int settings_erase(void);
#endif
