#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

typedef struct heap_stats {
    size_t total_size;
    size_t used_size;
    size_t free_size;
    size_t largest_free_block;
    size_t allocated_blocks;
    size_t free_blocks;
} heap_stats_t;

/**
 * @brief Initializes the memory pool.
 * Sets up the initial free block and aligns the starting address to the 
 * architecture's word boundary. 
 * @param pool Pointer to the raw memory buffer to be managed.
 * @param size Total size of the raw buffer in bytes.
 */
void allocator_init(uint8_t* pool, size_t size);

/**
 * @brief Allocates a block of memory from the pool.
 * Finds the first free block large enough to satisfy the request. 
 * The request is automatically aligned to the system's word size.
 * @param size Number of bytes requested.
 * @return void* Pointer to the allocated memory, or NULL if allocation fails.
 */
void* allocator_malloc(size_t size);

/**
 * @brief Returns a block of memory back to the pool.
 * Marks the block as free and immediately performs merging
 * with adjacent free blocks to prevent fragmentation.
 * @param ptr Pointer to the memory block to be freed. If NULL, does nothing.
 */
void  allocator_free(void* ptr);

/**
 * @brief Resizes an existing memory block.
 * If the new size is smaller, the block remains in place. If larger, a new 
 * block is allocated, data is copied via memcpy, and the old block is freed.
 * @param ptr Pointer to the currently allocated memory.
 * @param new_size Requested new size in bytes.
 * @return void* Pointer to the new memory location, or NULL if it fails.
 */
void* allocator_realloc(void* ptr, size_t new_size);

/**
 * @brief Retrieves the total amount of free memory available for data.
 * @return size_t Total free bytes (excluding internal header overhead).
 */
size_t allocator_get_free_size(void);

/**
 * @brief Counts the number of non-contiguous free blocks in the pool.
 * A high count relative to free_size indicates high memory fragmentation.
 * @return size_t Number of individual free "holes".
 */
size_t allocator_get_fragment_count(void);

/**
 * @brief Check if a pointer falls within the managed heap range.
 * @param ptr Pointer to check.
 * @return 1 if pointer is in heap, 0 otherwise.
 */
int allocator_is_heap_pointer(void *ptr);

/**
 * @brief  Populates the stats structure with current heap state.
 * @param  stats Pointer to a heap_stats_t struct to fill.
 * @return 0 on success, -1 if heap not initialized or stats is NULL.
 */
int allocator_get_stats(heap_stats_t *stats);

/**
 * @brief Verify the integrity of the heap.
 * 
 * @return 0 on success, or a negative error code indicating the type of corruption.
 */
int allocator_check_integrity(void);


#endif