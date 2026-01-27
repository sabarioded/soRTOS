#include "semaphore.h"
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

/* Initialize a semaphore */
void so_sem_init(so_sem_t *s, uint32_t initial_count, uint32_t max_count) {
    spinlock_init(&s->lock);
    s->count = initial_count;
    s->max_count = max_count;
    s->wait_head = NULL;
    s->wait_tail = NULL;
}

/* Wait for the semaphore. Block if count is 0. */
void so_sem_wait(so_sem_t *s) {
    task_t *current_task = (task_t*)task_get_current();
    if (!current_task) {
        return;
    }

    /* Reuse the wait node since we can't be blocked on a queue and semaphore simultaneously */
    wait_node_t *node = task_get_wait_node(current_task);
    node->task = current_task;
    node->next = NULL;

    while(1) {
        uint32_t flags = spin_lock(&s->lock);

        /* If resource available, take it */
        if (s->count > 0) {
            s->count--;
            spin_unlock(&s->lock, flags);
            return;
        }

        /* No resource available. Add to wait queue and block */
        _add_to_wait_list(&s->wait_head, &s->wait_tail, node);
        task_set_state(current_task, TASK_BLOCKED);

        spin_unlock(&s->lock, flags);
        
        /* Yield CPU so scheduler can switch to another task */
        platform_yield();
    }
}

/* Give a token. Wake up one waiter if any. */
void so_sem_signal(so_sem_t *s) {
    uint32_t flags = spin_lock(&s->lock);

    /* If tasks are waiting, wake the first one */
    void *task = _pop_from_wait_list(&s->wait_head, &s->wait_tail);
    if (task) {
        task_unblock((task_t*)task);
    }
    
    /* Increment count, but cap at max */
    if (s->count < s->max_count) {
        s->count++;
    }

    spin_unlock(&s->lock, flags);
}

/* Wake up ALL waiting tasks */
void so_sem_broadcast(so_sem_t *s) {
    uint32_t flags = spin_lock(&s->lock);

    /* Wake everyone in the queue */
    while (1) {
        void *task = _pop_from_wait_list(&s->wait_head, &s->wait_tail);
        if (!task) {
            break;
        }
        
        task_unblock((task_t*)task);
        
        /* Increment count for each woken task, capped at max */
        if (s->count < s->max_count) {
            s->count++;
        }
    }

    spin_unlock(&s->lock, flags);
}