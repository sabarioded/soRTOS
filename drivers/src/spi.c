#include "spi.h"
#include "allocator.h"
#include "spi_hal.h"
#include "utils.h"

struct spi_context {
    void *hal_handle;          /* Hardware handle (passed to HAL) */
    void *cb_arg;              /* User argument for async callback */
    const uint8_t *tx_buf;     /* Pointer to transmit buffer */
    uint8_t *rx_buf;           /* Pointer to receive buffer */
    spi_callback_t callback;   /* Async completion callback */
    volatile size_t transfer_len; /* Total bytes to transfer */
    volatile size_t tx_count;  /* Number of bytes transmitted */
    volatile size_t rx_count;  /* Number of bytes received */
    volatile uint8_t busy;     /* Non-zero while an async transfer is active */
};

/* Get the size of the SPI context structure */
size_t spi_get_context_size(void) {
    return sizeof(struct spi_context);
}

/* Initialize a SPI context in caller-provided memory */
spi_port_t spi_init(void *memory_block, void *hal_handle, void *config) {
    if (!memory_block || !hal_handle) {
        return NULL;
    }

    spi_port_t port = (spi_port_t)memory_block;
    utils_memset(port, 0, sizeof(struct spi_context));

    port->hal_handle = hal_handle;
    port->busy = 0;

    /* Initialize hardware via HAL */
    spi_hal_init(port->hal_handle, config);

    return port;
}

/* Create and initialize a SPI context (allocates memory) */
spi_port_t spi_create(void *hal_handle, void *config) {
    void *mem = allocator_malloc(sizeof(struct spi_context));
    if (!mem) {
        return NULL;
    }
    return spi_init(mem, hal_handle, config);
}

/* Destroy a SPI context created with spi_create */
void spi_destroy(spi_port_t port) {
    if (port) {
        allocator_free(port);
    }
}

/* Blocking, polled SPI transfer */
int spi_transfer(spi_port_t port, 
                const uint8_t *tx_data, 
                uint8_t *rx_data,
                size_t len) {
    if (!port || !port->hal_handle) {
        return -1;
    }

    if (port->busy) {
        return -1;
    }

    for (size_t i = 0; i < len; i++) {
        uint8_t tx_byte = (tx_data != NULL) ? tx_data[i] : 0xFF;
        uint8_t rx_byte = spi_hal_transfer_byte(port->hal_handle, tx_byte);

        if (rx_data != NULL) {
            rx_data[i] = rx_byte;
        }
    }
    return 0;
}

/* Interrupt-driven transfer. completion via callback */
int spi_transfer_async(spi_port_t port, 
                    const uint8_t *tx_data,
                    uint8_t *rx_data, 
                    size_t len, 
                    spi_callback_t callback,
                    void *arg) {
    if (!port || !port->hal_handle || len == 0) {
        return -1;
    }

    if (port->busy) {
        return -1;
    }

    /* Initialize Transfer State */
    port->tx_buf = tx_data;
    port->rx_buf = rx_data;
    port->transfer_len = len;
    port->tx_count = 0;
    port->rx_count = 0;
    port->callback = callback;
    port->cb_arg = arg;

    port->busy = 1;

    /* Enable Interrupts to start transfer.
       Enabling TXEIE will trigger an immediate interrupt because the TX buffer is empty. */
    spi_hal_enable_rx_irq(port->hal_handle, 1);
    spi_hal_enable_tx_irq(port->hal_handle, 1);

    return 0;
}

/*
 * This function should be called by the HAL ISR.
 * It handles the data pumping.
 */
void spi_core_irq_handler(spi_port_t port) {
    if (!port) {
        return;
    }

    /* Handle RX (Data Received) */
    if (spi_hal_rx_ready(port->hal_handle)) {
        /* Read data */
        uint8_t rx_byte = spi_hal_read_byte(port->hal_handle);

        if (port->rx_buf && (port->rx_count < port->transfer_len)) {
            port->rx_buf[port->rx_count] = rx_byte;
        }
        port->rx_count++;

        /* Check for completion */
        if (port->rx_count >= port->transfer_len) {
            /* Disable all interrupts */
            spi_hal_enable_tx_irq(port->hal_handle, 0);
            spi_hal_enable_rx_irq(port->hal_handle, 0);

            port->busy = 0;

            /* Signal completion to the caller */
            if (port->callback) {
                port->callback(port->cb_arg);
            }
            return;
        }
    }

    /* Handle TX (Transmit Buffer Empty) */
    if (spi_hal_tx_ready(port->hal_handle)) {
        if (port->tx_count < port->transfer_len) {
            /* Send next byte */
            uint8_t tx_byte = (port->tx_buf != NULL) ? port->tx_buf[port->tx_count] : 0xFF;
            spi_hal_write_byte(port->hal_handle, tx_byte);
            port->tx_count++;
        } else {
            /* All data sent, disable TX interrupt to stop firing */
            spi_hal_enable_tx_irq(port->hal_handle, 0);
        }
    }
}
