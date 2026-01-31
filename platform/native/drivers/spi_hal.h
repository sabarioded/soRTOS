#ifndef SPI_HAL_NATIVE_H
#define SPI_HAL_NATIVE_H

#include <stdint.h>

/* Minimal SPI register model for native simulation */
typedef struct {
    uint32_t CR1;
    uint32_t CR2;
    uint32_t SR;
    uint32_t DR;
} SPI_TypeDef;

extern SPI_TypeDef SPI1_Inst;
extern SPI_TypeDef SPI2_Inst;

#define SPI1 (&SPI1_Inst)
#define SPI2 (&SPI2_Inst)

#define SPI_SR_RXNE (1U << 0)
#define SPI_SR_TXE (1U << 1)

void spi_hal_init(void *hal_handle, void *config_ptr);
uint8_t spi_hal_transfer_byte(void *hal_handle, uint8_t byte);
void spi_hal_enable_rx_irq(void *hal_handle, uint8_t enable);
void spi_hal_enable_tx_irq(void *hal_handle, uint8_t enable);

#endif /* SPI_HAL_NATIVE_H */
