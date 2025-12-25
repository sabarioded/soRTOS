#include "allocator.h"
#include <string.h>

/* 8 bytes header */
typedef struct Block {
    size_t size_and_free; // Bit 0: is_free, Bits 1-31: actual size
    struct Block* next;
} Block;

#define IS_FREE_MASK 0x01
#define NOT_FREE_MASK 0x00

#define GET_SIZE(s)  ((s) & ~IS_FREE_MASK)
#define GET_FREE(s)  ((s) & IS_FREE_MASK)

// hardware-aware alignment
#define ALIGN_SIZE sizeof(void*) /* how big is a pointer in target machine */
/* 
 * align size to 4 byte
 * for example for STM32 4 byte we get (size + 00..011) & 11..100 
 * if size=6 we get (6+3) & 11...100 = 00..01001 & 11...100 = 1000 = 8
 */
#define ALIGN(size) (((size) + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1))
#define UPDATE_SIZE_AND_FREE(size, free) (((size) & ~IS_FREE_MASK) | (free))

static Block* head = NULL;
static size_t mem_capacity = 0;
static size_t free_mem = 0;

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
}

void* allocator_malloc(size_t size) {
    if (size == 0 || size > mem_capacity) return NULL;
    size_t aligned_size = ALIGN(size);

    Block* curr = head;
    while (curr) {
        size_t curr_size = GET_SIZE(curr->size_and_free);
        if (GET_FREE(curr->size_and_free) && curr_size >= aligned_size) {
            
            /* Check if we can split the current block. need space for header+data+*/
            if (curr_size >= (aligned_size + sizeof(Block) + ALIGN_SIZE)) {
                /* start of the next block (the data) + the size of data the user requested. */
                Block* next_block = (Block*)((uint8_t*)(curr + 1) + size);
                
                /* Calculate remaining size for the new block */
                size_t remaining_size = curr_size - aligned_size - sizeof(Block);
                
                /* Set up the new free block */
                next_block->size_and_free = UPDATE_SIZE_AND_FREE(remaining_size, IS_FREE_MASK);
                next_block->next = curr->next;

                /* Update current block */
                curr->next = next_block;
                curr_size = aligned_size;
                free_mem -= (aligned_size + sizeof(Block));
            } else {
                /* header already allocated */
                free_mem -= curr_size;
            }

            curr->size_and_free = UPDATE_SIZE_AND_FREE(curr_size, NOT_FREE_MASK);
            return (void*)(curr + 1);
        }
        curr = curr->next;
    }
    return NULL;
}

void allocator_free(void* ptr) {
    if(!ptr) return;

    /* Free the requested block (its before the data) */
    Block* block_to_free = (Block*)ptr - 1;
    block_to_free->size_and_free |= IS_FREE_MASK;
    free_mem += GET_SIZE(block_to_free->size_and_free);

    /* Merge free blocks */
    Block* curr = head;
    while (curr && curr->next) {
        if (GET_FREE(curr->size_and_free) && GET_FREE(curr->next->size_and_free)) {
            /* The next block's header is now usable memory */
            free_mem += sizeof(Block);

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
}

void* allocator_realloc(void* ptr, size_t new_size) {
    if (!ptr) return allocator_malloc(new_size);
    
    if (new_size == 0) {
        allocator_free(ptr);
        return NULL;
    }

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
            
        }
        return ptr;
    } 

    // Case 2: Growing (Must move)
    void* new_ptr = allocator_malloc(new_size);
    if (new_ptr) {
        // Only copy the data that fits in both
        memcpy(new_ptr, ptr, curr_size);
        allocator_free(ptr);
    } else {
        return NULL;
    }
    return new_ptr;
}

size_t allocator_get_free_size(void) {
    return free_mem;
}

size_t allocator_get_fragment_count(void) {
    Block* curr = head;
    size_t fgmt_cnt = 0;
    while(curr) {
        if(GET_FREE(curr->size_and_free)) {
            fgmt_cnt++;
        }
        curr = curr->next;
    }
    return fgmt_cnt;
}

void allocator_dump_stats(int (*print_fn)(const char*, ...)) {
    if (!print_fn) return;

    Block* curr = head;
    print_fn("\r\n--- STM32 HEAP MAP ---\r\n");
    print_fn("Capacity: %zu bytes | Free: %zu bytes\r\n", mem_capacity, free_mem);
    print_fn("------------------------------------------\r\n");

    int i = 0;
    while (curr) {
        size_t size = GET_SIZE(curr->size_and_free);
        int is_free = GET_FREE(curr->size_and_free);
        
        // Print block index, status, payload size, and the payload start address
        print_fn("[%d] %s | Size: %zu | Data Addr: %p\r\n", 
                 i++, 
                 is_free ? "FREE" : "USED", 
                 size, 
                 (void*)(curr + 1)); // Points to the actual user memory
                 
        curr = curr->next;
    }
    print_fn("------------------------------------------\r\n");
    print_fn("Fragment Count: %zu\r\n", allocator_get_fragment_count());
}
