#include "dma.h"
#include "dma_hal.h"
#include "allocator.h"
#include "utils.h"

struct dma_context {
    void *hal_handle;
};

size_t dma_get_context_size(void) {
    return sizeof(struct dma_context);
}

dma_channel_t dma_init(void *memory_block, void *hal_handle, void *config) {
    if (!memory_block || !hal_handle) {
        return NULL;
    }

    dma_channel_t channel = (dma_channel_t)memory_block;
    utils_memset(channel, 0, sizeof(struct dma_context));
    
    channel->hal_handle = hal_handle;

    /* Initialize hardware via HAL */
    dma_hal_init(channel->hal_handle, config);

    return channel;
}

dma_channel_t dma_create(void *hal_handle, void *config) {
    void *mem = allocator_malloc(sizeof(struct dma_context));
    if (!mem) {
        return NULL;
    }
    return dma_init(mem, hal_handle, config);
}

void dma_destroy(dma_channel_t channel) {
    if (channel) {
        dma_hal_stop(channel->hal_handle);
        allocator_free(channel);
    }
}

int dma_start(dma_channel_t channel, void *src, void *dst, size_t length) {
    if (!channel || !channel->hal_handle) {
        return -1;
    }
    dma_hal_start(channel->hal_handle, (uintptr_t)src, (uintptr_t)dst, length);
    return 0;
}

void dma_stop(dma_channel_t channel) {
    if (channel && channel->hal_handle) {
        dma_hal_stop(channel->hal_handle);
    }
}
