/* bios_clock.c — ver include/bios_clock.h */
#include "bios_clock.h"
#include <time.h>

uint16_t (*bios_clock_hook)(void) = NULL;

bool (*vsync_hook)(const char *site, bool need_retrace) = NULL;

bool vsync_gate(const char *site, bool need_retrace) {
    return vsync_hook ? vsync_hook(site, need_retrace) : true;
}

uint16_t bios_clock_read(void) {
    if (bios_clock_hook) return bios_clock_hook();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55);                      /* ~18.2 ticks/s */
}
