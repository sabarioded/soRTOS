#ifndef UART_HAL_NATIVE_H
#define UART_HAL_NATIVE_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    UART_WORDLENGTH_8B = 0,
    UART_WORDLENGTH_9B
} UART_WordLength_t;

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} UART_Parity_t;

typedef enum {
    UART_STOPBITS_1 = 0,
    UART_STOPBITS_2
} UART_StopBits_t;

typedef struct {
    uint32_t BaudRate;
    UART_WordLength_t WordLength;
    UART_Parity_t Parity;
    UART_StopBits_t StopBits;
    uint8_t OverSampling8;
} UART_Config_t;

void uart_hal_init(void *hal_handle, void *config_ptr, uint32_t clock_freq);
void uart_hal_enable_rx_interrupt(void *hal_handle, uint8_t enable);
void uart_hal_enable_tx_interrupt(void *hal_handle, uint8_t enable);
void uart_hal_write_byte(void *hal_handle, uint8_t byte);
int uart_hal_start_tx_dma(void *hal_handle, const uint8_t *buf, size_t len, void (*done_cb)(void *), void *cb_arg);
int uart_hal_start_rx_dma(void *hal_handle, uint8_t *buf, size_t len, void (*done_cb)(void *), void *cb_arg);

#endif /* UART_HAL_NATIVE_H */
