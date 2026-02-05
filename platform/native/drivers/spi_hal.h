#ifndef SPI_HAL_NATIVE_H
#define SPI_HAL_NATIVE_H

#include <stdint.h>
#include <stddef.h>

/* Minimal SPI handle for native stubs */
typedef struct {
    uint8_t last_tx;
    uint8_t last_rx;
} SPI_Handle_t;

extern SPI_Handle_t SPI1_Inst;
extern SPI_Handle_t SPI2_Inst;

#define SPI1 (&SPI1_Inst)
#define SPI2 (&SPI2_Inst)

void spi_hal_init(void *hal_handle, void *config_ptr);
uint8_t spi_hal_transfer_byte(void *hal_handle, uint8_t byte);
void spi_hal_enable_rx_irq(void *hal_handle, uint8_t enable);
void spi_hal_enable_tx_irq(void *hal_handle, uint8_t enable);

/* Status helpers for ISR-driven transfers (native stubs) */
static inline uint8_t spi_hal_rx_ready(void *hal_handle) {
    (void)hal_handle;
    return 0;
}

static inline uint8_t spi_hal_tx_ready(void *hal_handle) {
    (void)hal_handle;
    return 0;
}

static inline uint8_t spi_hal_read_byte(void *hal_handle) {
    (void)hal_handle;
    return 0;
}

static inline void spi_hal_write_byte(void *hal_handle, uint8_t byte) {
    SPI_Handle_t *SPIx = (SPI_Handle_t *)hal_handle;
    if (SPIx) {
        SPIx->last_tx = byte;
        SPIx->last_rx = byte;
    }
}
#endif /* SPI_HAL_NATIVE_H */
