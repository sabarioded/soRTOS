#include "allocator.h"
#include <stdint.h>
#include <stddef.h>
#include <platform.h>

extern uint8_t __heap_start__;       /* Start of heap (after .bss) */
extern uint8_t __heap_end__;        /* End of heap (end of SRAM1) */

static size_t heap_size;

/* Initialize the memory map and the kernel allocator */
void memory_map_init(void) {
    uint8_t* start_addr = &__heap_start__;
    uint8_t* end_addr   = &__heap_end__;

    size_t heap_size = 0;

    if (end_addr > start_addr) {
        heap_size = (size_t)(end_addr - start_addr);
    } else {
        platform_panic();
    }

    /* initialize the memory pool for tasks and mallocs */
    allocator_init(start_addr, heap_size);
}

/* Get the start address of the heap */
void* memory_map_get_heap_start(void) {
    return (void*)&__heap_start__;
}

/* Get the total size of the heap */
size_t memory_map_get_heap_size(void) {
    return heap_size;
}