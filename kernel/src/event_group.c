#include "event_group.h"
#include "scheduler.h"
#include "platform.h"
#include "spinlock.h"
#include <stddef.h>
#include "allocator.h"

struct event_group {
    wait_node_t     *wait_head;     /* Tasks waiting for events */
    wait_node_t     *wait_tail;
    uint32_t        bits;           /* Current event bits */
    spinlock_t      lock;
};

#define EVENT_SATISFIED_FLAG 0x80

/* Add to wait list */
static void _add_waiter(wait_node_t **head, wait_node_t **tail, wait_node_t *node) {
    node->next = NULL;
    if (*tail) {
        (*tail)->next = node;
    } else {
        *head = node;
    }
    *tail = node;
}

/* Remove from wait list */
static void _remove_waiter(wait_node_t **head, wait_node_t **tail, void *task) {
    wait_node_t *curr = *head;
    wait_node_t *prev = NULL;
    
    while (curr) {
        if (curr->task == task) {
            if (prev) {
                prev->next = curr->next;
            } else {
                *head = curr->next;
            }
            if (curr == *tail) {
                *tail = prev;
            }
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

/* Check if condition is satisfied and wake tasks */
static void _check_and_wake_tasks(event_group_t *eg) {
    wait_node_t *curr = eg->wait_head;
    wait_node_t *prev = NULL;
    
    while (curr) {
        task_t *task = (task_t*)curr->task;
        uint32_t bits_to_wait = task_get_event_bits(task);
        uint8_t flags = task_get_event_flags(task);
        uint8_t wait_all = (flags & EVENT_WAIT_ALL);
        
        uint8_t satisfied = 0;
        
        if (wait_all) {
            /* All bits must be set */
            if ((eg->bits & bits_to_wait) == bits_to_wait) {
                satisfied = 1;
            }
        } else {
            /* Any bit can satisfy */
            if (eg->bits & bits_to_wait) {
                satisfied = 1;
            }
        }
        
        if (satisfied) {
            /* Remove from list */
            wait_node_t *to_wake = curr;
            curr = curr->next;
            
            if (prev) {
                prev->next = to_wake->next;
            } else {
                eg->wait_head = to_wake->next;
            }
            
            if (to_wake == eg->wait_tail) {
                eg->wait_tail = prev;
            }
            
            /* Clear bits if requested */
            if (flags & EVENT_CLEAR_ON_EXIT) {
                eg->bits &= ~bits_to_wait;
            }
            
            /* Store result and mark satisfied */
            task_set_event_wait(task, eg->bits, flags | EVENT_SATISFIED_FLAG);
            task_unblock(task);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

/* Create an event group */
event_group_t* event_group_create(void) {
    event_group_t *eg = (event_group_t*)allocator_malloc(sizeof(struct event_group));
    if (eg) {
        eg->bits = 0;
        eg->wait_head = NULL;
        eg->wait_tail = NULL;
        spinlock_init(&eg->lock);
    }
    return eg;
}

/* Delete an event group */
void event_group_delete(event_group_t *eg) {
    if (!eg) {
        return;
    }
    
    /* Wake up any tasks waiting on this group before deleting */
    uint32_t flags = spin_lock(&eg->lock);
    while (eg->wait_head) {
        wait_node_t *node = eg->wait_head;
        eg->wait_head = node->next;
        task_unblock((task_t*)node->task);
    }
    spin_unlock(&eg->lock, flags);
    
    allocator_free(eg);
}

/* Set event bits */
uint32_t event_group_set_bits(event_group_t *eg, uint32_t bits_to_set) {
    uint32_t flags = spin_lock(&eg->lock);
    
    eg->bits |= bits_to_set;
    uint32_t result = eg->bits;
    
    /* Check if any waiting tasks can be woken */
    _check_and_wake_tasks(eg);
    
    spin_unlock(&eg->lock, flags);
    return result;
}

/* Set bits from ISR */
uint32_t event_group_set_bits_from_isr(event_group_t *eg, uint32_t bits_to_set) {
    /* At the moment its the same as the none ISR function. Should be optimized */
    uint32_t flags = spin_lock(&eg->lock);

    eg->bits |= bits_to_set;
    uint32_t result = eg->bits;

    /* Check if any waiting tasks can be woken */
    _check_and_wake_tasks(eg);

    spin_unlock(&eg->lock, flags);
    return result;
}

/* Clear event bits */
uint32_t event_group_clear_bits(event_group_t *eg, uint32_t bits_to_clear) {
    uint32_t flags = spin_lock(&eg->lock);
    
    eg->bits &= ~bits_to_clear;
    uint32_t result = eg->bits;
    
    spin_unlock(&eg->lock, flags);
    return result;
}

/* Wait for event bits */
uint32_t event_group_wait_bits(event_group_t *eg, 
                               uint32_t bits_to_wait, 
                               uint8_t flags_param, 
                               uint32_t timeout_ticks) {
    task_t *current = (task_t*)task_get_current();
    if (!current) {
        return 0;
    }
    
    wait_node_t *node = task_get_wait_node(current);
    node->task = current;
    node->next = NULL;
    
    uint8_t wait_all = (flags_param & EVENT_WAIT_ALL) ? 1 : 0;
    uint8_t clear_on_exit = (flags_param & EVENT_CLEAR_ON_EXIT) ? 1 : 0;
    
    /* Store wait parameters */
    task_set_event_wait(current, bits_to_wait, flags_param);
    
    uint32_t flags = spin_lock(&eg->lock);
    
    /* Check if condition already satisfied */
    uint8_t satisfied = 0;
    if (wait_all) {
        if ((eg->bits & bits_to_wait) == bits_to_wait) {
            satisfied = 1;
        }
    } else {
        if (eg->bits & bits_to_wait) {
            satisfied = 1;
        }
    }
    
    if (satisfied) {
        uint32_t result = eg->bits;
        if (clear_on_exit) {
            eg->bits &= ~bits_to_wait;
        }
        spin_unlock(&eg->lock, flags);
        return result;
    }
    
    /* Not satisfied. need to wait */
    _add_waiter(&eg->wait_head, &eg->wait_tail, node);
    
    /* If timeout is 0, don't block. Just check and return. */
    if (timeout_ticks == 0) {
        spin_unlock(&eg->lock, flags);
        return eg->bits;
    }
    
    /* Handle blocking atomically for infinite wait to prevent lost wakeups */
    if (timeout_ticks > 0 && timeout_ticks != UINT32_MAX) {
        spin_unlock(&eg->lock, flags);
        task_sleep_ticks(timeout_ticks);
    } else {
        /* Mark as blocked BEFORE unlocking to ensure we don't miss the wakeup 
         * if an ISR fires immediately after unlock. */
        task_set_state(current, TASK_BLOCKED);
        spin_unlock(&eg->lock, flags);
        
        platform_yield();
    }
    
    /* Woken up. Check if we got the bits or timed out */
    flags = spin_lock(&eg->lock);
    
    uint32_t result = 0;
    
    uint8_t out_flags = task_get_event_flags(current);
    if (out_flags & EVENT_SATISFIED_FLAG) {
        result = task_get_event_bits(current);
    } else {
        /* Timed out. Remove from wait list manually */
        _remove_waiter(&eg->wait_head, &eg->wait_tail, current);
    }
    
    spin_unlock(&eg->lock, flags);
    return result;
}

/* Get current bits */
uint32_t event_group_get_bits(event_group_t *eg) {
    uint32_t flags = spin_lock(&eg->lock);
    uint32_t result = eg->bits;
    spin_unlock(&eg->lock, flags);
    return result;
}
