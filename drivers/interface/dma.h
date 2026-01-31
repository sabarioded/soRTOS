#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque Handle for a DMA channel.
 */
typedef struct dma_context* dma_channel_t;

/**
 * @brief Create and initialize a DMA channel.
 * Allocates memory for the DMA context.
 * 
 * @param hal_handle Pointer to the low-level hardware handle (e.g., DMA1_Channel1).
 * @param config Pointer to the configuration structure (defined in dma_hal.h).
 * @return dma_channel_t Handle to the initialized DMA channel, or NULL on failure.
 */
dma_channel_t dma_create(void *hal_handle, void *config);

/**
 * @brief Initialize a DMA context in a user-provided memory block.
 * 
 * @param memory_block Pointer to a block of memory of size dma_get_context_size().
 * @param hal_handle Pointer to the low-level hardware handle.
 * @param config Pointer to the configuration structure.
 * @return dma_channel_t Handle to the initialized DMA channel.
 */
dma_channel_t dma_init(void *memory_block, void *hal_handle, void *config);

/**
 * @brief Destroy the DMA channel and free resources.
 * @param channel Handle to the DMA channel.
 */
void dma_destroy(dma_channel_t channel);

/**
 * @brief Start a DMA transfer.
 * 
 * @param channel Handle to the DMA channel.
 * @param src Source address.
 * @param dst Destination address.
 * @param length Number of data items to transfer.
 * @return int 0 on success, -1 on error.
 */
int dma_start(dma_channel_t channel, void *src, void *dst, size_t length);

/**
 * @brief Stop an ongoing DMA transfer.
 * @param channel Handle to the DMA channel.
 */
void dma_stop(dma_channel_t channel);

/**
 * @brief Get the size of the DMA context structure.
 * @return size_t Size in bytes.
 */
size_t dma_get_context_size(void);

#ifdef __cplusplus
}
#endif

#endif /* DMA_H */