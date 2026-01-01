#include "mutex.h"
#include "scheduler.h"
#include "arch_ops.h"
#include "platform.h"
#include <stddef.h>

/* Initialize a mutex */
void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->owner = NULL;
    m->head = 0;
    m->tail = 0;
    m->count = 0;
    for(int i = 0; i < MAX_TASKS; i++) {
        m->wait_queue[i] = NULL;
    }
}

/* Acquire lock. If busy, block current task and yield. */
void mutex_lock(mutex_t *m) {
    while(1) {
        uint32_t flag = arch_irq_lock();
        
        void* current_task = task_get_current();
        
        /* Check if we already own it (Direct Handoff) */
        if (m->locked && m->owner == current_task) {
            arch_irq_unlock(flag);
            return;
        }
        
        /* If unlocked, grab it */
        if (m->locked == 0) {
            m->locked = 1;
            m->owner = current_task;
            arch_irq_unlock(flag);
            return;
        }

        /* If locked, add to wait queue */
        if (m->count < MAX_TASKS) {
            m->wait_queue[m->tail] = current_task;
            m->tail = (m->tail + 1) % MAX_TASKS;
            m->count++;

            task_set_state((task_t*)current_task, TASK_BLOCKED); 
        }

        arch_irq_unlock(flag);
        
        /* Yield CPU to allow other tasks to run */
        platform_yield();
    }
}

/* Release lock and wake up one waiting task */
void mutex_unlock(mutex_t *m) {
    uint32_t flag = arch_irq_lock();

    /* Only the owner can unlock */
    if(m->owner != task_get_current()) {
        arch_irq_unlock(flag);
        return;
    }

    /* If tasks are waiting, perform Direct Handoff */
    if (m->count > 0) {
        void *next_task = m->wait_queue[m->head];
        m->head = (m->head + 1) % MAX_TASKS;
        m->count--;

        /* Pass ownership directly. Lock remains 1. */
        m->owner = next_task;
        task_set_state((task_t*)next_task, TASK_READY);
    } else {
        /* No one waiting, release the lock */
        m->locked = 0;
        m->owner = NULL;
    }

    arch_irq_unlock(flag);
}
