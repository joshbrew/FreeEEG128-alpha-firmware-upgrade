#include "watchdog.h"
#include "runtime.h"
static uint32_t last_feed;
static int active;
void watchdog_init(uint32_t now_ms)
{
    active = runtime_watchdog_start();
    last_feed = now_ms;
}
void watchdog_service(uint32_t now_ms)
{
    if (active && (uint32_t)(now_ms - last_feed) >= 1000u) {
        runtime_watchdog_feed();
        last_feed = now_ms;
    }
}
int watchdog_active(void) { return active; }
