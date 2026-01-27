#ifndef MUTEX_H
#define MUTEX_H

#include <stdint.h>
#include "project_config.h"
#include "spinlock.h"
#include "scheduler.h"

typedef struct {
    spinlock_t lock;
    void *owner; /* Task holding the lock */
    
    /* Linked list for waiting tasks */
    wait_node_t *wait_head;
    wait_node_t *wait_tail;
} mutex_t;

/**
 * @brief Initialize a mutex.
 * @param m Pointer to the mutex structure.
 */
void mutex_init(mutex_t *m);

/**
 * @brief Acquire the lock.
 * 
 * Blocks the current task if the mutex is already locked by another task.
 * @param m Pointer to the mutex structure.
 */
void mutex_lock(mutex_t *m);

/**
 * @brief Release the lock.
 * 
 * Wakes up the next waiting task (if any).
 * @param m Pointer to the mutex structure.
 */
void mutex_unlock(mutex_t *m);

#endif /* MUTEX_H */