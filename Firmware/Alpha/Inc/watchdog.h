#ifndef FREEEEG128_WATCHDOG_H
#define FREEEEG128_WATCHDOG_H
#include <stdint.h>
void watchdog_init(uint32_t now_ms);
void watchdog_service(uint32_t now_ms); /* Main loop only, after work returns. */
int watchdog_active(void);
#endif
