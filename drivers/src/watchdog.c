#include "watchdog.h"
#include "watchdog_hal.h"

int watchdog_init(uint32_t timeout_ms) {
    return watchdog_hal_init(timeout_ms);
}

void watchdog_kick(void) {
    watchdog_hal_kick();
}