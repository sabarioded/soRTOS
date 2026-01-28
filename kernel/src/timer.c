#include "timer.h"
#include "scheduler.h"
#include "allocator.h"
#include "arch_ops.h"
#include "platform.h"
#include "utils.h"
#include "spinlock.h"
#include "mempool.h"

#define TIMER_FLAG_AUTORELOAD   (1 << 0)
#define TIMER_FLAG_ACTIVE       (1 << 1)

struct sw_timer {
    struct sw_timer *next; /* For internal linked list */
    uint32_t expiry_tick;
    uint32_t period_ticks;
    const char *name;
    timer_callback_t callback;
    void *arg;
    uint8_t flags;
};

static sw_timer_t *timer_list_head = NULL;
static uint16_t timer_task_id = 0;
static spinlock_t timer_lock;
static mempool_t *timer_pool = NULL;

/* Insert timer into sorted linked list (sorted by expiry_tick) */
static void timer_insert(sw_timer_t *tmr) {
    sw_timer_t **curr = &timer_list_head;
    
    /* Find insertion point */
    while (*curr != NULL) {
        if ((int32_t)(tmr->expiry_tick - (*curr)->expiry_tick) < 0) {
            break;
        }
        curr = &(*curr)->next;
    }
    
    tmr->next = *curr;
    *curr = tmr;
}

/* Remove timer from list */
static void timer_remove(sw_timer_t *tmr) {
    sw_timer_t **curr = &timer_list_head;
    while (*curr != NULL) {
        if (*curr == tmr) {
            *curr = tmr->next;
            tmr->next = NULL;
            return;
        }
        curr = &(*curr)->next;
    }
}

/* Check for expired timers and run callbacks */
uint32_t timer_check_expiries(void) {
    while (1) {
        uint32_t now = (uint32_t)platform_get_ticks();
        
        uint32_t flags = spin_lock(&timer_lock);
        sw_timer_t *curr = timer_list_head;
        
        if (curr != NULL) {
            /* Check if expired */
            if ((int32_t)(now - curr->expiry_tick) >= 0) {
                /* Timer expired */
                sw_timer_t *tmr = curr;
                
                /* Remove from list */
                timer_list_head = tmr->next;
                tmr->next = NULL;
                tmr->flags &= ~TIMER_FLAG_ACTIVE;
                
                /* If auto-reload, restart it */
                if (tmr->flags & TIMER_FLAG_AUTORELOAD) {
                    tmr->expiry_tick = now + tmr->period_ticks;
                    tmr->flags |= TIMER_FLAG_ACTIVE;
                    timer_insert(tmr);
                }
                
                spin_unlock(&timer_lock, flags);
                
                /* Execute callback (outside critical section) */
                if (tmr->callback) {
                    tmr->callback(tmr->arg);
                }
                
                /* Loop again immediately to check next timer */
                continue; 
            } else {
                /* Not expired yet, calculate wait time */
                uint32_t next_wake = curr->expiry_tick - now;
                spin_unlock(&timer_lock, flags);
                return next_wake;
            }
        }
        
        spin_unlock(&timer_lock, flags);
        return UINT32_MAX;
    }
}

/* The Daemon Task */
static void timer_task_entry(void *arg) {
    (void)arg;
    while (1) {
        uint32_t wait_time = timer_check_expiries();
        task_notify_wait(1, wait_time);
    }
}

/* Initialize the software timer subsystem */
void timer_service_init(uint32_t max_timers) {
    spinlock_init(&timer_lock);
    
    if (max_timers == 0) {
        max_timers = TIMER_DEFAULT_POOL_SIZE;
    }

    /* Create memory pool for timers to prevent heap fragmentation */
    timer_pool = mempool_create(sizeof(sw_timer_t), max_timers);

    /* Create the timer daemon task with high priority */
    int32_t id = task_create(timer_task_entry, NULL, STACK_SIZE_1KB, TASK_WEIGHT_HIGH);
    if (id > 0) {
        timer_task_id = (uint16_t)id;
    }
}

/* Create a new software timer */
sw_timer_t* timer_create(const char *name, 
                        uint32_t period_ticks, 
                        uint8_t auto_reload, 
                        timer_callback_t callback, 
                        void *arg) {
    if (!timer_pool) {
        return NULL;
    }

    sw_timer_t *tmr = (sw_timer_t*)mempool_alloc(timer_pool);
    if (!tmr) {
        return NULL;
    }
    
    tmr->name = name;
    tmr->period_ticks = period_ticks;
    tmr->flags = auto_reload ? TIMER_FLAG_AUTORELOAD : 0;
    tmr->callback = callback;
    tmr->arg = arg;
    tmr->next = NULL;
    
    return tmr;
}

/* Start a timer */
int timer_start(sw_timer_t *timer) {
    if (!timer) {
        return -1;
    }
    
    uint32_t flags = spin_lock(&timer_lock);
    
    if (timer->flags & TIMER_FLAG_ACTIVE) {
        timer_remove(timer);
    }
    
    timer->expiry_tick = (uint32_t)platform_get_ticks() + timer->period_ticks;
    timer->flags |= TIMER_FLAG_ACTIVE;
    
    /* Check if this new timer is the earliest one */
    uint8_t is_head = 0;
    if (timer_list_head == NULL || (int32_t)(timer->expiry_tick - timer_list_head->expiry_tick) < 0) {
        is_head = 1;
    }
    
    timer_insert(timer);
    
    spin_unlock(&timer_lock, flags);
    
    /* If we inserted at the head, wake up the timer task so it can reschedule */
    if (is_head && timer_task_id != 0) {
        task_notify(timer_task_id, 1);
    }
    
    return 0;
}

/* Stop a timer */
int timer_stop(sw_timer_t *timer) {
    if (!timer) {
        return -1;
    }
    
    uint32_t flags = spin_lock(&timer_lock);
    
    if (timer->flags & TIMER_FLAG_ACTIVE) {
        timer_remove(timer);
        timer->flags &= ~TIMER_FLAG_ACTIVE;
    }
    
    spin_unlock(&timer_lock, flags);
    return 0;
}

/* Delete a timer */
void timer_delete(sw_timer_t *timer) {
    if (!timer) {
        return;
    }
    
    timer_stop(timer);
    mempool_free(timer_pool, timer);
}

/* Get timer name */
const char* timer_get_name(sw_timer_t *timer) {
    return timer ? timer->name : NULL;
}

/* Get timer period */
uint32_t timer_get_period(sw_timer_t *timer) {
    return timer ? timer->period_ticks : 0;
}

/* Check if timer is active */
uint8_t timer_is_active(sw_timer_t *timer) {
    return (timer && (timer->flags & TIMER_FLAG_ACTIVE)) ? 1 : 0;
}
