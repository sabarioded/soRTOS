#ifndef SPI_H
#define SPI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque Handle for the SPI port.
 */
typedef struct spi_context *spi_port_t;

/**
 * @brief SPI Callback function type.
 * @param arg User-provided argument passed to spi_transfer_async.
 */
typedef void (*spi_callback_t)(void *arg);

/**
 * @brief Initialize the SPI port.
 * Allocates memory for the SPI context.
 *
 * @param hal_handle Pointer to the low-level hardware handle.
 * @param config Pointer to the configuration structure (defined in spi_hal.h).
 * @return spi_port_t Handle to the initialized SPI port, or NULL on failure.
 */
spi_port_t spi_create(void *hal_handle, void *config);

/**
 * @brief Initialize a SPI context in a user-provided memory block.
 *
 * @param memory_block Pointer to a block of memory of size spi_get_context_size().
 * @param hal_handle Pointer to the low-level hardware handle.
 * @param config Pointer to the configuration structure.
 * @return spi_port_t Handle to the initialized SPI port.
 */
spi_port_t spi_init(void *memory_block, void *hal_handle, void *config);

/**
 * @brief Destroy the SPI port and free resources.
 * @param port Handle to the SPI port.
 */
void spi_destroy(spi_port_t port);

/**
 * @brief Transmit and Receive data simultaneously.
 *
 * @param port Handle to the SPI port.
 * @param tx_data Pointer to the data to transmit (can be NULL to send dummy
 * bytes).
 * @param rx_data Pointer to the buffer to receive data (can be NULL to discard
 * received data).
 * @param len Number of bytes to transfer.
 * @return int 0 on success, -1 on error.
 */
int spi_transfer(spi_port_t port, 
                const uint8_t *tx_data, 
                uint8_t *rx_data,
                size_t len);

/**
 * @brief Transmit and Receive data asynchronously.
 *
 * @param port Handle to the SPI port.
 * @param tx_data Pointer to the data to transmit.
 * @param rx_data Pointer to the buffer to receive data.
 * @param len Number of bytes to transfer.
 * @param callback Function to call when transfer completes.
 * @param arg Argument to pass to the callback.
 * @return int 0 on success, -1 on error.
 */
int spi_transfer_async(spi_port_t port, 
                       const uint8_t *tx_data,
                       uint8_t *rx_data, 
                       size_t len, 
                       spi_callback_t callback,
                       void *arg);

/**
 * @brief Get the size of the SPI context structure.
 * @return size_t Size in bytes.
 */
size_t spi_get_context_size(void);

/**
 * @brief SPI Interrupt Handler.
 * Called by the platform ISR.
 * @param port Handle to the SPI port.
 */
void spi_core_irq_handler(spi_port_t port);

#ifdef __cplusplus
}
#endif

#endif /* SPI_H */
