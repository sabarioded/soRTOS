#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include "project_config.h"

typedef struct queue queue_t;

/* Callback function type for queue events */
typedef void (*queue_notify_cb_t)(void *arg);

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
 * @brief Push an item to the queue (Blocking).
 * 
 * Copies the item into the queue. If the queue is full, the calling task
 * blocks until space becomes available.
 * 
 * @param q Pointer to the queue.
 * @param item Pointer to the data to copy into the queue.
 * @return 0 on success, -1 on error (e.g. invalid queue).
 */
int queue_push(queue_t *q, const void *item);

/**
 * @brief Push multiple items to the queue (Blocking).
 * 
 * Writes 'count' items to the queue. If the queue fills up, it blocks
 * until space is available, then continues writing.
 * 
 * @param q Pointer to the queue.
 * @param data Pointer to the array of items to write.
 * @param count Number of items to write.
 * @return 0 on success, -1 on error.
 */
int queue_push_arr(queue_t *q, const void *data, size_t count);

/**
 * @brief Pop an item from the queue (Blocking).
 * 
 * Copies an item from the queue into the provided buffer. If the queue is empty,
 * the calling task blocks until data arrives.
 * 
 * @param q Pointer to the queue.
 * @param buffer Pointer to the memory where the received item will be copied.
 * @return 0 on success, -1 on error.
 */
int queue_pop(queue_t *q, void *buffer);

/**
 * @brief Push an item to the queue from an Interrupt Service Routine (Non-Blocking).
 * 
 * Copies the item into the queue if space is available. If the queue is full,
 * returns immediately with an error. Does NOT block.
 * 
 * @param q Pointer to the queue.
 * @param item Pointer to the data to copy.
 * @return 0 on success, -1 if queue is full or invalid.
 */
int queue_push_from_isr(queue_t *q, const void *item);

/**
 * @brief Pop an item from the queue from an Interrupt Service Routine (Non-Blocking).
 * 
 * Copies an item from the queue into the provided buffer. If the queue is empty,
 * returns immediately with an error. Does NOT block.
 * 
 * @param q Pointer to the queue.
 * @param buffer Pointer to the memory where the received item will be copied.
 * @return 0 on success, -1 if queue is empty or invalid.
 */
int queue_pop_from_isr(queue_t *q, void *buffer);

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

/**
 * @brief Register a callback to be called when data is pushed to the queue.
 * 
 * @param q Pointer to the queue.
 * @param cb Function to call.
 * @param arg Argument to pass to the function.
 */
void queue_set_push_callback(queue_t *q, queue_notify_cb_t cb, void *arg);

#endif /* QUEUE_H */