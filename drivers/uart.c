#include <stddef.h>      /* for NULL */
#include "uart.h"
#include "utils.h"
#include "project_config.h"

/* RCC_AHB2ENR  – AHB2 peripheral clock enable register
 * Bits 3:0 control the clock to GPIOA..GPIOD.
 * We use these to turn on the GPIO ports that carry the UART pins.
 */
#define RCC_AHB2ENR_GPIOAEN_POS (0)                      // Bit 0: GPIOA clock enable
#define RCC_AHB2ENR_GPIOAEN     (1UL << RCC_AHB2ENR_GPIOAEN_POS)

#define RCC_AHB2ENR_GPIOBEN_POS (1)                      // Bit 1: GPIOB clock enable
#define RCC_AHB2ENR_GPIOBEN     (1UL << RCC_AHB2ENR_GPIOBEN_POS)

#define RCC_AHB2ENR_GPIOCEN_POS (2)                      // Bit 2: GPIOC clock enable
#define RCC_AHB2ENR_GPIOCEN     (1UL << RCC_AHB2ENR_GPIOCEN_POS)

#define RCC_AHB2ENR_GPIODEN_POS (3)                      // Bit 3: GPIOD clock enable
#define RCC_AHB2ENR_GPIODEN     (1UL << RCC_AHB2ENR_GPIODEN_POS)

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

/* GPIO helper macro: set pin to AF mode and select AF number */
#define GPIO_SET_AF(GPIOx, pin, af) do {                          \
    /* Clear mode bits for this pin */                            \
    (GPIOx)->MODER &= ~(3UL << ((pin) * 2U));                     \
    /* Set to AF mode (0b10) */                                   \
    (GPIOx)->MODER |=  (2UL << ((pin) * 2U));                     \
                                                                  \
    if ((pin) < 8U) {                                             \
        /* AFR[0] is used for pins 0..7 */                        \
        /* Use (pin & 7) to ensure shift is always safe (0..28) */ \
        (GPIOx)->AFR[0] &= ~(0xFUL << (((pin) & 7U) * 4U));       \
        (GPIOx)->AFR[0] |=  ((af)  << (((pin) & 7U) * 4U));       \
    } else {                                                      \
        /* AFR[1] is used for pins 8..15 */                       \
        (GPIOx)->AFR[1] &= ~(0xFUL << (((pin) & 7U) * 4U));       \
        (GPIOx)->AFR[1] |=  ((af)  << (((pin) & 7U) * 4U));       \
    }                                                             \
} while (0)

#define UART_MAX_INSTANCES   6

/* We track these instances for buffered RX + callbacks. */
static USART_t *uart_instances[UART_MAX_INSTANCES];

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

static void (*uart_rx_callback[UART_MAX_INSTANCES])(char) = { 0 };


/* Find array index for a given instance */
static int uart_get_index(USART_t *UARTx)
{
    for (int i = 0; i < UART_MAX_INSTANCES; ++i) {
        if (uart_instances[i] == UARTx) {
            return i;
        }
    }
    return -1;
}


/* Register a UART instance and return its index */
static int uart_register_instance(USART_t *UARTx)
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
            return i;
        }
    }
    return -1; // no free slots
}


/* Enable peripheral clocks and configure GPIO pins for a given UARTx. */
static void uart_enable_clocks_and_pins(USART_t *UARTx)
{
    if (UARTx == USART1) {
        /* USART1: PA9 (TX), PA10 (RX), AF7 */
        /* Enable GPIOA clock (AHB2) */
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
        /* Enable USART1 clock (APB2) */
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

        GPIO_SET_AF(GPIOA, 9U, 7U);   /* PA9  -> USART1_TX (AF7) */
        GPIO_SET_AF(GPIOA, 10U, 7U);  /* PA10 -> USART1_RX (AF7) */
    }
    else if (UARTx == USART2) {
        /* USART2: PA2 (TX), PA3 (RX), AF7  — Nucleo VCP */
        /* Enable GPIOA clock */
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
        /* Enable USART2 clock (APB1) */
        RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;

        GPIO_SET_AF(GPIOA, 2U, 7U);   /* PA2 -> USART2_TX (AF7) */
        GPIO_SET_AF(GPIOA, 3U, 7U);   /* PA3 -> USART2_RX (AF7) */
    }
    else if (UARTx == USART3) {
        /* USART3: PB10 (TX), PB11 (RX), AF7 */
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
        RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN;

        GPIO_SET_AF(GPIOB, 10U, 7U);  /* PB10 -> USART3_TX (AF7) */
        GPIO_SET_AF(GPIOB, 11U, 7U);  /* PB11 -> USART3_RX (AF7) */
    }
    else if (UARTx == UART4) {
        /* UART4: PC10 (TX), PC11 (RX), AF8 */
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
        RCC->APB1ENR1 |= RCC_APB1ENR1_UART4EN;

        GPIO_SET_AF(GPIOC, 10U, 8U);  /* PC10 -> UART4_TX (AF8) */
        GPIO_SET_AF(GPIOC, 11U, 8U);  /* PC11 -> UART4_RX (AF8) */
    }
    else if (UARTx == UART5) {
        /* UART5: PC12 (TX), PD2 (RX), AF8 */
        RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOCEN | RCC_AHB2ENR_GPIODEN);
        RCC->APB1ENR1 |= RCC_APB1ENR1_UART5EN;

        GPIO_SET_AF(GPIOC, 12U, 8U);  /* PC12 -> UART5_TX (AF8) */
        GPIO_SET_AF(GPIOD, 2U, 8U);   /* PD2  -> UART5_RX (AF8) */
    }
    else if (UARTx == LPUART1) {
        /* LPUART1: PC0 (TX), PC1 (RX), AF8 */
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
        RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;

        GPIO_SET_AF(GPIOC, 0U, 8U);   /* PC0 -> LPUART1_TX (AF8) */
        GPIO_SET_AF(GPIOC, 1U, 8U);   /* PC1 -> LPUART1_RX (AF8) */
    }
}


/* Initialize UART with full config */
int uart_init(USART_t *UARTx, const UART_Config_t *config, uint32_t periph_clock_hz)
{
    if ((UARTx == NULL) || (config == NULL) || (config->BaudRate == 0U)) {
        return UART_FAIL;
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
        case UART_WORDLENGTH_7B:
            cr1 |= USART_CR1_M1; // M1=1, M0=0
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
    if (config->OverSampling8) { // by 8
        cr1 |= USART_CR1_OVER8;
    } else { // by 16
        cr1 &= ~USART_CR1_OVER8;
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
        return UART_FAIL;
    }

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
        usartdiv = (periph_clock_hz + (baud / 2U)) / baud;
        UARTx->BRR = usartdiv;
    } else {
        /* Oversampling by 8 */
        usartdiv = ((periph_clock_hz * 2U) + (baud / 2U)) / baud;

        /* Map USARTDIV into BRR as described in the reference manual */
        UARTx->BRR =
            (usartdiv & 0xFFF0U) |          /* BRR[15:4] = USARTDIV[15:4], BRR[3:0]=0 */
            ((usartdiv & 0x000FU) >> 1U);   /* BRR[2:0] = USARTDIV[3:1], BRR[3] stays 0 */
    }

    /* Enable transmitter and receiver */
    UARTx->CR1 |= (USART_CR1_TE | USART_CR1_RE);

    /* register instance */
    int idx = uart_register_instance(UARTx);
    if (idx < 0) {
        UARTx->CR1 &= ~(USART_CR1_TE | USART_CR1_RE); // disable TX/RX
        return UART_FAIL;
    }

    /* Enable UART */
    UARTx->CR1 |= USART_CR1_UE;

    __DSB(); // barrier for I/O synchronization

    return UART_OK;
}


/* Send a char via uart */
int uart_send_char(USART_t *UARTx, char c)
{
    if (UARTx == NULL) {
        return UART_FAIL;
    }

    /* Wait for TXE (Transmit Data Register Empty) with timeout */
    if (wait_for_flag_set(&UARTx->ISR, USART_ISR_TXE, UART_MAX_ITERATIONS) != 0) {
        return UART_ERR_TIMEOUT;
    }

    /* Writing to TDR starts the transmission */
    UARTx->TDR = (uint8_t)c;

    return UART_OK;
}


/* Send a string via uart. (does NOT send the terminating '\0') */
int uart_send_string(USART_t *UARTx, const char *str)
{
    if ((UARTx == NULL) || (str == NULL)) {
        return UART_FAIL;
    }

    while (*str != '\0') {
        int status = uart_send_char(UARTx, *str++);
        if (status != UART_OK) {
            return status;   // propagate timeout/fail
        }
    }

    return UART_OK;
}


/* Receive a char via uart */
int uart_receive_char(USART_t *UARTx, char *result)
{
    if ((UARTx == NULL) || (result == NULL)) {
        return UART_FAIL;
    }

    /* Wait for RXNE (Read Data Register Not Empty) */
    if (wait_for_flag_set(&UARTx->ISR, USART_ISR_RXNE, UART_MAX_ITERATIONS) != 0) {
        return UART_ERR_TIMEOUT;
    }

    /* Read ISR to check for errors */
    uint32_t isr_flags = UARTx->ISR;

    /* Read data (this clears RXNE flag and helps clear ORE) */
    *result = (char)(UARTx->RDR & 0xFFU);

    /* Clear error flags if present */
    if (isr_flags & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) {
        UARTx->ICR = USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF;
    }

    /* Check error flags now that data has been read */
    if (isr_flags & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
        if (isr_flags & USART_ISR_ORE) {
            return UART_ERR_OVERRUN;
        }
        if (isr_flags & USART_ISR_FE) {
            return UART_ERR_FRAMING;
        }
        if (isr_flags & USART_ISR_NE) {
            return UART_ERR_NOISE;
        }
        return UART_FAIL; // fallback
    }

    return UART_OK;
}


/* Receive a string via uart. */
int uart_receive_string(USART_t *UARTx, char *buffer, uint32_t max_length)
{
    if ((UARTx == NULL) || (buffer == NULL) || (max_length == 0U)) {
        return UART_FAIL;
    }

    uint32_t i = 0;

    /* Leave space for '\0' terminator */
    while (i < (max_length - 1U)) {
        char c;
        int status = uart_receive_char(UARTx, &c);

        if (status != UART_OK) {
            /* terminate buffer with what we have so far */
            buffer[i] = '\0';
            return status;  // timeout or HW error
        }

        /* Simple line termination on '\n' or '\r' */
        if ((c == '\n') || (c == '\r')) {
            break;
        }

        buffer[i++] = c;
    }

    buffer[i] = '\0';
    return UART_OK;
}


/* Register RX callback */
void uart_set_rx_callback(USART_t *UARTx, void (*cb)(char c))
{
    int idx = uart_get_index(UARTx);
    if (idx < 0) {
        return;
    }
    uart_rx_callback[idx] = cb;
}


/* Enable/disable RX interrupt */
void uart_enable_rx_interrupt(USART_t *UARTx, int enable)
{
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
uint32_t uart_available(USART_t *UARTx)
{
    int idx = uart_get_index(UARTx);
    if (idx < 0) {
        return 0;
    }

    uint32_t basepri_state = enter_critical_basepri(MAX_SYSCALL_PRIORITY); 
    uint32_t head = uart_rx_head[idx];
    uint32_t tail = uart_rx_tail[idx];
    exit_critical_basepri(basepri_state);

    return (head + UART_RX_BUFFER_SIZE - tail) % UART_RX_BUFFER_SIZE;
}


/* Read bytes from RX buffer */
uint32_t uart_read_buffer(USART_t *UARTx, char *dst, uint32_t len)
{
    int idx = uart_get_index(UARTx);
    if ((idx < 0) || (dst == NULL) || (len == 0U)) {
        return 0;
    }

    uint32_t copied = 0;

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);
    uint32_t head = uart_rx_head[idx];
    uint32_t tail = uart_rx_tail[idx];

    while ((copied < len) && (head != tail)) {
        dst[copied++] = (char)uart_rx_buf[idx][tail];
        tail = (tail + 1U) % UART_RX_BUFFER_SIZE;
    }
    uart_rx_tail[idx] = tail;
    exit_critical_basepri(stat);

    return copied;
}


/* function for IRQ Handlers */
void uart_irq_handler(USART_t *UARTx)
{
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

        /* Put into circular buffer */
        uint32_t head = uart_rx_head[idx];
        uint32_t next = (head + 1U) % UART_RX_BUFFER_SIZE;

        if (next != uart_rx_tail[idx]) {
            uart_rx_buf[idx][head] = b;
            __DMB(); /* Ensure the data byte is written to memory before we publish the head pointer. */
            uart_rx_head[idx] = next;
            __DMB();
        } else {
            uart_rx_overflow[idx]++;   // record overflow
        }

        /* Optional per-byte callback */
        if (uart_rx_callback[idx]) {
            uart_rx_callback[idx]((char)b);
        }
    }

    /* Check if the Transmit Data Register is Empty */
    if (UARTx->ISR & USART_ISR_TXE) {
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


/* Get the number of overflow bytes */
uint32_t uart_get_overflow_count(USART_t *UARTx)
{
    int idx = uart_get_index(UARTx);
    if (idx < 0) return 0;
    return uart_rx_overflow[idx];
}


/**
 * @brief Get the number of RX errors (parity, framing, noise) that occurred.
 * This counter increments each time an error flag is detected in the ISR.
 * 
 * @param UARTx UART instance
 * @return Number of RX errors detected, or 0 if instance not found
 */
uint32_t uart_get_error_count(USART_t *UARTx)
{
    int idx = uart_get_index(UARTx);
    if (idx < 0) return 0;
    return uart_rx_errors[idx];
}


/*
 * Enqueue up to `len` bytes for interrupt-driven TX (TX ring buffer).
 * Requires NVIC IRQ enabled for that USARTx (e.g. NVIC_EnableIRQ(USART2_IRQn)).
 * The driver will set TXEIE to kick transmission when data is queued.
 * The application must call uart_enable_tx_interrupt() once to initialize this path.
 */
uint32_t uart_write_buffer(USART_t *UARTx, const char *src, uint32_t len) {
    if (UARTx == NULL) {
        return 0;
    }

    int idx = uart_get_index(UARTx);
    if (idx < 0) {
        return 0;
    }
    
    uint32_t sent = 0;
    uint32_t basepri_state;

    /* Ensure critical section because interrupts modify */
    basepri_state = enter_critical_basepri(MAX_SYSCALL_PRIORITY); 

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
        __DMB(); /* Ensure data is written before updating head pointer */
        uart_tx_head[idx] = next_head;
        __DMB(); /* Ensure head pointer is visible before ISR sees new data */

        sent++;
    }

    UARTx->CR1 |= USART_CR1_TXEIE;
    /* Exit Critical Section */
    exit_critical_basepri(basepri_state);

    return sent;
}


/* Enable TX interrupts */
void uart_enable_tx_interrupt(USART_t *UARTx, int enable)
{
    if (UARTx == NULL) {
        return;
    }

    if (enable) {
        UARTx->CR1 |= USART_CR1_TXEIE;
    } else {
        UARTx->CR1 &= ~USART_CR1_TXEIE;
    }
    
    __DSB();
}


/* Return number of bytes pending in TX buffer. */
uint32_t uart_tx_pending(USART_t *UARTx)
{
    int idx = uart_get_index(UARTx);
    if (idx < 0) return 0;

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);
    uint32_t head = uart_tx_head[idx];
    uint32_t tail = uart_tx_tail[idx];
    exit_critical_basepri(stat);

    /* Circular buffer: calculate distance from tail to head */
    return (head + UART_TX_BUFFER_SIZE - tail) % UART_TX_BUFFER_SIZE;
}


/* Waits for TX buffer empty and hardware TC flag set.
 * 
 * IMPORTANT: Requires that USART_CR1_TXEIE (TX interrupt) is enabled.
 * Call uart_enable_tx_interrupt(UARTx, 1) before using this.
 */
int uart_flush(USART_t *UARTx)
{
    int idx = uart_get_index(UARTx);
    if (idx < 0) return UART_FAIL;

    /* Wait for the Software TX Buffer to Empty
     * Yields CPU to other tasks while ISR drains the buffer.
     */
    while (uart_tx_pending(UARTx) > 0) {
        yield_cpu();
    }

    /* Wait for the Hardware Transmission Complete (TC) flag
     * The TX buffer is now empty. We must wait for the last byte to physically
     * leave the shift register.
     */
    int ret = wait_for_flag_set(&UARTx->ISR, USART_ISR_TC, UART_MAX_ITERATIONS);
    
    if (ret != UART_OK) {
        return UART_ERR_TIMEOUT;
    }
    
    /* Clear the TC flag for edge-triggered behavior in future calls */
    UARTx->ICR = USART_ICR_TCCF;

    return UART_OK;
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
