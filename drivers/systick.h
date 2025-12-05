#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>
#include "system_clock.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SysTick timer to generate interrupts at specified frequency.
 * 
 * @param ticks_hz tick frequency in Hz.
 * @return int 0 on success, -1 on error (e.g., invalid parameters).
 */
int systick_init(uint32_t ticks_hz);

/**
 * @brief Get the number of SysTick interrupts since initialization.
 * 
 * @return uint32_t Number of ticks.
 */
uint32_t systick_get_ticks(void);

/**
 * @brief Busy-wait for specified number of SysTick ticks.
 * 
 * @param ticks Number of ticks to wait.
 */
void systick_delay_ticks(uint32_t ticks);


#ifdef __cplusplus
}
#endif

#endif /* SYSTICK_H */