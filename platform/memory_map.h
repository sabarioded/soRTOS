#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <stddef.h>

/**
 * @brief Initialize the Kernel Allocator with platform-specific heap regions.
 */
void memory_map_init(void);

/**
 * @brief Get the start address of the heap.
 */
void* memory_map_get_heap_start(void);

/**
 * @brief Get the total size of the heap in bytes.
 */
size_t memory_map_get_heap_size(void);

#endif /* MEMORY_MAP_H */