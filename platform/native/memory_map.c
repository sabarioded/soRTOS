#include "memory_map.h"
#include "allocator.h"
#include <stdint.h>

/* 1MB Heap for the native platform */
static uint8_t native_heap[1024 * 1024];

void memory_map_init(void) {
    allocator_init(native_heap, sizeof(native_heap));
}

void* memory_map_get_heap_start(void) {
    return native_heap;
}

size_t memory_map_get_heap_size(void) {
    return sizeof(native_heap);
}