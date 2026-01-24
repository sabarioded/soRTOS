#include "allocator.h"
#include "arch_ops.h"
#include "utils.h"
#include "platform.h"
#include "spinlock.h"
#include <string.h>
#include "project_config.h"
#include "logger.h"


/* Calculate the actual number of Second Level indices */
#define SL_INDEX_COUNT      (1 << SL_INDEX_COUNT_LOG2)

/* Alignment requirements */
#define ALIGN_SIZE PLATFORM_STACK_ALIGNMENT
#define ALIGN(size) (((size) + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1))

/* Flags and Masks */
#define BLOCK_FREE_BIT      0x1
#define BLOCK_SIZE_MASK     (~(size_t)0x3) /* Mask out bottom 2 bits */

#define GET_SIZE(x)         ((x)->size & BLOCK_SIZE_MASK)
#define IS_FREE(x)          ((x)->size & BLOCK_FREE_BIT)
#define SET_FREE(x)         ((x)->size |= BLOCK_FREE_BIT)
#define SET_USED(x)         ((x)->size &= ~BLOCK_FREE_BIT)

typedef struct BlockHeader {
    size_t size; /* bit 0 is a free bit, rest is size of block including header */
    
    /* Pointer to the block physically previous to this block in memory. */
    struct BlockHeader *prev_phys_block;

    /* 
     * User data starts here.
     * IF the block is FREE, this area is used for free-list pointers.
     */
    struct BlockHeader *next_free;
    struct BlockHeader *prev_free;
} block_header_t;

/* The header size that is always present (size + prev_phys). */
#define BLOCK_OVERHEAD      (offsetof(block_header_t, next_free))

/* Minimum block size must be large enough to hold the full struct */
#define BLOCK_MIN_SIZE      (sizeof(block_header_t))

typedef struct {
    /* Bitmaps to quickly find non-empty free lists */
    uint32_t fl_bitmap;
    uint32_t sl_bitmap[FL_INDEX_MAX];

    /* Array of free lists */
    block_header_t* blocks[FL_INDEX_MAX][SL_INDEX_COUNT];
} control_t;

static control_t control;
static spinlock_t allocator_lock;
static void* heap_start_ptr = NULL;
static void* heap_end_ptr = NULL;

/* Stats */
static size_t mem_capacity = 0;
static size_t free_mem = 0;
static size_t allocated_mem = 0;
static size_t free_blocks = 0;
static size_t allocated_blocks = 0;


/* Count Leading Zeros to find index of set MSB */
static inline uint32_t find_msb_index(uint32_t word) {
    if (word == 0) {
        return 0;   
    }
    /* __builtin_clz returns the number of leading zeros starting from MSB (available in GCC/Clang) */
    return 31 - __builtin_clz(word);
}

/* Find First Set bit (index of set LSB) */
static inline uint32_t find_lsb_index(uint32_t word) {
    return __builtin_ctz(word);
}

/* Calculates the FL/SL indices */
static void mapping_indices_calc(size_t size, uint32_t *fl, uint32_t *sl) {
    if (size < BLOCK_MIN_SIZE) {
        *fl = 0;
        *sl = 0;
        return;
    }
    
    *fl = find_msb_index(size);

    /* Handle small blocks to prevent underflow */
    if (*fl < SL_INDEX_COUNT_LOG2) {
        *sl = size >> 1; /* Simple mapping for tiny blocks */
    } else {
        /* We want to extract the "precision" bits that follow the MSB. */
        
        /* find how many noise bits that dont affect which bucket we choose */
        const uint32_t shift_amount = *fl - SL_INDEX_COUNT_LOG2;
        
        /* drop the noise bits */
        size_t shifted = size >> shift_amount;
        
        /* strip the MSB so we only get the second level bits */
        *sl = shifted ^ (1 << SL_INDEX_COUNT_LOG2);
    }
}

/* Calculates the FL/SL indices to start searching for a free block. */
static void mapping_indices_search(size_t size, uint32_t *fl, uint32_t *sl) {
    if (size >= (1 << FL_INDEX_MAX)) {
        size = (1 << FL_INDEX_MAX) - 1;
    }
    
    /* find the slice width */
    uint32_t fl_temp = find_msb_index(size);

    if (fl_temp >= SL_INDEX_COUNT_LOG2) {
        /* Calculate the step size and add it to size to round up */
        size += (1 << (fl_temp - SL_INDEX_COUNT_LOG2)) - 1;
    }
    
    mapping_indices_calc(size, fl, sl);
}

/* Insert a free block into the appropriate free list */
static void block_insert(block_header_t *block) {
    uint32_t fl, sl;
    mapping_indices_calc(GET_SIZE(block), &fl, &sl);
    
    block->next_free = control.blocks[fl][sl];
    block->prev_free = NULL;
    
    if (block->next_free) {
        block->next_free->prev_free = block;
    }
    
    control.blocks[fl][sl] = block;
    
    /* Update bitmaps */
    control.fl_bitmap |= (1 << fl);
    control.sl_bitmap[fl] |= (1 << sl);
}

/* Remove a free block from the free list */
static void block_remove(block_header_t *block) {
    uint32_t fl, sl;
    mapping_indices_calc(GET_SIZE(block), &fl, &sl);
    
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        control.blocks[fl][sl] = block->next_free;
    }
    
    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    }
    
    /* If list is empty, clear bitmap bit */
    if (control.blocks[fl][sl] == NULL) {
        control.sl_bitmap[fl] &= ~(1 << sl);
        if (control.sl_bitmap[fl] == 0) {
            control.fl_bitmap &= ~(1 << fl);
        }
    }
}

/* Merge a block with its physical neighbor if free */
static block_header_t* block_merge_prev(block_header_t *block) {
    if (block->prev_phys_block && IS_FREE(block->prev_phys_block)) {
        block_header_t *prev = block->prev_phys_block;
        /* Remove prev from free list because its size about to change */
        block_remove(prev);
        
        /* Extend prev to cover block */
        /* Update stats: We are reclaiming the header of 'block' as usable memory */
        free_mem += BLOCK_OVERHEAD;
        free_blocks--;

        size_t size = GET_SIZE(block);
        prev->size += size;
        
        /* Update the next block's prev pointer */
        block_header_t *next = (block_header_t*)((uint8_t*)prev + GET_SIZE(prev));
        if ((void*)next < heap_end_ptr) {
            next->prev_phys_block = prev;
        }

        return prev; /* Larger merged block */
    }
    /* No merge required */
    return block;
}

/* Merge a block with its physical next neighbor if free */
static block_header_t* block_merge_next(block_header_t *block) {
    block_header_t *next = (block_header_t*)((uint8_t*)block + GET_SIZE(block));
    
    if ((void*)next < heap_end_ptr && IS_FREE(next)) {
        /* Remove next from free list because its size about to change */
        block_remove(next);
        
        /* Update stats: Reclaiming 'next' header */
        free_mem += BLOCK_OVERHEAD;
        free_blocks--;

        /* Extend block to cover next */
        size_t size = GET_SIZE(next);
        block->size += size;
        
        /* Update the block after next */
        block_header_t *next_next = (block_header_t*)((uint8_t*)block + GET_SIZE(block));
        if ((void*)next_next < heap_end_ptr) {
            next_next->prev_phys_block = block;
        }
    }

    return block;
}

/* Split a block if it's too big */
static void block_trim(block_header_t *block, size_t size) {
    size_t remaining_size = GET_SIZE(block) - size;
    
    if (remaining_size >= BLOCK_MIN_SIZE) {
        block_header_t *remaining = (block_header_t*)((uint8_t*)block + size);
        
        remaining->size = remaining_size | BLOCK_FREE_BIT;
        remaining->prev_phys_block = block;
        
        block->size = size; /* Mark as used (no free bit) */
        
        /* Update the block after remaining */
        block_header_t *next = (block_header_t*)((uint8_t*)remaining + GET_SIZE(remaining));
        if ((void*)next < heap_end_ptr) {
            next->prev_phys_block = remaining;
        }
        
        remaining = block_merge_next(remaining);
        
        block_insert(remaining);
        
        /* Update stats */
        free_blocks++;
        free_mem += (remaining_size - BLOCK_OVERHEAD);
    }
}

/* Find a suitable free block */
static block_header_t* block_locate_free(size_t size) {
    uint32_t fl, sl;
    mapping_indices_search(size, &fl, &sl);
    
    /* Check if there is a block in the specific SL list */
    uint32_t sl_map = control.sl_bitmap[fl] & (~0U << sl);
    
    if (!sl_map) {
        /* No block in this FL, check higher FLs */
        uint32_t fl_map = control.fl_bitmap & (~0U << (fl + 1));
        if (!fl_map) {
            return NULL; /* No memory available */
        }
        
        fl = find_lsb_index(fl_map);
        sl_map = control.sl_bitmap[fl];
    }
    
    sl = find_lsb_index(sl_map);
    return control.blocks[fl][sl];
}

/* Initialize the memory pool and TLSF structures */
void allocator_init(uint8_t* pool, size_t size) {
    spinlock_init(&allocator_lock);
    utils_memset(&control, 0, sizeof(control));
    
    uintptr_t raw_addr = (uintptr_t)pool;
    uintptr_t aligned_addr = ALIGN(raw_addr);
    size -= (aligned_addr - raw_addr);
    
    if (size < BLOCK_MIN_SIZE) {
        platform_panic();  
    }
    
    heap_start_ptr = (void*)aligned_addr;
    heap_end_ptr = (void*)((uint8_t*)heap_start_ptr + size);
    mem_capacity = size;
    
    /* Create one massive free block */
    block_header_t *block = (block_header_t*)heap_start_ptr;
    block->size = size | BLOCK_FREE_BIT;
    block->prev_phys_block = NULL;
    
    block_insert(block);
    
    free_mem = size - BLOCK_OVERHEAD;
    allocated_mem = 0;
    free_blocks = 1;
    allocated_blocks = 0;
#if LOG_ENABLE
    logger_log("Heap Init Size:%u", (uint32_t)size, 0);
#endif
}

/* Allocate a block of memory of at least size bytes */
void* allocator_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    /* Add overhead and align */
    size_t adjust = size + BLOCK_OVERHEAD;
    /* Check for overflow */
    if (adjust < size) {
        return NULL;
    }
    size_t req_size = (adjust < BLOCK_MIN_SIZE) ? BLOCK_MIN_SIZE : ALIGN(adjust);
    
    uint32_t flags = spin_lock(&allocator_lock);
    
    block_header_t *block = block_locate_free(req_size);
    
    if (block) {
        block_remove(block);
        
        /* Update stats: Remove the ENTIRE block from free stats first */
        free_mem -= (GET_SIZE(block) - BLOCK_OVERHEAD);
        free_blocks--;

        block_trim(block, req_size);
        SET_USED(block);
        
        allocated_mem += (GET_SIZE(block) - BLOCK_OVERHEAD);
        allocated_blocks++;
        
        spin_unlock(&allocator_lock, flags);
        return (void*)((uint8_t*)block + BLOCK_OVERHEAD);
    }
    
    spin_unlock(&allocator_lock, flags);
#if LOG_ENABLE
    logger_log("Malloc Fail Size:%u", (uint32_t)size, 0);
#endif
    return NULL;
}

/* Free a previously allocated block */
void allocator_free(void* ptr) {
    if(!ptr) {
        return;
    }

    uint32_t flags = spin_lock(&allocator_lock);
    
    block_header_t *block = (block_header_t*)((uint8_t*)ptr - BLOCK_OVERHEAD);
    
    /* Update stats */
    size_t size = GET_SIZE(block);
    allocated_mem -= (size - BLOCK_OVERHEAD);
    allocated_blocks--;
    
    /* Add to free stats (will be adjusted if merged) */
    free_mem += (size - BLOCK_OVERHEAD);
    free_blocks++;

    SET_FREE(block);
    block = block_merge_prev(block);
    block = block_merge_next(block);
    block_insert(block);
    
    spin_unlock(&allocator_lock, flags);
}

/* Resize a previously allocated block */
void* allocator_realloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return allocator_malloc(new_size);
    }
    if (new_size == 0) {
        allocator_free(ptr);
        return NULL;
    }

    uint32_t flags = spin_lock(&allocator_lock);
    
    block_header_t *block = (block_header_t*)((uint8_t*)ptr - BLOCK_OVERHEAD);
    size_t curr_size = GET_SIZE(block);
    size_t req_size = ALIGN(new_size + BLOCK_OVERHEAD);
    if (req_size < BLOCK_MIN_SIZE) {
        req_size = BLOCK_MIN_SIZE;
    }

    /* Case 1: Shrinking */
    if (curr_size >= req_size) {
        /* Try to split */
        if (curr_size - req_size >= BLOCK_MIN_SIZE) {
            block_trim(block, req_size);
            /* block_trim adds remainder to free stats. We must reduce allocated stats. */
            allocated_mem -= (curr_size - req_size);
        }
        spin_unlock(&allocator_lock, flags);
        return ptr;
    }
    
    /* Case 2: Growing - Try to merge with next block */
    block_header_t *next = (block_header_t*)((uint8_t*)block + curr_size);
    if ((void*)next < heap_end_ptr && IS_FREE(next)) {
        size_t combined_size = curr_size + GET_SIZE(next);
        if (combined_size >= req_size) {
            block_remove(next);
            
            /* Update stats: Consuming a free block */
            free_mem -= (GET_SIZE(next) - BLOCK_OVERHEAD);
            free_blocks--;
            
            block->size = combined_size; /* Still used */
            
            /* Update next's next prev pointer */
            block_header_t *next_next = (block_header_t*)((uint8_t*)block + combined_size);
            if ((void*)next_next < heap_end_ptr) {
                next_next->prev_phys_block = block;
            }
            
            /* We gained 'next' as allocated memory (header + payload) */
            allocated_mem += GET_SIZE(next);

            /* Split if too big */
            if (combined_size - req_size >= BLOCK_MIN_SIZE) {
                block_trim(block, req_size);
                /* block_trim adds remainder to free stats. Reduce allocated. */
                allocated_mem -= (combined_size - req_size);
            }
            
            spin_unlock(&allocator_lock, flags);
            return ptr;
        }
    }
    
    /* Case 3: Full Realloc */
    spin_unlock(&allocator_lock, flags);
    
    void* new_ptr = allocator_malloc(new_size);
    if (new_ptr) {
        utils_memcpy(new_ptr, ptr, curr_size - BLOCK_OVERHEAD);
        allocator_free(ptr);
    }
    return new_ptr;
}

/* Get the total amount of free memory in bytes */
size_t allocator_get_free_size(void) {
    return free_mem;
}

/* Get the number of free blocks (fragments) */
size_t allocator_get_fragment_count(void) {
    return free_blocks;
}

/* Populate the heap statistics structure */
int allocator_get_stats(heap_stats_t *stats) {
    if (!stats) {
        return -1;
    }
    
    uint32_t flags = spin_lock(&allocator_lock);
    
    stats->total_size = mem_capacity;
    stats->free_size = free_mem;
    stats->used_size = allocated_mem;
    stats->allocated_blocks = allocated_blocks;
    stats->free_blocks = free_blocks;
    
    /* Find largest free block by checking bitmaps from top down */
    stats->largest_free_block = 0;
    if (control.fl_bitmap) {
        uint32_t fl = find_msb_index(control.fl_bitmap);
        uint32_t sl = find_msb_index(control.sl_bitmap[fl]);
        block_header_t *block = control.blocks[fl][sl];
        if (block) {
            stats->largest_free_block = GET_SIZE(block) - BLOCK_OVERHEAD;
        }
    }
    
    spin_unlock(&allocator_lock, flags);
    return 0;
}

/* Verify the integrity of the heap */
int allocator_check_integrity(void) {
    uint32_t flags = spin_lock(&allocator_lock);
    
    if (!heap_start_ptr) {
        spin_unlock(&allocator_lock, flags);
        return -1; /* Heap not initialized */
    }
    
    /* Physical Walk: Verify heap layout and block integrity */
    block_header_t *curr = (block_header_t*)heap_start_ptr;
    block_header_t *prev = NULL;
    
    size_t calc_free_mem = 0;
    size_t calc_allocated_mem = 0;
    size_t calc_free_blocks = 0;
    size_t calc_allocated_blocks = 0;
    
    while ((void*)curr < heap_end_ptr) {
        size_t size = GET_SIZE(curr);
        
        /* Check alignment */
        if ((uintptr_t)curr & (ALIGN_SIZE - 1)) {
            spin_unlock(&allocator_lock, flags);
#if LOG_ENABLE
            logger_log("Heap Align Err", 0, 0);
#endif
            return -2; /* Block Alignment Error */
        }
        
        /* Check header bits for corruption (bits between LSB and alignment) */
        if (curr->size & (ALIGN_SIZE - 1) & ~BLOCK_FREE_BIT) {
            spin_unlock(&allocator_lock, flags);
#if LOG_ENABLE
            logger_log("Heap Header Err", 0, 0);
#endif
            return -2; /* Block Alignment Error */
        }

        /* Check size validity */
        if (size < BLOCK_MIN_SIZE) {
            spin_unlock(&allocator_lock, flags);
#if LOG_ENABLE
            logger_log("Heap Block Small", 0, 0);
#endif
            return -3; /* Block too small (header corruption) */
        }
        
        /* Check boundary overflow */
        if ((void*)((uint8_t*)curr + size) > heap_end_ptr) {
            spin_unlock(&allocator_lock, flags);
#if LOG_ENABLE
            logger_log("Heap Overflow", 0, 0);
#endif
            return -4; /* Size pushes past heap end */
        }

        /* Check physical link consistency */
        if (curr->prev_phys_block != prev) {
            spin_unlock(&allocator_lock, flags);
#if LOG_ENABLE
            logger_log("Heap Chain Broken", 0, 0);
#endif
            return -5; /* Broken physical chain */
        }
        
        /* Free block checks */
        if (IS_FREE(curr)) {
            /* Coalescing Check: Two free blocks should never be adjacent */
            if (prev && IS_FREE(prev)) {
                 spin_unlock(&allocator_lock, flags);
                 return -6; /* Missed merge opportunity (fragmentation) */
            }

            calc_free_mem += (size - BLOCK_OVERHEAD);
            calc_free_blocks++;
        } else {
            calc_allocated_mem += (size - BLOCK_OVERHEAD);
            calc_allocated_blocks++;
        }
        
        prev = curr;
        curr = (block_header_t*)((uint8_t*)curr + size);
    }
    
    /* Check if we ended exactly at the heap end */
    if ((void*)curr != heap_end_ptr) {
        spin_unlock(&allocator_lock, flags);
        return -7; /* Heap size mismatch */
    }
    
    /* Logical Walk: Verify free lists and bitmaps */
    
    size_t list_free_blocks = 0;

    for (int fl = 0; fl < FL_INDEX_MAX; fl++) {
        for (int sl = 0; sl < SL_INDEX_COUNT; sl++) {
            block_header_t *block = control.blocks[fl][sl];
            
            if (block) {
                /* Verify Bitmap: If list is not null, bit must be set */
                if (!(control.sl_bitmap[fl] & (1 << sl))) {
                     spin_unlock(&allocator_lock, flags);
                     return -8; /* Bitmap desync: List has blocks, bit is 0 */
                }
            } else {
                 /* Verify Bitmap: If list is null, bit must be cleared */
                 if (control.sl_bitmap[fl] & (1 << sl)) {
                     spin_unlock(&allocator_lock, flags);
                     return -9; /* Bitmap desync: List empty, bit is 1 */
                 }
            }

            block_header_t *prev_node = NULL;
            while (block) {
                /* Verify block is actually free */
                if (!IS_FREE(block)) {
                    spin_unlock(&allocator_lock, flags);
                    return -10; /* Used block found in free list */
                }

                /* Verify back-link */
                if (block->prev_free != prev_node) {
                    spin_unlock(&allocator_lock, flags);
                    return -11; /* Corrupt double linked list */
                }

                /* Verify bucket placement */
                uint32_t correct_fl, correct_sl;
                mapping_indices_calc(GET_SIZE(block), &correct_fl, &correct_sl);
                if (correct_fl != fl || correct_sl != sl) {
                     spin_unlock(&allocator_lock, flags);
                     return -12; /* Block in wrong bucket */
                }

                list_free_blocks++;
                prev_node = block;
                
                /* Bounds check for pointer safety */
                if ((void*)block->next_free != NULL && 
                   ((void*)block->next_free < heap_start_ptr || 
                    (void*)block->next_free >= heap_end_ptr)) {
                    spin_unlock(&allocator_lock, flags);
                    return -13; /* Wild pointer in free list */
                }

                block = block->next_free;
            }
        }
    }

    /* Verify statistics consistency */
    if (calc_free_mem != free_mem || calc_allocated_mem != allocated_mem ||
        calc_free_blocks != free_blocks || calc_allocated_blocks != allocated_blocks) {
        spin_unlock(&allocator_lock, flags);
        return -14; /* Global stats counter mismatch */
    }
    
    if (list_free_blocks != free_blocks) {
        spin_unlock(&allocator_lock, flags);
        return -15; /* Physical free count != Logical free list count */
    }
    
    spin_unlock(&allocator_lock, flags);
    return 0; /* Integrity OK */
}
