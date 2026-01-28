#include <stddef.h>      /* for NULL */
#include "uart.h"
#include "utils.h"
#include "project_config.h"
#include "platform.h"
#include "device_registers.h"
#include "arch_ops.h"
#include "gpio.h"
#include "scheduler.h"
#include "queue.h"

/*
 * RCC_APB1ENR1 – APB1 peripheral clock enable register 1
 * These bits enable the clocks for the low-speed UART/USART peripherals
 * on the APB1 bus: USART2, USART3, UART4, UART5.
*/
#define RCC_APB1ENR1_USART2EN_POS (17)                   // Bit 17: USART2 clock enable
#define RCC_APB1ENR1_USART2EN     (1UL << RCC_APB1ENR1_USART2EN_POS)

#define RCC_APB1ENR1_USART3EN_POS (18)                   // Bit 18: USART3 clock enable
#define RCC_APB1ENR1_USART3EN     (1UL << RCC_APB1ENR1_USART3EN_POS)

#define RCC_APB1ENR1_UART4EN_POS  (19)                   // Bit 19: UART4 clock enable
#define RCC_APB1ENR1_UART4EN      (1UL << RCC_APB1ENR1_UART4EN_POS)

#define RCC_APB1ENR1_UART5EN_POS  (20)                   // Bit 20: UART5 clock enable
#define RCC_APB1ENR1_UART5EN      (1UL << RCC_APB1ENR1_UART5EN_POS)


/*
 * RCC_APB2ENR – APB2 peripheral clock enable register
 * APB2 is the high-speed peripheral bus; USART1 lives here.
 */
#define RCC_APB2ENR_USART1EN_POS (14)                    // Bit 14: USART1 clock enable
#define RCC_APB2ENR_USART1EN     (1UL << RCC_APB2ENR_USART1EN_POS)


/* RCC_APB1ENR2 – APB1 peripheral clock enable register 2
 * LPUART1 (low-power UART) has its own enable bit here.
 */
#define RCC_APB1ENR2_LPUART1EN_POS (0)                   // Bit 0: LPUART1 clock enable
#define RCC_APB1ENR2_LPUART1EN     (1UL << RCC_APB1ENR2_LPUART1EN_POS)

/* CR1 – Control Register 1
 * Core configuration (enable, TX/RX enable, word length, oversampling, parity,
 * and interrupt enables).
 */ 
#define USART_CR1_UE_POS        (0)
#define USART_CR1_UE            (1UL << USART_CR1_UE_POS)    // USART enable

#define USART_CR1_RE_POS        (2)
#define USART_CR1_RE            (1UL << USART_CR1_RE_POS)    // Receiver enable

#define USART_CR1_TE_POS        (3)
#define USART_CR1_TE            (1UL << USART_CR1_TE_POS)    // Transmitter enable

#define USART_CR1_RXNEIE_POS    (5)
#define USART_CR1_RXNEIE        (1UL << USART_CR1_RXNEIE_POS) // RXNE interrupt enable (receive data ready)

#define USART_CR1_TCIE_POS      (6)
#define USART_CR1_TCIE          (1UL << USART_CR1_TCIE_POS)  // Transmission complete interrupt enable

#define USART_CR1_TXEIE_POS     (7)
#define USART_CR1_TXEIE         (1UL << USART_CR1_TXEIE_POS)  // TXE interrupt enable (transmit data register empty)

#define USART_CR1_PS_POS        (9)
#define USART_CR1_PS            (1UL << USART_CR1_PS_POS)    // Parity selection (0: even, 1: odd)

#define USART_CR1_PCE_POS       (10)
#define USART_CR1_PCE           (1UL << USART_CR1_PCE_POS)   // Parity control enable

#define USART_CR1_M0_POS        (12)
#define USART_CR1_M0            (1UL << USART_CR1_M0_POS)    // Word length bit 0 (with M1 controls 7/8/9 bits)

#define USART_CR1_OVER8_POS     (15)
#define USART_CR1_OVER8         (1UL << USART_CR1_OVER8_POS) // Oversampling mode (0: x16, 1: x8)

#define USART_CR1_M1_POS        (28)
#define USART_CR1_M1            (1UL << USART_CR1_M1_POS)    // Word length bit 1


/* ISR – Interrupt and Status Register
 * Contains the runtime flags indicating TX/RX state (and errors).
 */
#define USART_ISR_PE   (1UL << 0)   // Parity error
#define USART_ISR_FE   (1UL << 1)   // Framing error
#define USART_ISR_NE   (1UL << 2)   // Noise error
#define USART_ISR_ORE  (1UL << 3)   // Overrun error

#define USART_ISR_RXNE_POS  (5)
#define USART_ISR_RXNE      (1UL << USART_ISR_RXNE_POS)  // RXNE: read data register not empty

#define USART_ISR_TC_POS    (6)
#define USART_ISR_TC        (1UL << USART_ISR_TC_POS)    // TC: transmission complete

#define USART_ISR_TXE_POS   (7)
#define USART_ISR_TXE       (1UL << USART_ISR_TXE_POS)   // TXE: transmit data register empty


/* ICR – Interrupt Clear Register
 * Write 1 to these bits to clear the corresponding flag
 */
#define USART_ICR_TCCF      (1UL << 6)   // Transmission complete clear flag
#define USART_ICR_PECF      (1UL << 0)   // Parity error clear flag
#define USART_ICR_FECF      (1UL << 1)   // Framing error clear flag
#define USART_ICR_NECF      (1UL << 2)   // Noise error clear flag
#define USART_ICR_ORECF     (1UL << 3)   // Overrun error clear flag


/* CR2 – Control Register 2
 * STOP[13:12] select the number of stop bits in the frame.
 */
#define USART_CR2_STOP_Pos  (12)
#define USART_CR2_STOP_Msk  (3UL << USART_CR2_STOP_Pos)  // STOP field mask (00: 1 stop bit, 10: 2 stop bits)

#define UART_MAX_INSTANCES   6

#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE 128
#endif

#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE 128
#endif

/* We track these instances for buffered RX + callbacks. */
static USART_TypeDef *uart_instances[UART_MAX_INSTANCES];

/* RX circular buffers per instance */
static volatile uint8_t  uart_rx_buf[UART_MAX_INSTANCES][UART_RX_BUFFER_SIZE];
static volatile uint32_t uart_rx_head[UART_MAX_INSTANCES];
static volatile uint32_t uart_rx_tail[UART_MAX_INSTANCES];
static volatile uint32_t uart_rx_overflow[UART_MAX_INSTANCES];
static volatile uint32_t uart_rx_errors[UART_MAX_INSTANCES];  /* Error count (parity, framing, noise) */

/* TX circular buffers per instance */
static volatile uint8_t  uart_tx_buf[UART_MAX_INSTANCES][UART_RX_BUFFER_SIZE];
static volatile uint32_t uart_tx_head[UART_MAX_INSTANCES];
static volatile uint32_t uart_tx_tail[UART_MAX_INSTANCES];
static volatile uint32_t uart_tx_overflow[UART_MAX_INSTANCES];
static volatile uint16_t uart_rx_notify_tasks[UART_MAX_INSTANCES];
static queue_t * volatile uart_rx_queues[UART_MAX_INSTANCES];
static queue_t * volatile uart_tx_queues[UART_MAX_INSTANCES];


/* Find array index for a given instance */
static int uart_get_index(USART_TypeDef *UARTx)
{
    for (int i = 0; i < UART_MAX_INSTANCES; ++i) {
        if (uart_instances[i] == UARTx) {
            return i;
        }
    }
    return -1;
}


/* Register a UART instance and return its index */
static int uart_register_instance(USART_TypeDef *UARTx)
{
    for (int i = 0; i < UART_MAX_INSTANCES; ++i)
    {
        if (uart_instances[i] == UARTx) {
            return i; // already registered
        }

        if (uart_instances[i] == NULL)
        {
            uart_instances[i] = UARTx;
            uart_rx_head[i] = 0;
            uart_rx_tail[i] = 0;
            uart_rx_overflow[i] = 0;
            uart_rx_errors[i] = 0;
            uart_tx_head[i] = 0;
            uart_tx_tail[i] = 0;
            uart_tx_overflow[i] = 0;
            uart_rx_notify_tasks[i] = 0;
            uart_rx_queues[i] = NULL;
            uart_tx_queues[i] = NULL;
            return i;
        }
    }
    return -1; // no free slots
}


/* Enable peripheral clocks and configure GPIO pins for a given UARTx. */
static void uart_enable_clocks_and_pins(USART_TypeDef *UARTx)
{
    if (UARTx == USART1) {
        /* USART1: PA9 (TX), PA10 (RX), AF7 */
        /* Enable USART1 clock (APB2) */
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

        gpio_init(GPIO_PORT_A, 9, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
        gpio_init(GPIO_PORT_A, 10, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
    }
    else if (UARTx == USART2) {
        /* USART2: PA2 (TX), PA3 (RX), AF7  — Nucleo VCP */
        /* Enable USART2 clock (APB1) */
        RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;

        gpio_init(GPIO_PORT_A, 2, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
        gpio_init(GPIO_PORT_A, 3, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
    }
    else if (UARTx == USART3) {
        /* USART3: PB10 (TX), PB11 (RX), AF7 */
        RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN;

        gpio_init(GPIO_PORT_B, 10, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
        gpio_init(GPIO_PORT_B, 11, GPIO_MODE_AF, GPIO_PULL_NONE, 7);
    }
    else if (UARTx == UART4) {
        /* UART4: PC10 (TX), PC11 (RX), AF8 */
        RCC->APB1ENR1 |= RCC_APB1ENR1_UART4EN;

        gpio_init(GPIO_PORT_C, 10, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
        gpio_init(GPIO_PORT_C, 11, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
    }
    else if (UARTx == UART5) {
        /* UART5: PC12 (TX), PD2 (RX), AF8 */
        RCC->APB1ENR1 |= RCC_APB1ENR1_UART5EN;

        gpio_init(GPIO_PORT_C, 12, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
        gpio_init(GPIO_PORT_D, 2, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
    }
    else if (UARTx == LPUART1) {
        /* LPUART1: PC1 (TX), PC0 (RX), AF8 */
        RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;

        gpio_init(GPIO_PORT_C, 1, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
        gpio_init(GPIO_PORT_C, 0, GPIO_MODE_AF, GPIO_PULL_NONE, 8);
    }
}


/* Initialize UART with full config */
void uart_init(uart_port_t port, UART_Config_t *config, uint32_t clock_freq)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    if ((UARTx == NULL) || (config == NULL) || (config->BaudRate == 0U)) {
        return;
    }

    /* Enable clocks and pins */
    uart_enable_clocks_and_pins(UARTx);

    /* Disable UART before configuration */
    UARTx->CR1 &= ~USART_CR1_UE;

    /* configure word length, parity, oversampling */
    uint32_t cr1 = UARTx->CR1;

    /* Clear M0, M1, PCE, PS, OVER8
     * M0,M1:
     *  00: 8 bits
     *  01: 9 bits
     *  10: 7 bits
     * PCE   = 0:no parity          , 1:parity enabled
     * PS    = 0:even               , 1:odd
     * OVER8 = 0:oversampling by 16 , 1:oversampling by 8
    */
    cr1 &= ~(USART_CR1_M0 | USART_CR1_M1 |
             USART_CR1_PCE | USART_CR1_PS |
             USART_CR1_OVER8);
    
    /* Word length */
    switch (config->WordLength) {
        case UART_WORDLENGTH_9B:
            cr1 |= USART_CR1_M0; // M1=0, M0=1
            break;
        case UART_WORDLENGTH_8B:
            cr1 &= ~(USART_CR1_M0 | USART_CR1_M1); // M1=0, M0=0
            break;
        default:
            break; // stay the same
    }

    /* Parity */
    if (config->Parity == UART_PARITY_NONE) {
        cr1 &= ~USART_CR1_PCE; // no parity
    } else {
        cr1 |= USART_CR1_PCE;  // parity enabled

        if (config->Parity == UART_PARITY_ODD) {
            cr1 |= USART_CR1_PS;  // odd
        } else {
            cr1 &= ~USART_CR1_PS; // even
        }
    }

    /* Oversampling */
    if (UARTx != LPUART1) {
        if (config->OverSampling8) { // by 8
            cr1 |= USART_CR1_OVER8;
        } else { // by 16
            cr1 &= ~USART_CR1_OVER8;
        }
    }

    /* Write back CR1 */
    UARTx->CR1 = cr1;

    /* Stop bits */
    UARTx->CR2 &= ~USART_CR2_STOP_Msk; // clear STOP bits
    if (config->StopBits == UART_STOPBITS_2) {
        UARTx->CR2 |= (2UL << USART_CR2_STOP_Pos); // STOP = 10b = 2 stop bits
    } else {
        UARTx->CR2 |= (0UL << USART_CR2_STOP_Pos); // STOP = 00b = 1 stop bit
    }

    /* Baud rate */
    uint32_t baud = config->BaudRate;
    uint32_t usartdiv;

    if (baud == 0U) {
        return;
    }

    if (UARTx == LPUART1) {
        /* LPUART1 baud rate generation:
         * LPUARTDIV = (256 * fck) / baud
         * Use uint64_t to prevent overflow (e.g. 80MHz * 256 > 4GB)
         */
        usartdiv = (uint32_t)( ((uint64_t)clock_freq * 256U + (baud / 2U)) / baud );
        UARTx->BRR = usartdiv;
    } else {
        /* According to RM (oversampling section):
         * OVER8 = 0 (oversampling by 16):
         *      USARTDIV = fck / Baud
         *      BRR      = USARTDIV
         *
         * OVER8 = 1 (oversampling by 8):
         *      USARTDIV  = (2 * fck) / Baud
         *      BRR[15:4] = USARTDIV[15:4]
         *      BRR[2:0]  = USARTDIV[3:1]
         *      BRR[3]    = 0
         *
         *  We add baud/2 before dividing to get simple integer rounding.
         */
        if ((UARTx->CR1 & USART_CR1_OVER8) == 0U) {
            /* Oversampling by 16 */
            usartdiv = (clock_freq + (baud / 2U)) / baud;
            UARTx->BRR = usartdiv;
        } else {
            /* Oversampling by 8 */
            usartdiv = ((clock_freq * 2U) + (baud / 2U)) / baud;

            /* Map USARTDIV into BRR as described in the reference manual */
            UARTx->BRR =
                (usartdiv & 0xFFF0U) |          /* BRR[15:4] = USARTDIV[15:4], BRR[3:0]=0 */
                ((usartdiv & 0x000FU) >> 1U);   /* BRR[2:0] = USARTDIV[3:1], BRR[3] stays 0 */
        }
    }

    /* Enable transmitter and receiver */
    UARTx->CR1 |= (USART_CR1_TE | USART_CR1_RE);

    /* register instance */
    int idx = uart_register_instance(UARTx);
    if (idx < 0) {
        UARTx->CR1 &= ~(USART_CR1_TE | USART_CR1_RE); // disable TX/RX
        return;
    }

    /* Enable UART */
    UARTx->CR1 |= USART_CR1_UE;

    arch_dsb(); // barrier for I/O synchronization
}


/* Enable/disable RX interrupt */
void uart_enable_rx_interrupt(uart_port_t port, uint8_t enable)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    if (UARTx == NULL) {
        return;
    }

    if (enable) {
        UARTx->CR1 |= USART_CR1_RXNEIE;
    } else {
        UARTx->CR1 &= ~USART_CR1_RXNEIE;
    }
}


/* Return number of bytes in RX buffer */
int uart_available(uart_port_t port)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    int idx = uart_get_index(UARTx);
    if (idx < 0) {
        return 0;
    }

    uint32_t stat = arch_irq_lock();
    uint32_t head = uart_rx_head[idx];
    uint32_t tail = uart_rx_tail[idx];
    arch_irq_unlock(stat);

    return (int)((head + UART_RX_BUFFER_SIZE - tail) % UART_RX_BUFFER_SIZE);
}


/* Read bytes from RX buffer */
int uart_read_buffer(uart_port_t port, char *dst, size_t len)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    int idx = uart_get_index(UARTx);
    if ((idx < 0) || (dst == NULL) || (len == 0U)) {
        return 0;
    }

    size_t copied = 0;

    uint32_t stat = arch_irq_lock();
    uint32_t head = uart_rx_head[idx];
    uint32_t tail = uart_rx_tail[idx];

    while ((copied < len) && (head != tail)) {
        dst[copied++] = (char)uart_rx_buf[idx][tail];
        tail = (tail + 1U) % UART_RX_BUFFER_SIZE;
    }
    uart_rx_tail[idx] = tail;
    arch_irq_unlock(stat);

    return (int)copied;
}


/* function for IRQ Handlers */
void uart_irq_handler(uart_port_t port)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    int idx = uart_get_index(UARTx);
    if (idx < 0) {
        return;
    }

    /* Check & Clear Errors INDEPENDENTLY of RXNE 
     * This ensures that even if RXNE is 0, we still clear flags so the ISR can exit.
    */
    if (UARTx->ISR & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) {
        uart_rx_errors[idx]++;
        UARTx->ICR = USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF;
    }

    if (UARTx->ISR & USART_ISR_RXNE) {
        /* read the recieve data register */
        uint8_t b = (uint8_t)(UARTx->RDR & 0xFFU);

        /* If a queue is registered, push to it directly */
        if (uart_rx_queues[idx] != NULL) {
            if (queue_send_from_isr(uart_rx_queues[idx], &b) < 0) {
                uart_rx_overflow[idx]++;
            }
        } else {
            /* Put into circular buffer */
            uint32_t head = uart_rx_head[idx];
            uint32_t next = (head + 1U) % UART_RX_BUFFER_SIZE;

            if (next != uart_rx_tail[idx]) {
                uart_rx_buf[idx][head] = b;
                arch_dmb(); /* Ensure the data byte is written to memory before we publish the head pointer. */
                uart_rx_head[idx] = next;
                arch_dmb();

                /* Notify registered task if any */
                if (uart_rx_notify_tasks[idx] != 0) {
                    task_notify(uart_rx_notify_tasks[idx], 1);
                }
            } else {
                uart_rx_overflow[idx]++;   // record overflow
            }
        }
    }

    /* Check if the Transmit Data Register is Empty */
    if ((UARTx->ISR & USART_ISR_TXE) && (UARTx->CR1 & USART_CR1_TXEIE)) {
        /* Check if a TX Queue is registered */
        if (uart_tx_queues[idx] != NULL) {
            uint8_t b;
            if (queue_receive_from_isr(uart_tx_queues[idx], &b) == 0) {
                UARTx->TDR = b;
            } else {
                /* Queue is empty, disable TX interrupt */
                UARTx->CR1 &= ~USART_CR1_TXEIE;
            }
        } else {
            /* Use internal Ring Buffer */
            uint32_t head = uart_tx_head[idx];
            uint32_t tail = uart_tx_tail[idx];
            
            /* If the buffer is not empty transmit*/
            if (head != tail) {
                UARTx->TDR = uart_tx_buf[idx][tail];
                
                uart_tx_tail[idx] = (tail + 1U) % UART_TX_BUFFER_SIZE;
            } else {
                /* Buffer is empty, stop the interrupt source */
                UARTx->CR1 &= ~USART_CR1_TXEIE; 
            }
        }
    }
}


/*
 * Enqueue up to `len` bytes for interrupt-driven TX (TX ring buffer).
 * Requires NVIC IRQ enabled for that USARTx (e.g. NVIC_EnableIRQ(USART2_IRQn)).
 * The driver will set TXEIE to kick transmission when data is queued.
 * The application must call uart_enable_tx_interrupt() once to initialize this path.
 */
int uart_write_buffer(uart_port_t port, const char *src, size_t len) {
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    if (UARTx == NULL) {
        return 0;
    }

    int idx = uart_get_index(UARTx);
    if (idx < 0) {
        return 0;
    }
    
    size_t sent = 0;
    uint32_t stat;

    /* Ensure critical section because interrupts modify */
    stat = arch_irq_lock();

    /* Loop and Enqueue Data */
    while (sent < len) {
        uint32_t head = uart_tx_head[idx];
        uint32_t tail = uart_tx_tail[idx];
        
        /* Calculate the next head pointer */
        uint32_t next_head = (head + 1U) % UART_TX_BUFFER_SIZE;

        if (next_head == tail) {
            break;
        }
        
        // Enqueue byte
        uart_tx_buf[idx][head] = src[sent];
        arch_dmb(); /* Ensure data is written before updating head pointer */
        uart_tx_head[idx] = next_head;
        arch_dmb(); /* Ensure head pointer is visible before ISR sees new data */

        sent++;
    }

    /* After adding to the buffer, re-enable the TX interrupt */
    UARTx->CR1 |= USART_CR1_TXEIE;
    /* Exit Critical Section */
    arch_irq_unlock(stat);

    return (int)sent;
}


/* Enable TX interrupts */
void uart_enable_tx_interrupt(uart_port_t port, uint8_t enable)
{
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    if (UARTx == NULL) {
        return;
    }

    if (enable) {
        UARTx->CR1 |= USART_CR1_TXEIE;
    } else {
        UARTx->CR1 &= ~USART_CR1_TXEIE;
    }
    
    arch_dsb();
}

void uart_set_rx_notify_task(uart_port_t port, uint16_t task_id) {
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    int idx = uart_get_index(UARTx);
    if (idx >= 0) {
        uart_rx_notify_tasks[idx] = task_id;
    }
}

/* Callback wrapper to enable TX interrupt when queue gets data */
static void uart_tx_queue_callback(void *arg) {
    uart_port_t port = (uart_port_t)arg;
    uart_enable_tx_interrupt(port, 1);
}

void uart_set_rx_queue(uart_port_t port, queue_t *q) {
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    int idx = uart_get_index(UARTx);
    if (idx >= 0) {
        uart_rx_queues[idx] = q;
    }
}

void uart_set_tx_queue(uart_port_t port, queue_t *q) {
    USART_TypeDef *UARTx = (USART_TypeDef *)port;
    int idx = uart_get_index(UARTx);
    if (idx >= 0) {
        uart_tx_queues[idx] = q;
        if (q) {
            queue_set_push_callback(q, uart_tx_queue_callback, port);
        }
    }
}


/* Weak ISR wrappers override startup aliases and forward to generic handler */
void USART1_IRQHandler(void)
{
    uart_irq_handler(USART1);
}

void USART2_IRQHandler(void)
{
    uart_irq_handler(USART2);
}

void USART3_IRQHandler(void)
{
    uart_irq_handler(USART3);
}

void UART4_IRQHandler(void)
{
    uart_irq_handler(UART4);
}

void UART5_IRQHandler(void)
{
    uart_irq_handler(UART5);
}

void LPUART1_IRQHandler(void)
{
    uart_irq_handler(LPUART1);
}
