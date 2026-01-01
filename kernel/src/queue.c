#include "queue.h"
#include "allocator.h"
#include "scheduler.h"
#include "arch_ops.h"
#include "utils.h"
#include "platform.h"

/* Create a queue: allocate struct and buffer */
queue_t* queue_create(size_t item_size, size_t capacity) {
    if (item_size == 0 || capacity == 0) return NULL;

    /* Allocate queue control block */
    queue_t *q = (queue_t*)allocator_malloc(sizeof(queue_t));
    if (!q) return NULL;

    /* Allocate data buffer */
    q->buffer = allocator_malloc(item_size * capacity);
    if (!q->buffer) {
        allocator_free(q);
        return NULL;
    }

    q->item_size = item_size;
    q->capacity = capacity;
    q->count = 0;
    q->head = 0;
    q->tail = 0;

    /* Initialize wait queues for blocking tasks */
    q->rx_head = q->rx_tail = q->rx_count = 0;
    q->tx_head = q->tx_tail = q->tx_count = 0;
    for(int i=0; i<MAX_TASKS; i++) {
        q->rx_wait_queue[i] = NULL;
        q->tx_wait_queue[i] = NULL;
    }

    return q;
}

/* Delete queue: free buffer and struct */
void queue_delete(queue_t *q) {
    if (!q) return;
    if (q->buffer) allocator_free(q->buffer);
    allocator_free(q);
}

/* Send item to queue. Block if full. */
int queue_send(queue_t *q, const void *item) {
    if (!q || !item) return -1;

    while (1) {
        uint32_t flags = arch_irq_lock();

        /* Check if there is space */
        if (q->count < q->capacity) {
            /* Copy data to the circular buffer */
            uint8_t *target = (uint8_t*)q->buffer + (q->tail * q->item_size);
            memcpy(target, item, q->item_size);
            q->tail = (q->tail + 1) % q->capacity;
            q->count++;

            /* If a task is waiting to receive, wake it up */
            if (q->rx_count > 0) {
                void *task = q->rx_wait_queue[q->rx_head];
                q->rx_head = (q->rx_head + 1) % MAX_TASKS;
                q->rx_count--;
                task_unblock((task_t*)task);
            }

            arch_irq_unlock(flags);
            return 0;
        }

        /* Queue is full: Add current task to TX wait queue and block */
        if (q->tx_count < MAX_TASKS) {
            q->tx_wait_queue[q->tx_tail] = task_get_current();
            q->tx_tail = (q->tx_tail + 1) % MAX_TASKS;
            q->tx_count++;
            /* Mark as blocked, but yield AFTER unlocking to allow context switch */
            task_set_state((task_t*)task_get_current(), TASK_BLOCKED);
        } else {
            /* Wait queue is full, cannot block */
            arch_irq_unlock(flags);
            return -1; /* Wait queue full */
        }

        arch_irq_unlock(flags);
        /* Yield CPU to allow other tasks to run (and hopefully consume data) */
        platform_yield();
    }
}

/* Receive item from queue. Block if empty. */
int queue_receive(queue_t *q, void *buffer) {
    if (!q || !buffer) return -1;

    while (1) {
        uint32_t flags = arch_irq_lock();

        /* Check if there is data */
        if (q->count > 0) {
            /* Copy data from the circular buffer */
            uint8_t *source = (uint8_t*)q->buffer + (q->head * q->item_size);
            memcpy(buffer, source, q->item_size);
            q->head = (q->head + 1) % q->capacity;
            q->count--;

            /* If a task is waiting to send, wake it up */
            if (q->tx_count > 0) {
                void *task = q->tx_wait_queue[q->tx_head];
                q->tx_head = (q->tx_head + 1) % MAX_TASKS;
                q->tx_count--;
                task_unblock((task_t*)task);
            }

            arch_irq_unlock(flags);
            return 0;
        }

        /* Queue is empty: Add current task to RX wait queue and block */
        if (q->rx_count < MAX_TASKS) {
            q->rx_wait_queue[q->rx_tail] = task_get_current();
            q->rx_tail = (q->rx_tail + 1) % MAX_TASKS;
            q->rx_count++;
            task_set_state((task_t*)task_get_current(), TASK_BLOCKED);
        } else {
            arch_irq_unlock(flags);
            return -1;
        }

        arch_irq_unlock(flags);
        platform_yield();
    }
}

/* Send item from ISR. Non-blocking. */
int queue_send_from_isr(queue_t *q, const void *item) {
    if (!q || !item) return -1;

    uint32_t flags = arch_irq_lock();

    /* Check space immediately */
    if (q->count < q->capacity) {
        /* Copy data */
        uint8_t *target = (uint8_t*)q->buffer + (q->tail * q->item_size);
        memcpy(target, item, q->item_size);
        q->tail = (q->tail + 1) % q->capacity;
        q->count++;

        /* Wake up a waiting receiver if any */
        if (q->rx_count > 0) {
            void *task = q->rx_wait_queue[q->rx_head];
            q->rx_head = (q->rx_head + 1) % MAX_TASKS;
            q->rx_count--;
            task_unblock((task_t*)task);
        }

        arch_irq_unlock(flags);
        return 0;
    }

    arch_irq_unlock(flags);
    return -1; /* Queue full */
}

/* Peek at the next item without removing it. Non-blocking. */
int queue_peek(queue_t *q, void *buffer) {
    if (!q || !buffer) return -1;

    uint32_t flags = arch_irq_lock();

    if (q->count > 0) {
        /* Copy data from the circular buffer */
        uint8_t *source = (uint8_t*)q->buffer + (q->head * q->item_size);
        memcpy(buffer, source, q->item_size);
        
        arch_irq_unlock(flags);
        return 0;
    }

    arch_irq_unlock(flags);
    return -1;
}

/* Reset queue: clear data and wake writers */
void queue_reset(queue_t *q) {
    if (!q) return;

    uint32_t flags = arch_irq_lock();

    q->head = 0;
    q->tail = 0;
    q->count = 0;

    /* Wake up all tasks waiting to send, as the queue is now empty */
    while(q->tx_count > 0) {
        void *task = q->tx_wait_queue[q->tx_head];
        q->tx_head = (q->tx_head + 1) % MAX_TASKS;
        q->tx_count--;
        task_unblock((task_t*)task);
    }

    arch_irq_unlock(flags);
}