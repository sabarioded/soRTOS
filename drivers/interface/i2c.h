#ifndef I2C_H
#define I2C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque Handle for the I2C port.
 */
typedef struct i2c_context *i2c_port_t;

/**
 * @brief I2C Callback function type.
 * @param arg User-provided argument.
 * @param status Completion status (I2C_STATUS_OK or error code).
 */
typedef enum {
    I2C_STATUS_OK = 0,
    I2C_STATUS_ERR = -1,
    I2C_STATUS_NACK = -2
} i2c_status_t;

typedef void (*i2c_callback_t)(void *arg, i2c_status_t status);

/**
 * @brief Create and initialize an I2C port.
 * Allocates memory for the I2C context.
 *
 * @param hal_handle Pointer to the low-level hardware handle.
 * @param config Pointer to the configuration structure (defined in i2c_hal.h).
 * @return i2c_port_t Handle to the initialized I2C port, or NULL on failure.
 */
i2c_port_t i2c_create(void *hal_handle, void *config);

/**
 * @brief Initialize an I2C context in a user-provided memory block.
 *
 * @param memory_block Pointer to a block of memory of size.
 * @param hal_handle Pointer to the low-level hardware handle.
 * @param config Pointer to the configuration structure.
 * @return i2c_port_t Handle to the initialized I2C port.
 */
i2c_port_t i2c_init(void *memory_block, void *hal_handle, void *config);

/**
 * @brief Destroy the I2C port and free resources.
 * @param port Handle to the I2C port.
 */
void i2c_destroy(i2c_port_t port);

/**
 * @brief Transmit data in Master mode.
 *
 * @param port Handle to the I2C port.
 * @param addr 7-bit Slave Address.
 * @param data Pointer to the data buffer.
 * @param len Number of bytes to transmit.
 *            Maximum length is platform-specific and enforced by the HAL.
 * @return int 0 on success, -1 on error.
 */
int i2c_master_transmit(i2c_port_t port, 
                        uint16_t addr, 
                        const uint8_t *data,
                        size_t len);

/**
 * @brief Receive data in Master mode.
 *
 * @param port Handle to the I2C port.
 * @param addr 7-bit Slave Address.
 * @param data Pointer to the buffer to store received data.
 * @param len Number of bytes to receive.
 *            Maximum length is platform-specific and enforced by the HAL.
 * @return int 0 on success, -1 on error.
 */
int i2c_master_receive(i2c_port_t port, 
                       uint16_t addr, 
                       uint8_t *data,
                       size_t len);

/**
 * @brief Transmit data asynchronously in Master mode.
 *
 * @param port Handle to the I2C port.
 * @param addr 7-bit Slave Address.
 * @param data Pointer to the data buffer.
 * @param len Number of bytes to transmit.
 * @param callback Function to call when transfer completes.
 * @param arg User argument.
 * @note Async support and maximum transfer length are platform-specific.
 * @return int 0 on success, -1 on error.
 */
int i2c_master_transmit_async(i2c_port_t port, 
                              uint16_t addr,
                              const uint8_t *data, 
                              size_t len,
                              i2c_callback_t callback, 
                              void *arg);

/**
 * @brief Receive data asynchronously in Master mode.
 *
 * @param port Handle to the I2C port.
 * @param addr 7-bit Slave Address.
 * @param data Pointer to the buffer to store received data.
 * @param len Number of bytes to receive.
 * @param callback Function to call when transfer completes.
 * @param arg User argument.
 * @note Async support and maximum transfer length are platform-specific.
 * @return int 0 on success, -1 on error.
 */
int i2c_master_receive_async(i2c_port_t port, 
                             uint16_t addr, 
                             uint8_t *data,
                             size_t len, 
                             i2c_callback_t callback, 
                             void *arg);

/**
 * @brief Get the size of the I2C context structure.
 * @return size_t Size in bytes.
 */
size_t i2c_get_context_size(void);

/**
 * @brief I2C Event Interrupt Handler.
 *
 * @param port Handle to the I2C port.
 */
void i2c_core_ev_irq_handler(i2c_port_t port);

/**
 * @brief I2C Error Interrupt Handler.
 *
 * @param port Handle to the I2C port.
 */
void i2c_core_er_irq_handler(i2c_port_t port);

#ifdef __cplusplus
}
#endif

#endif /* I2C_H */
