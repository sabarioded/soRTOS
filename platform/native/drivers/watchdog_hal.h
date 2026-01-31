#ifndef WATCHDOG_HAL_NATIVE_H
#define WATCHDOG_HAL_NATIVE_H

#include <stdint.h>

int watchdog_hal_init(uint32_t timeout_ms);
void watchdog_hal_kick(void);

#endif /* WATCHDOG_HAL_NATIVE_H */
