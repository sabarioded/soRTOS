#include "semaphore.h"
#include "scheduler.h"
#include "arch_ops.h"
#include "platform.h"
#include <stddef.h>

/* Initialize a semaphore */
void sem_init(sem_t *s, uint32_t initial_count, uint32_t max_count) {
    s->count = initial_count;
    s->max_count = max_count;
    s->head = 0;
    s->tail = 0;
    s->q_count = 0;
    for(int i=0; i<MAX_TASKS; i++) {
        s->wait_queue[i] = NULL;
    }
}

/* Wait for the semaphore. Block if count is 0. */
void sem_wait(sem_t *s) {
    while(1) {
        uint32_t flag = arch_irq_lock();

        /* If resource available, take it */
        if (s->count > 0) {
            s->count--;
            arch_irq_unlock(flag);
            return;
        }

        /* No resource available: add to wait queue and block */
        if (s->q_count < MAX_TASKS) {
            void *current_task = task_get_current();
            s->wait_queue[s->tail] = current_task;
            s->tail = (s->tail + 1) % MAX_TASKS;
            s->q_count++;
            task_set_state((task_t*)current_task, TASK_BLOCKED);
        }

        arch_irq_unlock(flag);
        
        /* Yield CPU so scheduler can switch to another task */
        platform_yield();
    }
}

/* Give a token. Wake up one waiter if any. */
void sem_signal(sem_t *s) {
    uint32_t flag = arch_irq_lock();

    /* If tasks are waiting, wake the first one */
    if (s->q_count > 0) {
        void *task = s->wait_queue[s->head];
        s->head = (s->head + 1) % MAX_TASKS;
        s->q_count--;
        task_set_state((task_t*)task, TASK_READY);
    }
    
    /* Increment count, but cap at max */
    if (s->count < s->max_count) {
        s->count++;
    }

    arch_irq_unlock(flag);
}

/* Wake up ALL waiting tasks */
void sem_broadcast(sem_t *s) {
    uint32_t flag = arch_irq_lock();

    /* Wake everyone in the queue */
    while(s->q_count > 0) {
        void *task = s->wait_queue[s->head];
        s->head = (s->head + 1) % MAX_TASKS;
        s->q_count--;
        task_set_state((task_t*)task, TASK_READY);

        /* Increment count for each woken task, capped at max */
        if (s->count < s->max_count) {
            s->count++;
        }
    }

    arch_irq_unlock(flag);
}