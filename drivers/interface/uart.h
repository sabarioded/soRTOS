#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>
#include "queue.h"

/* Opaque handle for the UART port (e.g. USART1, USART2) */
typedef void* uart_port_t;

/**
 * @brief UART Word Length settings
 */
typedef enum {
    UART_WORDLENGTH_8B = 0, /**< Standard 8-bit data mode. */
    UART_WORDLENGTH_9B      /**< 9-bit data mode (often used for parity or multi-processor comms). */
} UART_WordLength_t;

/**
 * @brief UART Parity settings
 */
typedef enum {
    UART_PARITY_NONE = 0,   /**< No parity check. */
    UART_PARITY_EVEN,       /**< Even parity bit added. */
    UART_PARITY_ODD         /**< Odd parity bit added. */
} UART_Parity_t;

/**
 * @brief UART Stop Bits settings
 */
typedef enum {
    UART_STOPBITS_1 = 0,    /**< Single stop bit (standard). */
    UART_STOPBITS_2         /**< Two stop bits (useful for slower devices). */
} UART_StopBits_t;

/**
 * @brief UART Configuration Structure
 */
typedef struct {
    uint32_t BaudRate;            /**< The speed of communication in bits per second (e.g., 115200). */
    UART_WordLength_t WordLength; /**< Number of data bits per frame. */
    UART_Parity_t Parity;         /**< Error checking mechanism. */
    UART_StopBits_t StopBits;     /**< Number of bits used to indicate the end of a frame. */
    uint8_t OverSampling8;        /**< Set to 1 for higher speed (8x), 0 for better noise tolerance (16x). */
} UART_Config_t;

/* Public API */

/**
 * @brief Sets up the UART hardware with the specific settings provided.
 * This prepares the port for sending and receiving data.
 * 
 * @param port Handle to the UART port (e.g., USART2).
 * @param config Pointer to the configuration settings.
 * @param clock_freq The frequency of the source clock feeding this peripheral (in Hz).
 */
void uart_init(uart_port_t port, UART_Config_t *config, uint32_t clock_freq);

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
 * @brief Turns the Receive Interrupt (RXNE) on or off.
 * @param port Handle to the UART port.
 * @param enable 1 to enable, 0 to disable.
 */
void uart_enable_rx_interrupt(uart_port_t port, uint8_t enable);

/**
 * @brief Turns the Transmit Interrupt (TXE) on or off.
 * @param port Handle to the UART port.
 * @param enable 1 to enable, 0 to disable.
 */
void uart_enable_tx_interrupt(uart_port_t port, uint8_t enable);

/**
 * @brief The core Interrupt Service Routine handler.
 * This function should be called from the platform-specific ISR.
 * It handles the low-level details of moving data between the hardware and the ring buffers.
 * 
 * @param port Handle to the UART port.
 */
void uart_irq_handler(uart_port_t port);

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

#endif /* UART_H */