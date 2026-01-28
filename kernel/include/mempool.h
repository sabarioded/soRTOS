#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mempool mempool_t;

/**
 * @brief Create a fixed-size memory pool.
 * 
 * @param item_size Size of each item in bytes.
 * @param count Number of items in the pool.
 * @return Pointer to the pool handle, or NULL on failure.
 */
mempool_t* mempool_create(size_t item_size, size_t count);

/**
 * @brief Allocate an item from the pool.
 * 
 * @param pool Pointer to the memory pool.
 * @return Pointer to the allocated memory, or NULL if pool is empty.
 */
void* mempool_alloc(mempool_t *pool);

/**
 * @brief Return an item to the pool.
 * 
 * @param pool Pointer to the memory pool.
 * @param ptr Pointer to the memory to free.
 */
void mempool_free(mempool_t *pool, void *ptr);

/**
 * @brief Delete the pool and free all resources.
 * @param pool Pointer to the memory pool.
 */
void mempool_delete(mempool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* MEMPOOL_H */