#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Simple free-list heap allocator for embedded systems
 * 
 * Features:
 * - First-fit allocation algorithm
 * - Free list to track free blocks
 * - Block headers for size tracking
 * - Alignment to 8-byte boundaries
 * - Heap fragmentation monitoring
 */

/* Heap configuration */
#define HEAP_ALIGNMENT        8U
#define HEAP_MIN_BLOCK_SIZE   16U  /* Minimum allocatable block size */

/* Block header structure (8 bytes) */
typedef struct heap_block {
    uint32_t size;              /* Size of block (including header, excluding next) */
    struct heap_block *next;    /* Pointer to next free block (only valid when free) */
    /* Data follows immediately after this header */
} heap_block_t;

/* Heap statistics */
typedef struct {
    size_t total_size;          /* Total heap size */
    size_t free_size;           /* Total free memory */
    size_t used_size;           /* Total used memory */
    size_t largest_free_block;  /* Size of largest free block */
    uint32_t block_count;       /* Number of allocated blocks */
    uint32_t fragment_count;    /* Number of free block fragments */
} heap_stats_t;

/**
 * @brief Initialize the heap allocator
 * 
 * Must be called before using malloc/free. Sets up the free list
 * with a single large block covering the entire heap region.
 * 
 * @param heap_start Start address of heap (must be 8-byte aligned)
 * @param heap_size  Size of heap in bytes (will be aligned down to multiple of 8)
 * @return 0 on success, -1 on failure
 */
int heap_init(void *heap_start, size_t heap_size);

/**
 * @brief Allocate memory from the heap
 * 
 * @param size Number of bytes to allocate (will be rounded up to alignment)
 * @return Pointer to allocated memory, or NULL on failure
 */
void* heap_malloc(size_t size);

/**
 * @brief Free previously allocated memory
 * 
 * @param ptr Pointer to memory previously allocated with heap_malloc()
 */
void heap_free(void *ptr);

/**
 * @brief Reallocate memory (change size of existing allocation)
 * 
 * @param ptr  Pointer to existing allocation (can be NULL)
 * @param size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
void* heap_realloc(void *ptr, size_t size);

/**
 * @brief Get heap statistics
 * 
 * @param stats Pointer to structure to fill with statistics
 * @return 0 on success, -1 if heap not initialized
 */
int heap_get_stats(heap_stats_t *stats);

/**
 * @brief Check heap integrity (walk free list and verify headers)
 * 
 * Useful for debugging heap corruption issues.
 * 
 * @return 0 if heap is valid, -1 if corruption detected
 */
int heap_check_integrity(void);

/**
 * @brief Get the start address of the heap
 * 
 * @return Start address, or NULL if not initialized
 */
void* heap_get_start(void);

/**
 * @brief Get the end address of the heap
 * 
 * @return End address, or NULL if not initialized
 */
void* heap_get_end(void);

#ifdef __cplusplus
}
#endif

#endif /* HEAP_H */

