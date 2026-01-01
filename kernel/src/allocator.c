#include "allocator.h"
#include "arch_ops.h"
#include "utils.h"

/* 8 bytes header */
typedef struct Block {
    size_t size_and_free; // Bit 0: is_free, Bits 1-31: actual size
    struct Block* next;
} Block;

#define IS_FREE_MASK ((size_t)0x01)
#define NOT_FREE_MASK 0x00

#define GET_SIZE(s)  ((s) & ~IS_FREE_MASK)
#define GET_FREE(s)  ((s) & IS_FREE_MASK)

// hardware-aware alignment
#define ALIGN_SIZE sizeof(void*) /* how big is a pointer in target machine */
/* 
 * Align size to machine word size (e.g., 4 bytes on 32-bit, 8 bytes on 64-bit).
 * This ensures pointers and data structures are properly aligned for the architecture.
 */
#define ALIGN(size) (((size) + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1))
#define UPDATE_SIZE_AND_FREE(size, free) (((size) & ~IS_FREE_MASK) | (free))

static Block* head = NULL;
static size_t mem_capacity = 0;
static size_t free_mem = 0;
static size_t allocated_mem = 0;
static size_t free_blocks = 0;
static size_t allocated_blocks = 0;

static Block* allocator_get_largest_free_block(void) {
    Block* curr = head;
    Block* largest_block = NULL;
    size_t largest_free = 0;
    while (curr) {
        size_t size = GET_SIZE(curr->size_and_free);
        int is_free = GET_FREE(curr->size_and_free);

        if (is_free) {
            if (size > largest_free) {
                largest_free = size;
                largest_block = curr;
            }
        } 
        curr = curr->next;
    }
    return largest_block;
}

void allocator_init(uint8_t* pool, size_t size) {
    /* Ensure the start of the pool is aligned */
    uintptr_t raw_addr = (uintptr_t)pool;
    uintptr_t aligned_addr = ALIGN(raw_addr);
    
    /* Adjust the size to the new aligned start address */
    size -= (aligned_addr - raw_addr);
    
    if (size < sizeof(Block)) return;

    head = (Block*)aligned_addr;
    size_t usable_size = size - sizeof(Block);
    mem_capacity = size;
    free_mem = usable_size;

    // Mark as FREE (Bit 0 = 1)
    head->size_and_free = UPDATE_SIZE_AND_FREE(usable_size, IS_FREE_MASK);
    head->next = NULL;

    free_blocks = 1;
    allocated_blocks = 0;
}

void* allocator_malloc(size_t size) {
    uint32_t flags = arch_irq_lock();
    if (size == 0 || size > mem_capacity) {
        arch_irq_unlock(flags);
        return NULL;
    }
    size_t aligned_size = ALIGN(size);

    Block* curr = head;
    while (curr) {
        size_t curr_size = GET_SIZE(curr->size_and_free);
        if (GET_FREE(curr->size_and_free) && curr_size >= aligned_size) {
            
            /* Check if we can split the current block. need space for header+data+*/
            if (curr_size >= (aligned_size + sizeof(Block) + ALIGN_SIZE)) {
                /* start of the next block (the data) + the size of data the user requested. */
                Block* next_block = (Block*)((uint8_t*)(curr + 1) + aligned_size);
                
                /* Calculate remaining size for the new block */
                size_t remaining_size = curr_size - aligned_size - sizeof(Block);
                
                /* Set up the new free block */
                next_block->size_and_free = UPDATE_SIZE_AND_FREE(remaining_size, IS_FREE_MASK);
                next_block->next = curr->next;

                /* Update current block */
                curr->next = next_block;
                curr_size = aligned_size;
                
                /* update stats */
                free_mem -= (aligned_size + sizeof(Block));
                allocated_mem += aligned_size;
                allocated_blocks++;
            } else {
                /* header already allocated */
                free_mem -= curr_size;
                allocated_mem += curr_size;
                free_blocks--;
                allocated_blocks++;
            }

            curr->size_and_free = UPDATE_SIZE_AND_FREE(curr_size, NOT_FREE_MASK);
            arch_irq_unlock(flags);
            return (void*)(curr + 1);
        }
        curr = curr->next;
    }
    arch_irq_unlock(flags);
    return NULL;
}

void allocator_free(void* ptr) {
    if(!ptr) return;

    uint32_t flags = arch_irq_lock();

    /* Free the requested block (its before the data) */
    Block* block_to_free = (Block*)ptr - 1;
    block_to_free->size_and_free |= IS_FREE_MASK;
    size_t block_mem = GET_SIZE(block_to_free->size_and_free);
    free_mem += block_mem;
    allocated_mem -= block_mem;
    free_blocks++;
    allocated_blocks--;

    /* Merge free blocks */
    Block* curr = head;
    while (curr && curr->next) {
        if (GET_FREE(curr->size_and_free) && GET_FREE(curr->next->size_and_free)) {
            /* The next block's header is now usable memory */
            free_mem += sizeof(Block);
            free_blocks--;

            /* size of the current block + the next block + its size */
            size_t merged_size = GET_SIZE(curr->size_and_free) + 
                                 sizeof(Block) + 
                                 GET_SIZE(curr->next->size_and_free);
            
            curr->size_and_free = UPDATE_SIZE_AND_FREE(merged_size, IS_FREE_MASK);

            curr->next = curr->next->next;
            continue;   
        }
        curr = curr->next;
    }
    arch_irq_unlock(flags);
}

void* allocator_realloc(void* ptr, size_t new_size) {
    if (!ptr) return allocator_malloc(new_size);
    
    if (new_size == 0) {
        allocator_free(ptr);
        return NULL;
    }

    /* Lock to safely read block header and prevent race conditions */
    uint32_t flags = arch_irq_lock();

    Block* block = (Block*)ptr - 1;
    size_t curr_size = GET_SIZE(block->size_and_free);
    size_t aligned_new = ALIGN(new_size);

    // Case 1: Shrinking or same size
    if (curr_size >= aligned_new) {
        if (curr_size >= (aligned_new + sizeof(Block) + ALIGN_SIZE)) {
            Block* next_block = (Block*)((uint8_t*)(block + 1) + aligned_new);
            size_t remaining_size = curr_size - aligned_new - sizeof(Block);

            next_block->size_and_free = UPDATE_SIZE_AND_FREE(remaining_size, IS_FREE_MASK);
            next_block->next = block->next;
            block->next = next_block;

            block->size_and_free = UPDATE_SIZE_AND_FREE(aligned_new, NOT_FREE_MASK);
            free_mem += remaining_size; 
            allocated_mem -= (remaining_size + sizeof(Block));
            free_blocks++;
        }
        arch_irq_unlock(flags);
        return ptr;
    } 

    // Case 2: Growing (Must move)
    void* new_ptr = allocator_malloc(new_size);
    if (new_ptr) {
        // Only copy the data that fits in both
        memcpy(new_ptr, ptr, curr_size);
        allocator_free(ptr);
        /* allocator_free takes its own lock, but we are holding one. 
           arch_irq_lock supports nesting, so this is safe. */
    } else {
        arch_irq_unlock(flags);
        return NULL;
    }
    arch_irq_unlock(flags);
    return new_ptr;
}

size_t allocator_get_free_size(void) {
    return free_mem;
}

size_t allocator_get_fragment_count(void) {
    return free_blocks;
}

int allocator_get_stats(heap_stats_t *stats) {
    uint32_t flags = arch_irq_lock();
    if (stats == NULL) {
        arch_irq_unlock(flags);
        return -1;
    }
    
    if (head == NULL) {
        /* Allocator not initialized yet */
        stats->total_size = 0;
        stats->used_size = 0;
        stats->free_size = 0;
        stats->largest_free_block = 0;
        stats->allocated_blocks = 0;
        stats->free_blocks = 0;
        arch_irq_unlock(flags);
        return -1;
    }

    /* Get the global stats*/
    stats->total_size       = mem_capacity;
    stats->free_size        = free_mem;
    stats->used_size        = stats->total_size - stats->free_size;
    stats->allocated_blocks = allocated_blocks;
    stats->free_blocks      = free_blocks;

    /* Calculate the largest free block */
    Block* largest_free_block = allocator_get_largest_free_block();
    if(largest_free_block) {
        stats->largest_free_block = GET_SIZE(largest_free_block->size_and_free);
    } else {
        stats->largest_free_block = 0;
    }

    arch_irq_unlock(flags);
    return 0;
}

int allocator_check_integrity(void) {
    uint32_t flags = arch_irq_lock();
    if (!head) return -1; /* Not initialized */

    size_t calculated_free = 0;
    size_t allocated_size = 0;
    Block* curr = head;
    
    /* We need boundaries to check if pointers are valid */
    uintptr_t heap_start = (uintptr_t)head;
    uintptr_t heap_end   = heap_start + mem_capacity;

    while (curr) {
        /* The current block must be within heap limits. */
        if ((uintptr_t)curr < heap_start || (uintptr_t)curr >= heap_end) {
            arch_irq_unlock(flags);
            return -1;
        }

        size_t size = GET_SIZE(curr->size_and_free);
        int is_free = GET_FREE(curr->size_and_free);

        /* A block cannot be larger than the entire heap. */
        if (size > mem_capacity) {
            arch_irq_unlock(flags);
            return -1;
        }

        if (is_free) {
            calculated_free += size;
        } else {
            allocated_size += size;
        }

        curr = curr->next;
    }

    /* The sum of free blocks found must match the global counter. */
    if (calculated_free != free_mem) {
        arch_irq_unlock(flags);
        return -1;
    }

    /* The sum of allocated blocks found must match the global counter. */
    if (allocated_size != allocated_mem) {
        arch_irq_unlock(flags);
        return -1;
    }

    arch_irq_unlock(flags);
    return 0; /* Integrity OK */
}
