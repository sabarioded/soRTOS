#ifndef MUTEX_H
#define MUTEX_H

#include <stdint.h>
#include "project_config.h" /* For MAX_TASKS */

typedef struct {
    volatile uint8_t locked;
    void *owner;                /* Task holding the lock */
    
    /* Simple circular buffer for waiting tasks */
    void *wait_queue[MAX_TASKS];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} mutex_t;

/** @brief Initialize a mutex */
void mutex_init(mutex_t *m);

/** @brief Acquire the lock. Blocks if already locked. */
void mutex_lock(mutex_t *m);

/** @brief Release the lock. Wakes up the next waiting task. */
void mutex_unlock(mutex_t *m);

#endif /* MUTEX_H */