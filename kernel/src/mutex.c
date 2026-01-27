#include "mutex.h"
#include "scheduler.h"
#include "arch_ops.h"
#include "platform.h"
#include "spinlock.h"
#include <stddef.h>

/* Add a node to the linked list tail */
static void _add_to_wait_list(wait_node_t **head, wait_node_t **tail, wait_node_t *node) {
    node->next = NULL;
    if (*tail) {
        (*tail)->next = node;
    } else {
        *head = node;
    }
    *tail = node;
}

/* Remove and return the first node from the linked list */
static void* _pop_from_wait_list(wait_node_t **head, wait_node_t **tail) {
    if (!*head) {
        return NULL;
    }
    
    wait_node_t *node = *head;
    void *task = node->task;
    
    *head = node->next;
    if (*head == NULL) {
        *tail = NULL;
    }
    return task;
}

/* Find highest weight among waiting tasks */
static uint8_t _get_max_waiter_weight(wait_node_t *head) {
    uint8_t max_weight = 0;
    wait_node_t *curr = head;
    
    while (curr) {
        task_t *task = (task_t*)curr->task;
        uint8_t w = task_get_weight(task);
        if (w > max_weight) {
            max_weight = w;
        }
        curr = curr->next;
    }
    return max_weight;
}

/* Initialize a mutex structure */
void so_mutex_init(so_mutex_t *m) {
    spinlock_init(&m->lock);
    m->owner = NULL;
    m->wait_head = NULL;
    m->wait_tail = NULL;
}

/* Acquire the lock. If busy, block current task and yield. */
void so_mutex_lock(so_mutex_t *m) {
    task_t *current_task = (task_t*)task_get_current();
    if (!current_task) {
        return;
    }

    /* Reuse the wait node since we can't be blocked on a queue and mutex simultaneously */
    wait_node_t *node = task_get_wait_node(current_task);
    node->task = current_task;
    node->next = NULL;

    while(1) {
        uint32_t flags = spin_lock(&m->lock);
        
        /* Check if we already own it (Recursive/Re-entry) */
        if (m->owner == current_task) {
            spin_unlock(&m->lock, flags);
            return;
        }
        
        /* If unlocked, take ownership */
        if (m->owner == NULL) {
            m->owner = current_task;
            spin_unlock(&m->lock, flags);
            return;
        }

        /* Boost owner if we have higher priority */
        task_t *owner = (task_t*)m->owner;
        uint8_t curr_w = task_get_weight(current_task);
        if (curr_w > task_get_weight(owner)) {
            task_boost_weight(owner, curr_w);
        }

        /* If locked, add to wait queue */
        _add_to_wait_list(&m->wait_head, &m->wait_tail, node);
        task_set_state(current_task, TASK_BLOCKED); 

        spin_unlock(&m->lock, flags);
        
        /* Yield CPU to allow other tasks to run */
        platform_yield();
    }
}

/* Release the lock and wake up one waiting task */
void so_mutex_unlock(so_mutex_t *m) {
    uint32_t flags = spin_lock(&m->lock);

    /* Only the owner can unlock */
    task_t *current = (task_t*)task_get_current();
    if(m->owner != current) {
        spin_unlock(&m->lock, flags);
        return;
    }

    /* Restore original priority */
    task_restore_base_weight(current);

    /* If tasks are waiting, perform direct handoff */
    void *next_task = _pop_from_wait_list(&m->wait_head, &m->wait_tail);
    if (next_task) {
        task_t *next = (task_t*)next_task;
        
        /* Pass ownership directly */
        m->owner = next;
        
        /* Check if new owner needs priority boost from remaining waiters */
        uint8_t max_waiter = _get_max_waiter_weight(m->wait_head);
        if (max_waiter > task_get_weight(next)) {
            task_boost_weight(next, max_waiter);
        }
        
        task_unblock(next);
    } else {
        /* No one waiting, clear ownership */
        m->owner = NULL;
    }

    spin_unlock(&m->lock, flags);
}
