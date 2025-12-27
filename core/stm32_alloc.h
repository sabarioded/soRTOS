#ifndef STM32_ALLOC_H
#define STM32_ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include "allocator.h"

/**
 * @file stm32_alloc.h
 * @brief Thread-safe wrapper for the block allocator using ARM Cortex-M BASEPRI.
 * * This layer ensures that memory operations are atomic, preventing heap 
 * corruption when malloc/free are called from different interrupt priorities.
 */

void  stm32_allocator_init(uint8_t* pool, size_t size);
void* stm32_allocator_malloc(size_t size);
void  stm32_allocator_free(void* ptr);
void* stm32_allocator_realloc(void* ptr, size_t new_size);
size_t stm32_allocator_get_free_size(void);
size_t stm32_allocator_get_fragment_count(void);
int  stm32_allocator_get_stats(heap_stats_t *stats);
int stm32_allocator_check_integrity(void);

#endif /* STM32_ALLOC_H */