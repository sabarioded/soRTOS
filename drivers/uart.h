#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "device_registers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes */
#define UART_OK             (0)
#define UART_FAIL           (-1)
#define UART_ERR_TIMEOUT    (-2)
#define UART_ERR_OVERRUN    (-3)
#define UART_ERR_FRAMING    (-4)
#define UART_ERR_NOISE      (-5)

/* Max number of iterations for busy-wait loops */
#define UART_MAX_ITERATIONS     10000U

/**
 * @brief UART word length selection
 *
 * Matches STM32L4 encoding using CR1 M0/M1 bits:
 *  - 8-bit: M1=0, M0=0
 *  - 9-bit: M1=0, M0=1
 *  - 7-bit: M1=1, M0=0
 */
typedef enum {
    UART_WORDLENGTH_8B = 0,
    UART_WORDLENGTH_9B = 1,
    UART_WORDLENGTH_7B = 2,
} UART_WordLength_t;

/**
 * @brief Simple UART config structure
 *
 * This is kept for convenience if you ever want
 * a minimal configuration (Baud+word length only).
 */
typedef struct {
    uint32_t          BaudRate;
    UART_WordLength_t WordLength;
} UART_Config_t;

/** Parity options */
typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN = 1,
    UART_PARITY_ODD  = 2,
} UART_Parity_t;

/** Stop bits */
typedef enum {
    UART_STOPBITS_1 = 0,
    UART_STOPBITS_2 = 1,
} UART_StopBits_t;

/** UART config */
typedef struct {
    uint32_t          BaudRate;
    UART_WordLength_t WordLength;   ///< 7/8/9
    UART_Parity_t     Parity;       ///< none/even/odd
    UART_StopBits_t   StopBits;     ///< 1 or 2
    uint8_t           OverSampling8;///< 0 -> 16x, non-zero -> 8x
} UART_Config_t;

/**
 * @brief Initialize a UART/USART peripheral.
 *
 * @param UARTx            Pointer to USART instance (USART1, USART2, USART3, UART4, UART5, LPUART1)
 * @param config           Pointer to UART_Config_t structure
 * @param periph_clock_hz  Peripheral clock frequency (PCLK for that UART)
 *
 * @return UART_OK on success, negative error code on failure
 */
int uart_init(USART_t *UARTx, const UART_Config_t *config, uint32_t periph_clock_hz);

/**
 * @brief Send one character.
 *
 * @param UARTx   Instance to use
 * @param c       Character to send
 * @return UART_OK on success, negative error on timeout/fail
 */
int uart_send_char(USART_t *UARTx, char c);

/**
 * @brief Send a null-terminated string.
 *
 * @param UARTx   Instance to use
 * @param str     Pointer to string
 * @return UART_OK on success, negative error on failure
 */
int uart_send_string(USART_t *UARTx, const char *str);

/**
 * @brief Receive one character.
 *
 * @param UARTx   Instance to use
 * @param result  Pointer to store received character
 * @return UART_OK on success, negative error on failure
 */
int uart_receive_char(USART_t *UARTx, char *result);

/**
 * @brief Receive a line/string into a buffer.
 *
 * Reads characters until:
 *  - '\n' or '\r' is received, or
 *  - max_length - 1 characters are stored
 *
 * Always null-terminates the buffer.
 *
 * @param UARTx       Instance to use
 * @param buffer      Destination buffer
 * @param max_length  Max number of bytes to store, including '\0'
 * @return UART_OK on success, negative error on failure
 */
int uart_receive_string(USART_t *UARTx, char *buffer, uint32_t max_length);

/**
 * @brief Register a per-UART RX callback (called from ISR when new byte arrives).
 *
 * @param UARTx UART instance
 * @param cb    Callback function, or NULL to clear
 */
void uart_set_rx_callback(USART_t *UARTx, void (*cb)(char c));

/**
 * @brief Enable or disable RX interrupt (RXNEIE). The actual IRQ must
 * be enabled in the NVIC by the application (this function only toggles
 * the peripheral interrupt enable bit).
 */
void uart_enable_rx_interrupt(USART_t *UARTx, int enable);

/**
 * @brief Read up to `len` bytes from the driver's RX circular buffer.
 * Returns number of bytes actually copied.
 */
uint32_t uart_read_buffer(USART_t *UARTx, char *dst, uint32_t len);

/**
 * @brief Return number of bytes currently available in RX buffer.
 */
uint32_t uart_available(USART_t *UARTx);

/**
 * @brief Generic handler that should be called from the real IRQ handler
 * corresponding to the USART instance. It handles RXNE and stores bytes
 * in the internal buffer, and invokes callbacks.
 */
void uart_irq_handler(USART_t *UARTx);

/**
 * @brief Track how many bytes have been lost due to RX buffer overflow.
 */
uint32_t uart_get_overflow_count(USART_t *UARTx);

#ifdef __cplusplus
}
#endif

#endif // UART_H
