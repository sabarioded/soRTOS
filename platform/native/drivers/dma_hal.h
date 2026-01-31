#ifndef DMA_HAL_NATIVE_H
#define DMA_HAL_NATIVE_H

#include <stdint.h>
#include <stddef.h>

void dma_hal_init(void *hal_handle, void *config_ptr);
void dma_hal_start(void *hal_handle, uint32_t src, uint32_t dst, uint32_t length);
void dma_hal_stop(void *hal_handle);

#endif /* DMA_HAL_NATIVE_H */
