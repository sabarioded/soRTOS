#include "heap.h"
#include "utils.h"
#include <string.h>

/* Heap region */
static void *heap_start = NULL;
static void *heap_end = NULL;
static size_t heap_size = 0;
static heap_block_t *free_list = NULL;  /* Head of free block list */

/* Internal helper functions */
static heap_block_t* get_block_from_ptr(void *ptr);
static void* get_ptr_from_block(heap_block_t *block);
static size_t align_size(size_t size);
static void insert_free_block(heap_block_t *block);
static void remove_free_block(heap_block_t *block);
static heap_block_t* merge_adjacent_free(heap_block_t *block);

/**
 * @brief Get block header from data pointer
 */
static heap_block_t* get_block_from_ptr(void *ptr)
{
    if (ptr == NULL) {
        return NULL;
    }
    return (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
}

/**
 * @brief Get data pointer from block header
 */
static void* get_ptr_from_block(heap_block_t *block)
{
    if (block == NULL) {
        return NULL;
    }
    return (uint8_t*)block + sizeof(heap_block_t);
}

/**
 * @brief Align size up to HEAP_ALIGNMENT boundary
 */
static size_t align_size(size_t size)
{
    if (size == 0) {
        size = HEAP_MIN_BLOCK_SIZE;
    }
    /* Round up to alignment boundary */
    size = (size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
    /* Ensure minimum block size (header + some data) */
    if (size < HEAP_MIN_BLOCK_SIZE) {
        size = HEAP_MIN_BLOCK_SIZE;
    }
    return size;
}

/**
 * @brief Insert a block into the free list (sorted by address)
 */
static void insert_free_block(heap_block_t *block)
{
    if (block == NULL) {
        return;
    }

    /* If list is empty or block goes before head */
    if (free_list == NULL || (uint8_t*)block < (uint8_t*)free_list) {
        block->next = free_list;
        free_list = block;
        return;
    }

    /* Find insertion point (sorted by address) */
    heap_block_t *current = free_list;
    while (current->next != NULL && (uint8_t*)current->next < (uint8_t*)block) {
        current = current->next;
    }

    block->next = current->next;
    current->next = block;
}

/**
 * @brief Remove a block from the free list
 */
static void remove_free_block(heap_block_t *block)
{
    if (block == NULL || free_list == NULL) {
        return;
    }

    if (free_list == block) {
        free_list = block->next;
        return;
    }

    heap_block_t *current = free_list;
    while (current->next != NULL) {
        if (current->next == block) {
            current->next = block->next;
            return;
        }
        current = current->next;
    }
}

/**
 * @brief Merge adjacent free blocks
 * 
 * After freeing a block, check if adjacent blocks are also free
 * and merge them to reduce fragmentation.
 */
static heap_block_t* merge_adjacent_free(heap_block_t *block)
{
    if (block == NULL) {
        return NULL;
    }

    /* Check if next block is adjacent and free */
    heap_block_t *next_block = (heap_block_t*)((uint8_t*)block + block->size);
    if ((uint8_t*)next_block < heap_end) {
        /* Check if next block is free (size field should be valid) */
        /* We can't easily check if it's free without walking the list,
         * so we'll try to merge and let the integrity check catch errors */
        heap_block_t *check = free_list;
        while (check != NULL) {
            if (check == next_block) {
                /* Found it in free list - merge */
                remove_free_block(next_block);
                block->size += next_block->size;
                break;
            }
            check = check->next;
        }
    }

    /* Check if previous block is adjacent and free */
    if ((uint8_t*)block > heap_start) {
        /* Walk free list to find previous block */
        heap_block_t *prev = NULL;
        heap_block_t *current = free_list;
        while (current != NULL) {
            heap_block_t *potential_prev = (heap_block_t*)((uint8_t*)current + current->size);
            if (potential_prev == block) {
                /* Found previous block - merge */
                remove_free_block(current);
                current->size += block->size;
                return current;
            }
            current = current->next;
        }
    }

    return block;
}

/* Public API */

int heap_init(void *heap_start_addr, size_t heap_size_bytes)
{
    if (heap_start_addr == NULL || heap_size_bytes < HEAP_MIN_BLOCK_SIZE) {
        return -1;
    }

    /* Align heap start to 8-byte boundary */
    uintptr_t addr = (uintptr_t)heap_start_addr;
    addr = (addr + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
    heap_start = (void*)addr;

    /* Align heap size down to multiple of alignment */
    heap_size = heap_size_bytes & ~(HEAP_ALIGNMENT - 1);
    heap_size -= ((uintptr_t)heap_start - (uintptr_t)heap_start_addr);
    heap_size = heap_size & ~(HEAP_ALIGNMENT - 1);
    heap_end = (uint8_t*)heap_start + heap_size;

    /* Initialize free list with one large block covering entire heap */
    free_list = (heap_block_t*)heap_start;
    free_list->size = heap_size;
    free_list->next = NULL;

    return 0;
}

void* heap_malloc(size_t size)
{
    if (heap_start == NULL || free_list == NULL) {
        return NULL;  /* Heap not initialized */
    }

    size_t alloc_size = align_size(size + sizeof(heap_block_t));
    if (alloc_size < size) {
        return NULL;  /* Overflow */
    }

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    /* First-fit algorithm: find first block large enough */
    heap_block_t *prev = NULL;
    heap_block_t *current = free_list;

    while (current != NULL) {
        if (current->size >= alloc_size) {
            /* Found a block large enough */
            size_t remaining = current->size - alloc_size;

            /* If remaining space is large enough for a new block, split it */
            if (remaining >= (HEAP_MIN_BLOCK_SIZE + sizeof(heap_block_t))) {
                /* Split the block */
                heap_block_t *new_free = (heap_block_t*)((uint8_t*)current + alloc_size);
                new_free->size = remaining;
                new_free->next = current->next;

                if (prev == NULL) {
                    free_list = new_free;
                } else {
                    prev->next = new_free;
                }

                current->size = alloc_size;
            } else {
                /* Use entire block */
                if (prev == NULL) {
                    free_list = current->next;
                } else {
                    prev->next = current->next;
                }
            }

            exit_critical_basepri(stat);
            return get_ptr_from_block(current);
        }

        prev = current;
        current = current->next;
    }

    exit_critical_basepri(stat);
    return NULL;  /* No suitable block found */
}

void heap_free(void *ptr)
{
    if (ptr == NULL || heap_start == NULL) {
        return;
    }

    heap_block_t *block = get_block_from_ptr(ptr);

    /* Validate block is within heap */
    if ((uint8_t*)block < heap_start || (uint8_t*)block >= heap_end) {
        return;  /* Invalid pointer */
    }

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    /* Mark as free (next pointer will be set by insert) */
    /* Size should already be set from allocation */
    
    /* Insert into free list and merge adjacent blocks */
    block = merge_adjacent_free(block);
    insert_free_block(block);

    exit_critical_basepri(stat);
}

void* heap_realloc(void *ptr, size_t new_size)
{
    if (ptr == NULL) {
        return heap_malloc(new_size);
    }

    if (new_size == 0) {
        heap_free(ptr);
        return NULL;
    }

    heap_block_t *block = get_block_from_ptr(ptr);
    size_t current_size = block->size - sizeof(heap_block_t);
    size_t aligned_new_size = align_size(new_size + sizeof(heap_block_t)) - sizeof(heap_block_t);

    /* If same size, return original pointer */
    if (aligned_new_size == current_size) {
        return ptr;
    }

    /* If shrinking, just return original (could split, but simpler to leave it) */
    if (aligned_new_size < current_size) {
        return ptr;
    }

    /* If growing, check if next block is free and large enough */
    heap_block_t *next_block = (heap_block_t*)((uint8_t*)block + block->size);
    if ((uint8_t*)next_block < heap_end) {
        /* Check if next block is in free list */
        heap_block_t *check = free_list;
        while (check != NULL) {
            if (check == next_block) {
                /* Next block is free - can we merge? */
                size_t combined_size = block->size + next_block->size;
                size_t needed_size = align_size(new_size + sizeof(heap_block_t));

                if (combined_size >= needed_size) {
                    /* Remove next block from free list and merge */
                    remove_free_block(next_block);
                    block->size = combined_size;

                    /* If there's leftover space, split it */
                    size_t remaining = combined_size - needed_size;
                    if (remaining >= (HEAP_MIN_BLOCK_SIZE + sizeof(heap_block_t))) {
                        heap_block_t *new_free = (heap_block_t*)((uint8_t*)block + needed_size);
                        new_free->size = remaining;
                        insert_free_block(new_free);
                        block->size = needed_size;
                    }
                    return ptr;
                }
                break;
            }
            check = check->next;
        }
    }

    /* Need to allocate new block and copy data */
    void *new_ptr = heap_malloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_size = (current_size < new_size) ? current_size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    heap_free(ptr);

    return new_ptr;
}

int heap_get_stats(heap_stats_t *stats)
{
    if (stats == NULL || heap_start == NULL) {
        return -1;
    }

    memset(stats, 0, sizeof(heap_stats_t));

    stats->total_size = heap_size;

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    /* Walk free list to calculate free memory */
    heap_block_t *current = free_list;
    size_t largest_free = 0;
    uint32_t frag_count = 0;

    while (current != NULL) {
        stats->free_size += current->size;
        frag_count++;
        if (current->size > largest_free) {
            largest_free = current->size;
        }
        current = current->next;
    }

    stats->used_size = stats->total_size - stats->free_size;
    stats->largest_free_block = largest_free;
    stats->fragment_count = frag_count;

    /* Count allocated blocks by walking heap */
    current = (heap_block_t*)heap_start;
    while ((uint8_t*)current < heap_end) {
        /* Check if block is allocated (not in free list) */
        int is_free = 0;
        heap_block_t *free_check = free_list;
        while (free_check != NULL) {
            if (free_check == current) {
                is_free = 1;
                break;
            }
            free_check = free_check->next;
        }

        if (!is_free) {
            stats->block_count++;
        }

        current = (heap_block_t*)((uint8_t*)current + current->size);
        if (current->size == 0) {
            break;  /* Safety check */
        }
    }

    exit_critical_basepri(stat);

    return 0;
}

int heap_check_integrity(void)
{
    if (heap_start == NULL) {
        return -1;
    }

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    /* Walk free list and verify all blocks are within heap */
    heap_block_t *current = free_list;
    while (current != NULL) {
        if ((uint8_t*)current < heap_start || (uint8_t*)current >= heap_end) {
            exit_critical_basepri(stat);
            return -1;  /* Block outside heap */
        }
        if (current->size < HEAP_MIN_BLOCK_SIZE) {
            exit_critical_basepri(stat);
            return -1;  /* Invalid block size */
        }
        if ((uint8_t*)current + current->size > heap_end) {
            exit_critical_basepri(stat);
            return -1;  /* Block extends beyond heap */
        }

        /* Check for loops in free list */
        heap_block_t *check = free_list;
        int count = 0;
        while (check != current && check != NULL) {
            check = check->next;
            count++;
            if (count > 1000) {  /* Safety limit */
                exit_critical_basepri(stat);
                return -1;  /* Possible loop */
            }
        }

        current = current->next;
    }

    exit_critical_basepri(stat);
    return 0;
}

void* heap_get_start(void)
{
    return heap_start;
}

void* heap_get_end(void)
{
    return heap_end;
}

