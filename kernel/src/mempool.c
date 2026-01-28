#include "mempool.h"
#include "allocator.h"
#include "spinlock.h"
#include "platform.h"
#include "utils.h"

#define ALIGN_SIZE      sizeof(void*)
#define ALIGN(size)     (((size) + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1))

struct mempool {
    void        *buffer;        /* Contiguous memory block */
    size_t      item_size;      /* Size of one item (aligned) */
    size_t      count;          /* Total capacity */
    void        *free_list;     /* Head of the free list */
    spinlock_t  lock;
};

/* Create a fixed-size memory pool */
mempool_t* mempool_create(size_t item_size, size_t count) {
    if (item_size == 0 || count == 0) {
        return NULL;
    }

    /*
     * Item size must be at least large enough to hold a pointer 
     * for the free list, and aligned to the system pointer size.
     */
    size_t real_size = item_size;
    if (real_size < sizeof(void*)) {
        real_size = sizeof(void*);
    }
    real_size = ALIGN(real_size);

    mempool_t *pool = (mempool_t*)allocator_malloc(sizeof(mempool_t));
    if (!pool) {
        return NULL;
    }

    pool->buffer = allocator_malloc(real_size * count);
    if (!pool->buffer) {
        allocator_free(pool);
        return NULL;
    }

    pool->item_size = real_size;
    pool->count = count;
    spinlock_init(&pool->lock);

    /* Initialize free list by threading pointers through the buffer */
    uint8_t *ptr = (uint8_t*)pool->buffer;
    pool->free_list = ptr;

    for (size_t i = 0; i < count - 1; i++) {
        void **next_link = (void**)ptr;
        /* Link current block to the next */
        *next_link = (ptr + real_size);
        ptr += real_size;
    }
    
    /* Last item points to NULL */
    *((void**)ptr) = NULL;

    return pool;
}

/* Allocate an item from the pool */
void* mempool_alloc(mempool_t *pool) {
    if (!pool) {
        return NULL;
    }

    uint32_t flags = spin_lock(&pool->lock);

    void *ptr = pool->free_list;
    if (ptr) {
        /* Move head to the next item */
        pool->free_list = *((void**)ptr);
    }

    spin_unlock(&pool->lock, flags);
    return ptr;
}

/* Return an item to the pool */
void mempool_free(mempool_t *pool, void *ptr) {
    if (!pool || !ptr) {
        return;
    }

    /* ensure ptr is within the pool's buffer range. */
    uintptr_t start = (uintptr_t)pool->buffer;
    uintptr_t end = start + (pool->count * pool->item_size);
    uintptr_t p = (uintptr_t)ptr;

    if (p < start || p >= end) {
        return;
    }

    uint32_t flags = spin_lock(&pool->lock);

    /* Push onto head of free list */
    *((void**)ptr) = pool->free_list;
    pool->free_list = ptr;

    spin_unlock(&pool->lock, flags);
}

/* Delete the pool and free all resources */
void mempool_delete(mempool_t *pool) {
    if (!pool) {
        return;
    }

    if (pool->buffer) {
        allocator_free(pool->buffer);
    }

    allocator_free(pool);
}
