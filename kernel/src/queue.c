#include "queue.h"
#include "allocator.h"
#include "scheduler.h"
#include "arch_ops.h"
#include "utils.h"
#include "platform.h"
#include "spinlock.h"
#include "logger.h"

typedef struct wait_node {
    void *task;
    struct wait_node *next;
} wait_node_t;

struct queue {
    void *buffer;                   /* Pointer to the allocated data storage */
    size_t item_size;               /* Size of a single item in bytes */
    size_t capacity;                /* Maximum number of items the queue can hold */
    size_t count;                   /* Current number of items in the queue */
    size_t head;                    /* Read index (where to take next) */
    size_t tail;                    /* Write index (where to put next) */
    
    /* Wait queues */
    wait_node_t *rx_wait_head;      /* Head of RX wait list */
    wait_node_t *rx_wait_tail;      /* Tail of RX wait list */
    
    wait_node_t *tx_wait_head;      /* Head of TX wait list */
    wait_node_t *tx_wait_tail;      /* Tail of TX wait list */

    /* Notification Callback */
    queue_notify_cb_t callback;      /* Function to call when data is added */
    void *callback_arg;              /* Argument for the callback */

    spinlock_t lock;                 /* Queue-specific lock */
};

/* Add task to wait list */
static void _add_to_wait_list(wait_node_t **head, wait_node_t **tail, wait_node_t *node) {
    node->next = NULL;
    
    if (*tail) {
        (*tail)->next = node;
    } else {
        *head = node;
    }
    *tail = node;
}

/* Remove and return first task from wait list */
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

/* Remove specific task from wait list */
static void _remove_task_from_list(wait_node_t **head, wait_node_t **tail, void *task) {
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

/* Create a queue, allocate struct and buffer */
queue_t* queue_create(size_t item_size, size_t capacity) {
    if (item_size == 0 || capacity == 0){
        return NULL;
    }

    /* Allocate queue control block */
    queue_t *q = (queue_t*)allocator_malloc(sizeof(queue_t));
    if (!q) {
        return NULL;
    }

    /* Allocate data buffer */
    q->buffer = allocator_malloc(item_size * capacity);
    if (!q->buffer) {
        allocator_free(q);
#if LOG_ENABLE
        logger_log("Queue Create Fail", 0, 0);
#endif
        return NULL;
    }

    q->item_size = item_size;
    q->capacity = capacity;
    q->count = 0;
    q->head = 0;
    q->tail = 0;

    /* Initialize wait queues for blocking tasks */
    q->rx_wait_head = NULL;
    q->rx_wait_tail = NULL;
    
    q->tx_wait_head = NULL;
    q->tx_wait_tail = NULL;

    q->callback = NULL;
    q->callback_arg = NULL;

    spinlock_init(&q->lock);
    return q;
}

/* Delete queue, free buffer and struct */
void queue_delete(queue_t *q) {
    if (!q) {
        return;
    }
    
    /* Free any remaining wait nodes */
    while (q->rx_wait_head) {
        _pop_from_wait_list(&q->rx_wait_head, &q->rx_wait_tail);
    }
    while (q->tx_wait_head) {
        _pop_from_wait_list(&q->tx_wait_head, &q->tx_wait_tail);
    }

    if (q->buffer) {
        allocator_free(q->buffer);
    }
    allocator_free(q);
}

/* Send item to queue. block if full. */
int queue_send(queue_t *q, const void *item) {
    if (!q || !item) {
        return -1;
    }
    
    wait_node_t node;
    node.task = task_get_current();

    while (1) {
        uint32_t flags = spin_lock(&q->lock);

        /* Check if there is space */
        if (q->count < q->capacity) {
            /* Copy data to the circular buffer */
            uint8_t *target = (uint8_t*)q->buffer + (q->tail * q->item_size);
            utils_memcpy(target, item, q->item_size);
            q->tail = (q->tail + 1) % q->capacity;
            q->count++;

            /* If a task is waiting to receive, wake it up */
            void *task = _pop_from_wait_list(&q->rx_wait_head, &q->rx_wait_tail);
            if (task) {
                task_unblock((task_t*)task);
            }

            /* Notify callback if registered */
            if (q->callback) {
                q->callback(q->callback_arg);
            }

            spin_unlock(&q->lock, flags);
            return 0;
        }

        /* Queue is full, add current task to TX wait queue and block */
        
        /* Ensure we aren't already in the list */
        _remove_task_from_list(&q->tx_wait_head, &q->tx_wait_tail, node.task);
        
        _add_to_wait_list(&q->tx_wait_head, &q->tx_wait_tail, &node);
        
        task_set_state((task_t*)node.task, TASK_BLOCKED);

        spin_unlock(&q->lock, flags);
        /* Yield CPU to allow other tasks to run (and hopefully consume data) */
        platform_yield();
    }
}

/* Receive item from queue. Block if empty. */
int queue_receive(queue_t *q, void *buffer) {
    if (!q || !buffer) {
        return -1;
    }

    wait_node_t node;
    node.task = task_get_current();

    while (1) {
        uint32_t flags = spin_lock(&q->lock);

        /* Check if there is data */
        if (q->count > 0) {
            /* Copy data from the circular buffer */
            uint8_t *source = (uint8_t*)q->buffer + (q->head * q->item_size);
            utils_memcpy(buffer, source, q->item_size);
            q->head = (q->head + 1) % q->capacity;
            q->count--;

            /* If a task is waiting to send, wake it up */
            void *task = _pop_from_wait_list(&q->tx_wait_head, &q->tx_wait_tail);
            if (task) {
                task_unblock((task_t*)task);
            }

            spin_unlock(&q->lock, flags);
            return 0;
        }

        /* Queue is empty, add current task to RX wait queue and block */
        _remove_task_from_list(&q->rx_wait_head, &q->rx_wait_tail, node.task);

        _add_to_wait_list(&q->rx_wait_head, &q->rx_wait_tail, &node);
        
        task_set_state((task_t*)node.task, TASK_BLOCKED);

        spin_unlock(&q->lock, flags);
        platform_yield();
    }
}

/* Send item from ISR. Non-blocking. */
int queue_send_from_isr(queue_t *q, const void *item) {
    if (!q || !item) {
        return -1;
    }

    uint32_t flags = spin_lock(&q->lock);

    /* Check space immediately */
    if (q->count < q->capacity) {
        /* Copy data */
        uint8_t *target = (uint8_t*)q->buffer + (q->tail * q->item_size);
        utils_memcpy(target, item, q->item_size);
        q->tail = (q->tail + 1) % q->capacity;
        q->count++;

        /* Wake up a waiting receiver if any */
        void *task = _pop_from_wait_list(&q->rx_wait_head, &q->rx_wait_tail);
        if (task) {
            task_unblock((task_t*)task);
        }

        /* Notify callback if registered */
        if (q->callback) {
            q->callback(q->callback_arg);
        }

        spin_unlock(&q->lock, flags);
        return 0;
    }

    spin_unlock(&q->lock, flags);
    return -1; /* Queue full */
}

/* Receive item from ISR. Non-blocking. */
int queue_receive_from_isr(queue_t *q, void *buffer) {
    if (!q || !buffer) {
        return -1;
    }

    uint32_t flags = spin_lock(&q->lock);

    if (q->count > 0) {
        /* Copy data from the circular buffer */
        uint8_t *source = (uint8_t*)q->buffer + (q->head * q->item_size);
        utils_memcpy(buffer, source, q->item_size);
        q->head = (q->head + 1) % q->capacity;
        q->count--;

        /* If a task is waiting to send, wake it up */
        void *task = _pop_from_wait_list(&q->tx_wait_head, &q->tx_wait_tail);
        if (task) {
            task_unblock((task_t*)task);
        }

        spin_unlock(&q->lock, flags);
        return 0;
    }

    spin_unlock(&q->lock, flags);
    return -1;
}

/* Peek at the next item without removing it. Non-blocking. */
int queue_peek(queue_t *q, void *buffer) {
    if (!q || !buffer) {
        return -1;
    }

    uint32_t flags = spin_lock(&q->lock);

    if (q->count > 0) {
        /* Copy data from the circular buffer */
        uint8_t *source = (uint8_t*)q->buffer + (q->head * q->item_size);
        utils_memcpy(buffer, source, q->item_size);
        
        spin_unlock(&q->lock, flags);
        return 0;
    }

    spin_unlock(&q->lock, flags);
    return -1;
}

/* Reset queue: clear data and wake writers */
void queue_reset(queue_t *q) {
    if (!q) {
        return;
    }

    uint32_t flags = spin_lock(&q->lock);

    q->head = 0;
    q->tail = 0;
    q->count = 0;

    /* Wake up all tasks waiting to send, as the queue is now empty */
    while (1) {
        void *task = _pop_from_wait_list(&q->tx_wait_head, &q->tx_wait_tail);
        if (!task) break;
        task_unblock((task_t*)task);
    }

    spin_unlock(&q->lock, flags);
#if LOG_ENABLE
    logger_log("Queue Reset", 0, 0);
#endif
}

/* Register a push callback */
void queue_set_push_callback(queue_t *q, queue_notify_cb_t cb, void *arg) {
    if (!q || !cb) {
        return;
    }
    uint32_t flags = spin_lock(&q->lock);
    q->callback = cb;
    q->callback_arg = arg;
    spin_unlock(&q->lock, flags);
}
