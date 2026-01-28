#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*timer_callback_t)(void *arg);

typedef struct sw_timer sw_timer_t;

/**
 * @brief Initialize the software timer subsystem.
 * 
 * Creates the daemon task that manages timers.
 * @param max_timers Maximum number of timers in the pool (0 for default).
 */
void timer_service_init(uint32_t max_timers);

/**
 * @brief Create a new software timer.
 * 
 * @param name Debug name for the timer.
 * @param period_ticks Duration of the timer.
 * @param auto_reload 1 for periodic, 0 for one-shot.
 * @param callback Function to call when timer expires.
 * @param arg Argument to pass to the callback.
 * @return Pointer to timer handle or NULL on failure.
 */
sw_timer_t* timer_create(const char *name, uint32_t period_ticks, uint8_t auto_reload, timer_callback_t callback, void *arg);

/**
 * @brief Start a timer.
 * If already active, it resets the expiry time.
 * @param timer Timer handle.
 * @return 0 on success.
 */
int timer_start(sw_timer_t *timer);

/**
 * @brief Stop a timer.
 * @param timer Timer handle.
 * @return 0 on success.
 */
int timer_stop(sw_timer_t *timer);

/**
 * @brief Delete a timer and free memory.
 * @param timer Timer handle.
 */
void timer_delete(sw_timer_t *timer);

/**
 * @brief Check for expired timers and execute callbacks.
 * @return Number of ticks until the next timer expires, or UINT32_MAX if none.
 */
uint32_t timer_check_expiries(void);

/**
 * @brief Get the debug name of the timer.
 * @param timer Timer handle.
 * @return Name string.
 */
const char* timer_get_name(sw_timer_t *timer);

/**
 * @brief Get the period of the timer.
 * @param timer Timer handle.
 * @return Period in ticks.
 */
uint32_t timer_get_period(sw_timer_t *timer);

/**
 * @brief Check if the timer is currently active.
 * @param timer Timer handle.
 * @return 1 if active, 0 if stopped.
 */
uint8_t timer_is_active(sw_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_H */