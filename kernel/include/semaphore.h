#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>
#include "project_config.h"

typedef struct {
    volatile uint32_t count;    /**< Current number of available resources */
    uint32_t max_count;         /**< Maximum number of resources (1 for binary sem) */
    
    /* Wait queue */
    void *wait_queue[MAX_TASKS]; /**< Circular buffer of tasks waiting for this semaphore */
    uint8_t head;                /**< Index of the next task to wake up */
    uint8_t tail;                /**< Index where the next blocked task will be placed */
    uint8_t q_count;             /**< Number of tasks currently waiting */
} sem_t;

/** 
 * @brief Initialize a semaphore.
 * 
 * @param s Pointer to the semaphore structure.
 * @param initial_count Initial number of available resources.
 * @param max_count Maximum limit for the count (use 1 for binary semaphore).
 */
void sem_init(sem_t *s, uint32_t initial_count, uint32_t max_count);

/** 
 * @brief Wait (Take) for a semaphore token.
 * 
 * If the count is > 0, it decrements the count and returns immediately.
 * If the count is 0, the calling task is blocked and placed in the wait queue
 * until a token is signaled.
 * 
 * @param s Pointer to the semaphore.
 */
void sem_wait(sem_t *s);

/** 
 * @brief Signal (Give) a semaphore token.
 * 
 * Increments the semaphore count (up to max_count).
 * If there are tasks waiting, the longest-waiting task is woken up.
 * 
 * @param s Pointer to the semaphore.
 */
void sem_signal(sem_t *s);

/** 
 * @brief Broadcast the semaphore to all waiting tasks.
 * 
 * Wakes up ALL tasks currently waiting in the queue.
 * Note: This increments the count for each woken task (up to max_count).
 * If max_count is reached, remaining tasks will wake up but may block again 
 * if they cannot acquire a token.
 * 
 * @param s Pointer to the semaphore.
 */
void sem_broadcast(sem_t *s);

#endif /* SEMAPHORE_H */