#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include "project_config.h"

/**
 * @brief Thread-safe Message Queue Structure.
 * 
 * Used for inter-task communication and synchronization.
 * Supports blocking send/receive operations and ISR-safe sending.
 */
typedef struct {
    void *buffer;           /**< Pointer to the allocated data storage */
    size_t item_size;       /**< Size of a single item in bytes */
    size_t capacity;        /**< Maximum number of items the queue can hold */
    size_t count;           /**< Current number of items in the queue */
    size_t head;            /**< Read index (where to take next) */
    size_t tail;            /**< Write index (where to put next) */
    
    /* Wait queues */
    void *rx_wait_queue[MAX_TASKS]; /**< List of tasks waiting to receive data */
    uint8_t rx_head;                /**< Head of RX wait queue */
    uint8_t rx_tail;                /**< Tail of RX wait queue */
    uint8_t rx_count;               /**< Number of tasks waiting to receive */
    
    void *tx_wait_queue[MAX_TASKS]; /**< List of tasks waiting to send data */
    uint8_t tx_head;                /**< Head of TX wait queue */
    uint8_t tx_tail;                /**< Tail of TX wait queue */
    uint8_t tx_count;               /**< Number of tasks waiting to send */
} queue_t;

/**
 * @brief Create a new message queue.
 * 
 * Allocates memory for the queue structure and the internal data buffer.
 * 
 * @param item_size Size of each message item in bytes.
 * @param capacity Maximum number of items the queue can hold.
 * @return Pointer to the created queue, or NULL if allocation failed.
 */
queue_t* queue_create(size_t item_size, size_t capacity);

/**
 * @brief Delete a queue and free its resources.
 * 
 * Frees the internal buffer and the queue structure itself.
 * @warning Do not use the queue handle after calling this.
 * 
 * @param q Pointer to the queue to delete.
 */
void queue_delete(queue_t *q);

/**
 * @brief Send an item to the queue (Blocking).
 * 
 * Copies the item into the queue. If the queue is full, the calling task
 * blocks until space becomes available.
 * 
 * @param q Pointer to the queue.
 * @param item Pointer to the data to copy into the queue.
 * @return 0 on success, -1 on error (e.g. invalid queue).
 */
int queue_send(queue_t *q, const void *item);

/**
 * @brief Receive an item from the queue (Blocking).
 * 
 * Copies an item from the queue into the provided buffer. If the queue is empty,
 * the calling task blocks until data arrives.
 * 
 * @param q Pointer to the queue.
 * @param buffer Pointer to the memory where the received item will be copied.
 * @return 0 on success, -1 on error.
 */
int queue_receive(queue_t *q, void *buffer);

/**
 * @brief Send an item to the queue from an Interrupt Service Routine (Non-Blocking).
 * 
 * Copies the item into the queue if space is available. If the queue is full,
 * returns immediately with an error. Does NOT block.
 * 
 * @param q Pointer to the queue.
 * @param item Pointer to the data to copy.
 * @return 0 on success, -1 if queue is full or invalid.
 */
int queue_send_from_isr(queue_t *q, const void *item);

/**
 * @brief Peek at the item at the head of the queue without removing it.
 * 
 * Copies the item at the head of the queue into the provided buffer.
 * Does not modify the queue state (count remains the same).
 * 
 * @param q Pointer to the queue.
 * @param buffer Pointer to the memory where the item will be copied.
 * @return 0 on success, -1 if queue is empty or invalid.
 */
int queue_peek(queue_t *q, void *buffer);

/**
 * @brief Reset the queue to an empty state.
 * 
 * Discards all data currently in the queue.
 * Wakes up any tasks waiting to send data (since space is now available).
 * 
 * @param q Pointer to the queue.
 */
void queue_reset(queue_t *q);

#endif /* QUEUE_H */