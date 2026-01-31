#include "uart.h"
#include <stddef.h>
#include "allocator.h"
#include "utils.h"
#include "uart_hal.h"
#include "queue.h"
#include "arch_ops.h"
#include "scheduler.h"
#include "spinlock.h"

struct uart_context {
    void            *hal_handle;        /* Hardware handle (passed to HAL) */
    uint8_t         *rx_buf;            /* Pointer to the receive buffer */
    uint8_t         *tx_buf;            /* Pointer to the transmit buffer */
    queue_t         *rx_queue;          /* Queue for received bytes (optional) */
    queue_t         *tx_queue;          /* Queue for bytes to transmit (optional) */
    spinlock_t      lock;               /* Spinlock for protecting the context */
    uint16_t        rx_buf_size;        /* Size of the receive buffer */
    uint16_t        tx_buf_size;        /* Size of the transmit buffer */
    volatile uint16_t rx_head;          /* Index of the next byte to write in RX buffer */
    volatile uint16_t rx_tail;          /* Index of the next byte to read from RX buffer */
    volatile uint16_t tx_head;          /* Index of the next byte to write in TX buffer */
    volatile uint16_t tx_tail;          /* Index of the next byte to read from TX buffer */
    volatile uint16_t rx_overflow;      /* Count of RX buffer overflows */
    volatile uint16_t rx_errors;        /* Count of RX hardware errors */
    uint16_t        rx_notify_task_id;  /* Task ID to notify on RX */
};

/* Get the size of the UART context structure */
size_t uart_get_context_size(void) {
    return sizeof(struct uart_context);
}

/* Initialize a UART context in a user-provided memory block */
uart_port_t uart_init(void *memory_block, 
                      void *hal_handle, 
                      uint8_t *rx_buf, 
                      size_t rx_size, 
                      uint8_t *tx_buf, 
                      size_t tx_size, 
                      void *config, 
                      uint32_t clock_freq) {
    /* Ensure buffer sizes fit in uint16_t */
    if (!memory_block || rx_size > 0xFFFF || tx_size > 0xFFFF) {
        return NULL;
    }
    
    uart_port_t port = (uart_port_t)memory_block;

    /* Zero out the context memory */
    utils_memset(port, 0, sizeof(struct uart_context));

    /* Initialize Context */
    port->hal_handle = hal_handle;
    port->rx_buf = rx_buf;
    port->tx_buf = tx_buf;
    spinlock_init(&port->lock);

    port->rx_buf_size = (uint16_t)rx_size;
    port->tx_buf_size = (uint16_t)tx_size;

    uart_hal_init(port->hal_handle, config, clock_freq);
    return port;
}

/* Create and initialize a UART port */
uart_port_t uart_create(void *hal_handle, uint8_t *rx_buf, size_t rx_size, uint8_t *tx_buf, size_t tx_size, void *config, uint32_t clock_freq) {
    void *mem = allocator_malloc(sizeof(struct uart_context));
    if (!mem) {
        return NULL;
    }
    return uart_init(mem, hal_handle, rx_buf, rx_size, tx_buf, tx_size, config, clock_freq);
}

/* Free the memory allocated for the UART context */
void uart_destroy(uart_port_t port) {
    if (port) {
        allocator_free(port);
    }
}

/* Get the HAL handle associated with the UART port */
void *uart_get_hal_handle(uart_port_t port) {
    return port ? port->hal_handle : NULL;
}

/* Check if there is any data sitting in the receive buffer */
int uart_available(uart_port_t port) {
    if (!port) {
        return 0;
    }

    uint32_t stat = spin_lock(&port->lock);
    uint16_t head = port->rx_head;
    uint16_t tail = port->rx_tail;
    uint16_t size = port->rx_buf_size;
    spin_unlock(&port->lock, stat);

    if (size == 0) {
        return 0;
    }
    
    if (head >= tail) {
        return (int)(head - tail);
    }
    return (int)(size + head - tail);
}

/* Read data from the internal receive buffer */
int uart_read_buffer(uart_port_t port, char *dst, size_t len) {
    if ((!port) || (dst == NULL) || (len == 0U) || (port->rx_buf == NULL)) {
        return 0;
    }

    size_t copied = 0;
    uint32_t stat = spin_lock(&port->lock);
    uint16_t head = port->rx_head;
    uint16_t tail = port->rx_tail;
    uint16_t size = port->rx_buf_size;

    while ((copied < len) && (head != tail)) {
        dst[copied++] = (char)port->rx_buf[tail];
        
        tail++;
        if (tail >= size) {
            tail = 0;
        }
    }
    
    /* Update tail once after copy is done */
    port->rx_tail = tail;
    spin_unlock(&port->lock, stat);

    return (int)copied;
}

/* Queue data to be sent out */
int uart_write_buffer(uart_port_t port, const char *src, size_t len) {
    if ((!port) || (port->tx_buf == NULL) || (len == 0U)) {
        return 0;
    }
    
    size_t sent = 0;
    uint32_t stat = spin_lock(&port->lock);
    
    uint16_t head = port->tx_head;
    uint16_t tail = port->tx_tail;

    while (sent < len) {
        uint16_t next_head = head + 1;
        if (next_head >= port->tx_buf_size) {
            next_head = 0;
        }

        if (next_head == tail) {
            break;
        }
        
        port->tx_buf[head] = src[sent];
        head = next_head;
        sent++;
    }

    /* Update head once. ISR cannot run because IRQ is locked, so this is safe. */
    port->tx_head = head;

    uart_hal_enable_tx_interrupt(port->hal_handle, 1);
    spin_unlock(&port->lock, stat);
    return (int)sent;
}

/* Enable or disable the Receive Interrupt */
void uart_enable_rx_interrupt(uart_port_t port, uint8_t enable) {
    uart_hal_enable_rx_interrupt(port ? port->hal_handle : NULL, enable);
}

/* Enable or disable the Transmit Interrupt */
void uart_enable_tx_interrupt(uart_port_t port, uint8_t enable) {
    uart_hal_enable_tx_interrupt(port ? port->hal_handle : NULL, enable);
}

/* Register a task to be notified when a byte is received */
void uart_set_rx_notify_task(uart_port_t port, uint16_t task_id) {
    if (port) {
        port->rx_notify_task_id = task_id;
    }
}

/* Internal callback for TX queue push events */
static void uart_tx_queue_callback(void *arg) {
    uart_port_t port = (uart_port_t)arg;
    uart_hal_enable_tx_interrupt(port->hal_handle, 1);
}

/* Register a queue to receive incoming bytes */
void uart_set_rx_queue(uart_port_t port, queue_t *q) {
    if (port) port->rx_queue = q;
}

/* Register a queue to source outgoing bytes */
void uart_set_tx_queue(uart_port_t port, queue_t *q) {
    if (port) {
        port->tx_queue = q;
        if (q) {
            queue_set_push_callback(q, uart_tx_queue_callback, port);
        }
    }
}

/* Called by the HAL when a byte is received */
void uart_core_rx_callback(uart_port_t port, uint8_t byte) {
    if (!port) {
        return;
    }

    if (port->rx_queue != NULL) {
        if (queue_push_from_isr(port->rx_queue, &byte) < 0) {
            port->rx_overflow++;
        }
    } else {
        if (port->rx_buf == NULL) {
            return;
        }

        uint32_t stat = spin_lock(&port->lock);
        int should_notify = 0;
        uint16_t notify_id = 0;
        uint16_t head = port->rx_head;
        uint16_t next = head + 1;
        if (next >= port->rx_buf_size) {
            next = 0;
        }

        if (next != port->rx_tail) {
            port->rx_buf[head] = byte;
            /* Barrier to ensure data is written before head is updated */
            arch_dmb(); 
            port->rx_head = next;
            if (port->rx_notify_task_id != 0) {
                should_notify = 1;
                notify_id = port->rx_notify_task_id;
            }
        } else {
            port->rx_overflow++;
        }
        spin_unlock(&port->lock, stat);

        if (should_notify) {
            task_notify(notify_id, 1);
        }
    }
}

/* Called by the HAL when the transmitter is ready for more data */
int uart_core_tx_callback(uart_port_t port, uint8_t *byte) {
    if (!port) {
        return 0;
    }

    if (port->tx_queue != NULL) {
        uint8_t b;
        if (queue_pop_from_isr(port->tx_queue, &b) == 0) {
            *byte = b;
            return 1;
        }
    } else {
        if (port->tx_buf == NULL) {
            return 0;
        }

        uint32_t stat = spin_lock(&port->lock);
        uint16_t head = port->tx_head;
        uint16_t tail = port->tx_tail;
        
        if (head != tail) {
            *byte = port->tx_buf[tail];
            uint16_t next_tail = tail + 1;
            if (next_tail >= port->tx_buf_size) next_tail = 0;
            port->tx_tail = next_tail;
            spin_unlock(&port->lock, stat);
            return 1;
        }
        spin_unlock(&port->lock, stat);
    }
    return 0; /* Queue empty */
}

/* Called by the HAL when a receive error occurs */
void uart_core_rx_error_callback(uart_port_t port) {
    if (port) {
        port->rx_errors++;
    }
}
