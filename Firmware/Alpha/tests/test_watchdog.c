#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "watchdog.h"
static unsigned starts, feeds;
static int success = 1;
int runtime_watchdog_start(void) { ++starts; return success; }
void runtime_watchdog_feed(void) { ++feeds; }
int main(void)
{
    watchdog_init(0);
    assert(starts == 1 && watchdog_active());
    watchdog_service(999); assert(feeds == 0);
    watchdog_service(1000); assert(feeds == 1);
    watchdog_service(1001); assert(feeds == 1);
    watchdog_service(6000); assert(feeds == 2);
    /* Millisecond wrap must not suppress feeding for a whole tick cycle. */
    watchdog_init(UINT32_MAX - 500u);
    watchdog_service(498); assert(feeds == 2);
    watchdog_service(499); assert(feeds == 3);
    success = 0; watchdog_init(0); assert(!watchdog_active());
    watchdog_service(2000); assert(feeds == 3);
    puts("Watchdog policy passed: main-loop cadence, delayed work, tick wrap and initialization failure.");
    return 0;
}
