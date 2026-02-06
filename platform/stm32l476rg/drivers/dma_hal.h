#ifndef DMA_HAL_STM32_H
#define DMA_HAL_STM32_H

#include "device_registers.h"
#include <stdint.h>
#include <stddef.h>

/* RCC Definitions for DMA */
#define RCC_AHB1ENR_DMA1EN      (1U << 0)
#define RCC_AHB1ENR_DMA2EN      (1U << 1)

/* DMA Channel Configuration Register (CCR) Bits */
#define DMA_CCR_EN              (1U << 0)
#define DMA_CCR_TCIE            (1U << 1)
#define DMA_CCR_HTIE            (1U << 2)
#define DMA_CCR_TEIE            (1U << 3)
#define DMA_CCR_DIR             (1U << 4)
#define DMA_CCR_CIRC            (1U << 5)
#define DMA_CCR_PINC            (1U << 6)
#define DMA_CCR_MINC            (1U << 7)
#define DMA_CCR_PSIZE_Pos       (8U)
#define DMA_CCR_MSIZE_Pos       (10U)
#define DMA_CCR_PL_Pos          (12U)
#define DMA_CCR_MEM2MEM         (1U << 14)

/* DMA Request Selection (CSELR) */
/* Note: On STM32L4, CSELR is at offset 0xA8 from DMA base */
#define DMA_CSELR_OFFSET        (0xA8U)

typedef enum {
    DMA_DIR_PERIPH_TO_MEM = 0,
    DMA_DIR_MEM_TO_PERIPH = 1,
    DMA_DIR_MEM_TO_MEM    = 2
} DMA_Direction_t;

typedef enum {
    DMA_WIDTH_8BIT  = 0,
    DMA_WIDTH_16BIT = 1,
    DMA_WIDTH_32BIT = 2
} DMA_DataWidth_t;

typedef enum {
    DMA_PRIORITY_LOW      = 0,
    DMA_PRIORITY_MEDIUM   = 1,
    DMA_PRIORITY_HIGH     = 2,
    DMA_PRIORITY_VERYHIGH = 3
} DMA_Priority_t;

typedef struct {
    DMA_TypeDef *DmaBase;       /* DMA1 or DMA2 */
    uint8_t      ChannelIndex;  /* 1 to 7 */
    uint8_t      Request;       /* Request ID (0-7) for CSELR mapping */
    
    DMA_Direction_t Direction;
    DMA_DataWidth_t DataWidth;  /* Assuming same for source and dest for simplicity */
    DMA_Priority_t  Priority;
    
    uint8_t CircularMode;       /* 1 for circular, 0 for normal */
    uint8_t IncrementSrc;       /* 1 to increment source address */
    uint8_t IncrementDst;       /* 1 to increment destination address */
} DMA_Config_t;

typedef struct {
    DMA_Channel_TypeDef *Channel;
    DMA_Direction_t Direction;
} DMA_Handle_t;

/**
 * @brief Initialize the DMA hardware channel.
 */
static inline void dma_hal_init(void *hal_handle, void *config_ptr) {
    DMA_Handle_t *handle = (DMA_Handle_t *)hal_handle;
    DMA_Config_t *cfg = (DMA_Config_t *)config_ptr;

    if (!handle || !handle->Channel || !cfg || !cfg->DmaBase) {
        return;
    }

    DMA_Channel_TypeDef *dma_channel = handle->Channel;
    handle->Direction = cfg->Direction;

    /* 1. Enable DMA Clock */
    if (cfg->DmaBase == DMA1) {
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    } else if (cfg->DmaBase == DMA2) {
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    }

    /* 2. Configure Request Selection (CSELR) */
    /* CSELR register is at offset 0xA8 from DMA base */
    volatile uint32_t *cselr = (volatile uint32_t *)((uint32_t)cfg->DmaBase + DMA_CSELR_OFFSET);
    
    /* Each channel has 4 bits in CSELR. Channel 1 is bits 0-3, Channel 7 is 24-27 */
    uint32_t shift = (cfg->ChannelIndex - 1) * 4;
    uint32_t mask = 0xF << shift;
    
    *cselr &= ~mask;
    *cselr |= (cfg->Request << shift);

    /* 3. Configure Channel (CCR) */
    /* Ensure channel is disabled before configuration */
    dma_channel->CCR &= ~DMA_CCR_EN;

    uint32_t ccr = 0;

    /* Direction */
    if (cfg->Direction == DMA_DIR_MEM_TO_PERIPH) {
        ccr |= DMA_CCR_DIR;
    } else if (cfg->Direction == DMA_DIR_MEM_TO_MEM) {
        ccr |= DMA_CCR_MEM2MEM;
    }

    /* Data Width (PSIZE and MSIZE set to same for now) */
    ccr |= (cfg->DataWidth << DMA_CCR_PSIZE_Pos);
    ccr |= (cfg->DataWidth << DMA_CCR_MSIZE_Pos);

    /* Priority */
    ccr |= (cfg->Priority << DMA_CCR_PL_Pos);

    /* Increment Mode */
    if (cfg->IncrementSrc) {
        /* If Mem2Periph: Mem is Src (MINC). If Periph2Mem: Periph is Src (PINC? No, Periph is always P) */
        /* STM32 Logic: 
           DIR=0 (P->M): Source is Periph (PINC), Dest is Mem (MINC)
           DIR=1 (M->P): Source is Mem (MINC), Dest is Periph (PINC)
        */
        if (cfg->Direction == DMA_DIR_MEM_TO_PERIPH) ccr |= DMA_CCR_MINC;
        else ccr |= DMA_CCR_PINC;
    }
    
    if (cfg->IncrementDst) {
        if (cfg->Direction == DMA_DIR_MEM_TO_PERIPH) ccr |= DMA_CCR_PINC;
        else ccr |= DMA_CCR_MINC;
    }

    if (cfg->CircularMode) {
        ccr |= DMA_CCR_CIRC;
    }

    dma_channel->CCR = ccr;
}

/**
 * @brief Start a DMA transfer.
 */
static inline void dma_hal_start(void *hal_handle, uintptr_t src, uintptr_t dst, size_t length) {
    DMA_Handle_t *handle = (DMA_Handle_t *)hal_handle;
    if (!handle || !handle->Channel) {
        return;
    }
    DMA_Channel_TypeDef *dma_channel = handle->Channel;
    
    /* Disable channel to configure addresses */
    dma_channel->CCR &= ~DMA_CCR_EN;

    dma_channel->CNDTR = (uint32_t)length;
    /* Map src/dst based on configured direction. */
    if (handle->Direction == DMA_DIR_MEM_TO_MEM) {
        /* Mem2Mem: CPAR is target, CMAR is source */
        dma_channel->CPAR = (uint32_t)dst;
        dma_channel->CMAR = (uint32_t)src;
    } else if (handle->Direction == DMA_DIR_MEM_TO_PERIPH) {
        /* Mem2Periph (DIR=1): CMAR -> CPAR */
        dma_channel->CMAR = (uint32_t)src;
        dma_channel->CPAR = (uint32_t)dst;
    } else {
        /* Periph2Mem (DIR=0): CPAR -> CMAR */
        dma_channel->CPAR = (uint32_t)src;
        dma_channel->CMAR = (uint32_t)dst;
    }

    /* Enable Channel */
    dma_channel->CCR |= DMA_CCR_EN;
}

/**
 * @brief Stop a DMA transfer.
 */
static inline void dma_hal_stop(void *hal_handle) {
    DMA_Handle_t *handle = (DMA_Handle_t *)hal_handle;
    if (!handle || !handle->Channel) return;
    DMA_Channel_TypeDef *dma_channel = handle->Channel;
    dma_channel->CCR &= ~DMA_CCR_EN;
}

#endif /* DMA_HAL_STM32_H */
