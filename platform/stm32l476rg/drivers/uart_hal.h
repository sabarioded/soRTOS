#ifndef UART_HAL_STM32_H
#define UART_HAL_STM32_H

#include "device_registers.h"
#include "gpio.h"
#include "gpio_hal.h"
#include "uart.h"

/* RCC clock enable bits for USART/UART peripherals */
#define RCC_APB1ENR1_USART2EN       (1UL << 17)
#define RCC_APB1ENR1_USART3EN       (1UL << 18)
#define RCC_APB1ENR1_UART4EN        (1UL << 19)
#define RCC_APB1ENR1_UART5EN        (1UL << 20)
#define RCC_APB2ENR_USART1EN        (1UL << 14)
#define RCC_APB1ENR2_LPUART1EN      (1UL << 0)

/* USART control register 1 (CR1) bit definitions */
#define USART_CR1_UE                (1UL << 0)
#define USART_CR1_RE                (1UL << 2)
#define USART_CR1_TE                (1UL << 3)
#define USART_CR1_RXNEIE            (1UL << 5)
#define USART_CR1_TXEIE             (1UL << 7)
#define USART_CR1_PS                (1UL << 9)
#define USART_CR1_PCE               (1UL << 10)
#define USART_CR1_M0                (1UL << 12)
#define USART_CR1_OVER8             (1UL << 15)
#define USART_CR1_M1                (1UL << 28)

/* USART interrupt/status register (ISR) flags */
#define USART_ISR_PE                (1UL << 0)
#define USART_ISR_FE                (1UL << 1)
#define USART_ISR_NE                (1UL << 2)
#define USART_ISR_ORE               (1UL << 3)
#define USART_ISR_RXNE              (1UL << 5)
#define USART_ISR_TXE               (1UL << 7)

/* USART interrupt flag clear register (ICR) bits */
#define USART_ICR_PECF              (1UL << 0)
#define USART_ICR_FECF              (1UL << 1)
#define USART_ICR_NECF              (1UL << 2)
#define USART_ICR_ORECF             (1UL << 3)

/* USART control register 2 (CR2) stop-bit field */
#define USART_CR2_STOP_Pos          (12)
#define USART_CR2_STOP_Msk          (3UL << USART_CR2_STOP_Pos)

typedef enum {
    UART_WORDLENGTH_8B = 0, /* Standard 8-bit data mode. */
    UART_WORDLENGTH_9B      /* 9-bit data mode. */
} UART_WordLength_t;

typedef enum {
    UART_PARITY_NONE = 0,   /* No parity check. */
    UART_PARITY_EVEN,       /* Even parity bit added. */
    UART_PARITY_ODD         /* Odd parity bit added. */
} UART_Parity_t;

typedef enum {
    UART_STOPBITS_1 = 0,    /* Single stop bit. */
    UART_STOPBITS_2         /* Two stop bits. */
} UART_StopBits_t;

typedef struct {
    uint32_t BaudRate;            /* The speed of communication in bits per second (e.g., 115200). */
    UART_WordLength_t WordLength; /* Number of data bits per frame. */
    UART_Parity_t Parity;         /* Error checking mechanism. */
    UART_StopBits_t StopBits;     /* Number of bits used to indicate the end of a frame. */
    uint8_t OverSampling8;        /* Set to 1 for higher speed (8x), 0 for better noise tolerance (16x). */
} UART_Config_t;

/**
 * @brief Enable the peripheral clock and configure GPIO pins for the specified UART.
 * @param UARTx Pointer to the UART hardware register block.
 */
static inline void uart_hal_enable_clocks_and_pins(USART_TypeDef *UARTx)
{
    if (UARTx == USART1) {
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
        gpio_init(GPIO_PORT_A, 9, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
        gpio_init(GPIO_PORT_A, 10, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
    }
    else if (UARTx == USART2) {
        RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
        gpio_init(GPIO_PORT_A, 2, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
        gpio_init(GPIO_PORT_A, 3, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
    }
    else if (UARTx == USART3) {
        RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN;
        gpio_init(GPIO_PORT_B, 10, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
        gpio_init(GPIO_PORT_B, 11, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
    }
    else if (UARTx == UART4) {
        RCC->APB1ENR1 |= RCC_APB1ENR1_UART4EN;
        gpio_init(GPIO_PORT_C, 10, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
        gpio_init(GPIO_PORT_C, 11, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
    }
    else if (UARTx == UART5) {
        RCC->APB1ENR1 |= RCC_APB1ENR1_UART5EN;
        gpio_init(GPIO_PORT_C, 12, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
        gpio_init(GPIO_PORT_D, 2, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
    }
    else if (UARTx == LPUART1) {
        RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;
        gpio_init(GPIO_PORT_C, 1, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
        gpio_init(GPIO_PORT_C, 0, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
    }
}

/* HAL Implementation */

/**
 * @brief Initialize the UART hardware with the given configuration.
 * 
 * @param hal_handle Pointer to the UART hardware register block (USART_TypeDef*).
 * @param config_ptr Pointer to the UART_Config_t structure.
 * @param clock_freq Frequency of the clock source feeding the UART peripheral.
 */
static inline void uart_hal_init(void *hal_handle, void *config_ptr, uint32_t clock_freq)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)hal_handle;
    UART_Config_t *config = (UART_Config_t *)config_ptr;
    /* Validate args */
    if ((UARTx == NULL) || (config == NULL) || (config->BaudRate == 0U)) {
        return;
    }

    /* Enable peripheral clock and configure GPIO pins */
    uart_hal_enable_clocks_and_pins(UARTx);

    /* Disable UART */
    UARTx->CR1 &= ~USART_CR1_UE;

    /* Configure CR1 */
    uint32_t cr1 = UARTx->CR1;
    cr1 &= ~(USART_CR1_M0 | USART_CR1_M1 | USART_CR1_PCE | USART_CR1_PS | USART_CR1_OVER8);
    
    if (config->WordLength == UART_WORDLENGTH_9B) {
        /* 9-bit word length uses M0=1, M1=0 */
        cr1 |= USART_CR1_M0;
    }
    
    if (config->Parity != UART_PARITY_NONE) {
        /* Enable parity and select odd/even */
        cr1 |= USART_CR1_PCE;
        if (config->Parity == UART_PARITY_ODD) {
            cr1 |= USART_CR1_PS;
        }
    }

    if (UARTx != LPUART1 && config->OverSampling8) {
        /* LPUART1 does not support oversampling by 8 */
        cr1 |= USART_CR1_OVER8;
    }

    UARTx->CR1 = cr1;

    /* Configure Stop Bits */
    UARTx->CR2 &= ~USART_CR2_STOP_Msk;
    if (config->StopBits == UART_STOPBITS_2) {
        UARTx->CR2 |= (2UL << USART_CR2_STOP_Pos);
    }

    /* Baud Rate */
    uint32_t baud = config->BaudRate;
    uint32_t usartdiv;

    if (UARTx == LPUART1) {
        /* LPUART uses a different BRR encoding (256x) */
        usartdiv = (uint32_t)( ((uint64_t)clock_freq * 256U + (baud / 2U)) / baud );
        UARTx->BRR = usartdiv;
    } else {
        if ((UARTx->CR1 & USART_CR1_OVER8) == 0U) {
            /* Oversampling by 16 */
            usartdiv = (clock_freq + (baud / 2U)) / baud;
            UARTx->BRR = usartdiv;
        } else {
            /* Oversampling by 8 */
            usartdiv = ((clock_freq * 2U) + (baud / 2U)) / baud;
            UARTx->BRR = (usartdiv & 0xFFF0U) | ((usartdiv & 0x000FU) >> 1U);
        }
    }

    /* Enable */
    UARTx->CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
}

/**
 * @brief Enable or disable the Receive Not Empty (RXNE) interrupt.
 * @param hal_handle Pointer to the UART hardware register block.
 * @param enable 1 to enable, 0 to disable.
 */
static inline void uart_hal_enable_rx_interrupt(void *hal_handle, uint8_t enable)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)hal_handle;
    if (UARTx) {
        if (enable) {
            UARTx->CR1 |= USART_CR1_RXNEIE;
        }
        else {
            UARTx->CR1 &= ~USART_CR1_RXNEIE;
        }
    }
}

/**
 * @brief Enable or disable the Transmit Empty (TXE) interrupt.
 * @param hal_handle Pointer to the UART hardware register block.
 * @param enable 1 to enable, 0 to disable.
 */
static inline void uart_hal_enable_tx_interrupt(void *hal_handle, uint8_t enable)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)hal_handle;
    if (UARTx) {
        if (enable) {
            UARTx->CR1 |= USART_CR1_TXEIE;
        }
        else {
            UARTx->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}

/**
 * @brief Write a single byte to the UART Transmit Data Register.
 * @param hal_handle Pointer to the UART hardware register block.
 * @param byte The byte to transmit.
 */
static inline void uart_hal_write_byte(void *hal_handle, uint8_t byte) {
    USART_TypeDef *UARTx = (USART_TypeDef *)hal_handle;
    if (UARTx) {
        UARTx->TDR = byte;
    }
}

/**
 * @brief Generic ISR handler to be called from platform-specific ISRs.
 * 
 * @param hal_handle Pointer to the UART hardware register block.
 * @param port Handle to the UART port context.
 */
static inline void uart_hal_irq_handler(void *hal_handle, uart_port_t port)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)hal_handle;
    if (!UARTx || !port) {
        return;
    }

    /* Error Handling */
    if (UARTx->ISR & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) {
        uart_core_rx_error_callback(port);
        UARTx->ICR = USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF;
    }

    /* RX */
    if (UARTx->ISR & USART_ISR_RXNE) {
        uint8_t b = (uint8_t)(UARTx->RDR & 0xFFU);
        uart_core_rx_callback(port, b);
    }

    /* TX */
    if ((UARTx->ISR & USART_ISR_TXE) && (UARTx->CR1 & USART_CR1_TXEIE)) {
        uint8_t b;
        if (uart_core_tx_callback(port, &b)) {
            UARTx->TDR = b;
        } else {
            UARTx->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}

#endif /* UART_HAL_STM32_H */
