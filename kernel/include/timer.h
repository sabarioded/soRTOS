#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stddef.h>

typedef void (*timer_callback_t)(void *arg);

typedef struct sw_timer {
    const char *name;
    timer_callback_t callback;
    void *arg;
    
    uint32_t period_ticks;
    uint32_t expiry_tick;
    uint8_t auto_reload;
    uint8_t active;
    
    struct sw_timer *next; /* For internal linked list */
} sw_timer_t;

/**
 * @brief Initialize the software timer subsystem.
 * Creates the daemon task that manages timers.
 */
void timer_service_init(void);

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
 */
void timer_delete(sw_timer_t *timer);

#ifdef UNIT_TESTING
/** @brief Helper to manually trigger timer checks in tests */
void timer_test_run_callbacks(void);
#endif

#endif /* TIMER_H */