#ifndef FREEEEG128_UF2_UPDATE_H
#define FREEEEG128_UF2_UPDATE_H
#include "uf2_layout.h"
#include <stddef.h>
/* Platform boundary. Only these functions touch physical flash. */
const uint8_t *uf2_flash_read(uint32_t address, uint32_t length);
int uf2_flash_erase(unsigned bank, unsigned sector);
int uf2_flash_program(uint32_t address, const uint8_t *data, uint32_t length);
void uf2_update_init(void);
/* 0: ordinary filesystem sector ignored; 1: accepted UF2; -1: invalid/update failure. */
int uf2_accept_sector(const uint8_t data[512]);
uint32_t uf2_crc32(const void *data, size_t length);
int uf2_record_valid(const uf2_record *record);
int uf2_image_valid(uint32_t base, const uf2_record *record);
int uf2_application_valid(void);
/* Idempotent: recover a pending stage before booting an application. */
int uf2_install_pending(void);
int uf2_update_ready(void);
void uf2_status(char *output, size_t capacity);
#endif
