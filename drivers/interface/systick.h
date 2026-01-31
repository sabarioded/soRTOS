#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the System Timer to generate interrupts.
 * @param ticks_hz Desired tick frequency in Hz (e.g., 1000 for 1ms ticks).
 * @return int 0 on success, -1 on error.
 */
int systick_init(uint32_t ticks_hz);

/**
 * @brief Get the number of system ticks since initialization.
 * This is the system "uptime".
 * @return uint32_t Number of ticks.
 */
uint32_t systick_get_ticks(void);

/**
 * @brief Blocking delay for a specified number of ticks.
 * @param ticks Number of ticks to wait.
 */
void systick_delay_ticks(uint32_t ticks);

/**
 * @brief Core SysTick handler called by the ISR/HAL.
 * Increments the tick counter and runs the scheduler.
 */
void systick_core_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTICK_H */