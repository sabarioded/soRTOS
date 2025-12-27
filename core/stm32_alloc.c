#include "utils.h"
#include "stm32_alloc.h"

#define ALLOCATOR_PRIORITY_THRESHOLD 0x50

void  stm32_allocator_init(uint8_t* pool, size_t size) {
    uint32_t status = enter_critical_basepri(ALLOCATOR_PRIORITY_THRESHOLD);
    allocator_init(pool, size);
    exit_critical_basepri(status);
}

void* stm32_allocator_malloc(size_t size) {
    uint32_t status = enter_critical_basepri(ALLOCATOR_PRIORITY_THRESHOLD);
    void* ptr = allocator_malloc(size);
    exit_critical_basepri(status);
    return ptr;
}

void  stm32_allocator_free(void* ptr) {
    uint32_t status = enter_critical_basepri(ALLOCATOR_PRIORITY_THRESHOLD);
    allocator_free(ptr);
    exit_critical_basepri(status);
}

void* stm32_allocator_realloc(void* ptr, size_t new_size) {
    uint32_t status = enter_critical_basepri(ALLOCATOR_PRIORITY_THRESHOLD);
    void* new_ptr = allocator_realloc(ptr, new_size);
    exit_critical_basepri(status);
    return new_ptr;
}

size_t stm32_allocator_get_free_size(void) {
    uint32_t status = enter_critical_basepri(ALLOCATOR_PRIORITY_THRESHOLD);
    size_t size = allocator_get_free_size();
    exit_critical_basepri(status);
    return size;
}

size_t stm32_allocator_get_fragment_count(void) {
    uint32_t status = enter_critical_basepri(ALLOCATOR_PRIORITY_THRESHOLD);
    size_t fgmnt_cnt = allocator_get_fragment_count();   
    exit_critical_basepri(status);
    return fgmnt_cnt;
}

int stm32_allocator_get_stats(heap_stats_t *stats) {
    uint32_t status = enter_critical_basepri(ALLOCATOR_PRIORITY_THRESHOLD);
    int integrity = allocator_get_stats(stats);
    exit_critical_basepri(status);
    return integrity;
}

int stm32_allocator_check_integrity(void) {
    uint32_t status = enter_critical_basepri(ALLOCATOR_PRIORITY_THRESHOLD);
    int integrity = allocator_check_integrity();
    exit_critical_basepri(status);
    return integrity;
}
