#include "watchdog.h"
#include "watchdog_hal.h"
#include "logger.h"

/* Initialize the watchdog with a timeout in milliseconds. */
int watchdog_init(uint32_t timeout_ms) {
    int res = watchdog_hal_init(timeout_ms);
#if LOG_ENABLE
    if (res != 0) {
        logger_log("watchdog_init failed (timeout_ms=%u)", (uintptr_t)timeout_ms, 0);
    }
#endif
    return res;
}

/* Reset/kick the watchdog to prevent a timeout reset. */
void watchdog_kick(void) {
    watchdog_hal_kick();
}
