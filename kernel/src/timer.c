#include "timer.h"
#include "scheduler.h"
#include "allocator.h"
#include "arch_ops.h"
#include "platform.h"
#include "utils.h"

static sw_timer_t *timer_list_head = NULL;
static uint16_t timer_task_id = 0;

/* Insert timer into sorted linked list (sorted by expiry_tick) */
static void timer_insert(sw_timer_t *tmr) {
    sw_timer_t **curr = &timer_list_head;
    
    /* Find insertion point */
    while (*curr != NULL) {
        /* Handle wrap-around cases for ticks if necessary, 
           but for simplicity assuming standard comparison works for near-term */
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

/* The Daemon Task */
static void timer_task_entry(void *arg) {
    (void)arg;
    
    while (1) {
        uint32_t next_wake = UINT32_MAX;
        uint32_t now = (uint32_t)platform_get_ticks();
        
        uint32_t flags = arch_irq_lock();
        sw_timer_t *curr = timer_list_head;
        
        if (curr != NULL) {
            /* Check if expired */
            if ((int32_t)(now - curr->expiry_tick) >= 0) {
                /* Timer expired! */
                sw_timer_t *tmr = curr;
                
                /* Remove from list */
                timer_list_head = tmr->next;
                tmr->next = NULL;
                tmr->active = 0;
                
                /* If auto-reload, restart it */
                if (tmr->auto_reload) {
                    tmr->expiry_tick = now + tmr->period_ticks;
                    tmr->active = 1;
                    timer_insert(tmr);
                }
                
                arch_irq_unlock(flags);
                
                /* Execute callback (outside critical section) */
                if (tmr->callback) {
                    tmr->callback(tmr->arg);
                }
                
                /* Loop again immediately to check next timer */
                continue;
            } else {
                /* Not expired yet, calculate wait time */
                next_wake = curr->expiry_tick - now;
            }
        }
        arch_irq_unlock(flags);
        
        /* Wait for next expiry OR a notification (new timer added) */
        task_notify_wait(1, next_wake);
    }
}

void timer_service_init(void) {
    /* Create the timer daemon task with high priority (conceptually) */
    int32_t id = task_create(timer_task_entry, NULL, STACK_SIZE_1KB, TASK_WEIGHT_HIGH);
    if (id > 0) {
        timer_task_id = (uint16_t)id;
    }
}

sw_timer_t* timer_create(const char *name, uint32_t period_ticks, uint8_t auto_reload, timer_callback_t callback, void *arg) {
    sw_timer_t *tmr = (sw_timer_t*)allocator_malloc(sizeof(sw_timer_t));
    if (!tmr) return NULL;
    
    tmr->name = name;
    tmr->period_ticks = period_ticks;
    tmr->auto_reload = auto_reload;
    tmr->callback = callback;
    tmr->arg = arg;
    tmr->active = 0;
    tmr->next = NULL;
    
    return tmr;
}

int timer_start(sw_timer_t *timer) {
    if (!timer) return -1;
    
    uint32_t flags = arch_irq_lock();
    
    if (timer->active) {
        timer_remove(timer);
    }
    
    timer->expiry_tick = (uint32_t)platform_get_ticks() + timer->period_ticks;
    timer->active = 1;
    
    /* Check if this new timer is the earliest one */
    uint8_t is_head = 0;
    if (timer_list_head == NULL || (int32_t)(timer->expiry_tick - timer_list_head->expiry_tick) < 0) {
        is_head = 1;
    }
    
    timer_insert(timer);
    
    arch_irq_unlock(flags);
    
    /* If we inserted at the head, wake up the timer task so it can reschedule */
    if (is_head && timer_task_id != 0) {
        task_notify(timer_task_id, 1);
    }
    
    return 0;
}

int timer_stop(sw_timer_t *timer) {
    if (!timer) return -1;
    
    uint32_t flags = arch_irq_lock();
    
    if (timer->active) {
        timer_remove(timer);
        timer->active = 0;
    }
    
    arch_irq_unlock(flags);
    return 0;
}

void timer_delete(sw_timer_t *timer) {
    if (!timer) return;
    
    timer_stop(timer);
    allocator_free(timer);
}

#ifdef UNIT_TESTING
/* 
 * Test Helper: Manually run the timer check loop.
 * This simulates what timer_task_entry does, but allows us to control time 
 * and execution in a single-threaded test environment.
 */
void timer_test_run_callbacks(void) {
    uint32_t now = (uint32_t)platform_get_ticks();
    uint32_t flags = arch_irq_lock();
    sw_timer_t *curr = timer_list_head;
    
    while (curr != NULL) {
        if ((int32_t)(now - curr->expiry_tick) >= 0) {
            sw_timer_t *tmr = curr;
            
            /* Remove from list */
            timer_list_head = tmr->next;
            tmr->next = NULL;
            tmr->active = 0;
            
            /* If auto-reload, restart it */
            if (tmr->auto_reload) {
                tmr->expiry_tick = now + tmr->period_ticks;
                tmr->active = 1;
                timer_insert(tmr);
                /* List changed, restart check from head to be safe/simple */
                curr = timer_list_head; 
            } else {
                curr = timer_list_head;
            }
            
            arch_irq_unlock(flags);
            
            /* Execute callback */
            if (tmr->callback) {
                tmr->callback(tmr->arg);
            }
            
            /* Re-lock for next iteration */
            flags = arch_irq_lock();
        } else {
            /* List is sorted, so we can stop */
            break; 
        }
    }
    arch_irq_unlock(flags);
}
#endif