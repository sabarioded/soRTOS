#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>
#include "queue.h"

/**
 * @brief Opaque Handle for the UART port.
 */
typedef struct uart_context* uart_port_t;

/**
 * @brief Sets up the UART hardware with the specific settings provided.
 * This prepares the port for sending and receiving data.
 * Allocates memory for the UART context.
 * 
 * @param hal_handle Pointer to the low-level hardware handle (passed to HAL).
 * @param rx_buf Pointer to the receive buffer.
 * @param rx_size Size of the receive buffer.
 * @param tx_buf Pointer to the transmit buffer.
 * @param tx_size Size of the transmit buffer.
 * @param config Pointer to the configuration settings.
 * @param clock_freq The frequency of the source clock feeding this peripheral (in Hz).
 * @return Pointer to the allocated UART handle, or NULL on failure.
 */
uart_port_t uart_create(void *hal_handle, 
                        uint8_t *rx_buf, 
                        size_t rx_size, 
                        uint8_t *tx_buf, 
                        size_t tx_size, 
                        void *config, 
                        uint32_t clock_freq);

/**
 * @brief Initializes a UART context in a user-provided memory block.
 * 
 * @param memory_block Pointer to a block of memory of size uart_get_context_size().
 * @param hal_handle Pointer to the low-level hardware handle.
 * @param rx_buf Pointer to the receive buffer.
 * @param rx_size Size of the receive buffer.
 * @param tx_buf Pointer to the transmit buffer.
 * @param tx_size Size of the transmit buffer.
 * @param config Pointer to the configuration settings.
 * @param clock_freq The frequency of the source clock.
 * @return Handle to the initialized UART port.
 */
uart_port_t uart_init(void *memory_block, 
                      void *hal_handle, 
                      uint8_t *rx_buf, 
                      size_t rx_size, 
                      uint8_t *tx_buf, 
                      size_t tx_size, 
                      void *config, 
                      uint32_t clock_freq);

/**
 * @brief Frees the memory allocated for the UART context.
 * @param port Handle to the UART port.
 */
void uart_destroy(uart_port_t port);

/**
 * @brief Get the HAL handle associated with the UART port.
 * @param port Handle to the UART port.
 * @return Pointer to the HAL handle.
 */
void *uart_get_hal_handle(uart_port_t port);

/**
 * @brief Checks if there is any data sitting in the receive buffer waiting to be read.
 * @param port Handle to the UART port.
 * @return The number of bytes currently available.
 */
int uart_available(uart_port_t port);

/**
 * @brief Pulls data out of the internal receive buffer and copies it into your application's buffer.
 * @param port Handle to the UART port.
 * @param buf The buffer where the data will be stored.
 * @param len The maximum number of bytes to read.
 * @return The number of bytes actually read and stored in 'buf'.
 */
int uart_read_buffer(uart_port_t port, char *buf, size_t len);

/**
 * @brief Queues data to be sent out.
 * The actual transmission happens in the background using interrupts, so this function returns quickly.
 * 
 * @param port Handle to the UART port.
 * @param buf The data to send.
 * @param len The number of bytes to send.
 * @return The number of bytes successfully added to the transmit queue.
 */
int uart_write_buffer(uart_port_t port, const char *buf, size_t len);

/**
 * @brief Turns the Receive Interrupt on or off.
 * @param port Handle to the UART port.
 * @param enable 1 to enable, 0 to disable.
 */
void uart_enable_rx_interrupt(uart_port_t port, uint8_t enable);

/**
 * @brief Turns the Transmit Interrupt on or off.
 * @param port Handle to the UART port.
 * @param enable 1 to enable, 0 to disable.
 */
void uart_enable_tx_interrupt(uart_port_t port, uint8_t enable);

/**
 * @brief Register a task to be notified when a byte is received.
 * @param port Handle to the UART port.
 * @param task_id The ID of the task to notify (0 to disable).
 */
void uart_set_rx_notify_task(uart_port_t port, uint16_t task_id);

/**
 * @brief Register a queue to receive incoming bytes.
 * @param port Handle to the UART port.
 * @param q Pointer to the queue (NULL to disable).
 */
void uart_set_rx_queue(uart_port_t port, queue_t *q);

/**
 * @brief Register a queue to source outgoing bytes (TX).
 * @param port Handle to the UART port.
 * @param q Pointer to the queue (NULL to disable).
 */
void uart_set_tx_queue(uart_port_t port, queue_t *q);

/**
 * @brief Called by the HAL when a byte is received.
 * @param port Handle to the UART port.
 * @param byte The byte received.
 */
void uart_core_rx_callback(uart_port_t port, uint8_t byte);

/**
 * @brief Called by the HAL when the transmitter is ready for more data.
 * @param port Handle to the UART port.
 * @param byte Pointer to store the byte to transmit.
 * @return 1 if a byte was provided, 0 if the queue is empty (HAL should disable TX IRQ).
 */
int uart_core_tx_callback(uart_port_t port, uint8_t *byte);

/**
 * @brief Called by the HAL when a receive error occurs.
 * @param port Handle to the UART port.
 */
void uart_core_rx_error_callback(uart_port_t port);

/**
 * @brief Get the size of the UART context structure.
 * This is useful when the application wants to allocate memory for the context.
 * @return Size of struct uart_context in bytes.
 */
size_t uart_get_context_size(void);

#endif /* UART_H */
