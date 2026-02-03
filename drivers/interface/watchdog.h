#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the Independent Watchdog.
 * 
 * @param timeout_ms Desired timeout in milliseconds.
 * The actual timeout might be slightly different depending on hardware constraints.
 * @return int 0 on success, -1 on error.
 */
int watchdog_init(uint32_t timeout_ms);

/**
 * @brief Refresh (kick) the watchdog timer to prevent system reset.
 * This must be called periodically within the timeout period.
 */
void watchdog_kick(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_H */
