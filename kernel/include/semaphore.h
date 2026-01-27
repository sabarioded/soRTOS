#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>
#include "project_config.h"
#include "spinlock.h"
#include "scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t count;    /* Current number of available resources */
    uint32_t max_count;         /* Maximum number of resources */
    
    /* Linked list for waiting tasks */
    wait_node_t *wait_head;
    wait_node_t *wait_tail;
    
    spinlock_t lock;
} so_sem_t;

/** 
 * @brief Initialize a semaphore.
 * 
 * @param s Pointer to the semaphore structure.
 * @param initial_count Initial number of available resources.
 * @param max_count Maximum limit for the count (use 1 for binary semaphore).
 */
void so_sem_init(so_sem_t *s, uint32_t initial_count, uint32_t max_count);

/** 
 * @brief Wait (Take) for a semaphore token.
 * 
 * If the count is > 0, it decrements the count and returns immediately.
 * If the count is 0, the calling task is blocked and placed in the wait queue
 * until a token is signaled.
 * 
 * @param s Pointer to the semaphore.
 */
void so_sem_wait(so_sem_t *s);

/** 
 * @brief Signal (Give) a semaphore token.
 * 
 * Increments the semaphore count (up to max_count).
 * If there are tasks waiting, the longest-waiting task is woken up.
 * 
 * @param s Pointer to the semaphore.
 */
void so_sem_signal(so_sem_t *s);

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
void so_sem_broadcast(so_sem_t *s);

#ifdef __cplusplus
}
#endif

#endif /* SEMAPHORE_H */