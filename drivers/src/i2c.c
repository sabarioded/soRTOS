#include "i2c.h"
#include "allocator.h"
#include "i2c_hal.h"
#include "utils.h"

typedef enum {
    I2C_STATE_IDLE,
    I2C_STATE_TX,
    I2C_STATE_RX
} i2c_state_t;

struct i2c_context {
    void *hal_handle;
    const uint8_t *tx_buf;
    uint8_t *rx_buf;
    i2c_callback_t callback;
    void *cb_arg;
    volatile size_t transfer_len;
    volatile size_t transfer_idx;
    volatile i2c_state_t state;
    uint16_t addr;
};

size_t i2c_get_context_size(void) {
    return sizeof(struct i2c_context);
}

i2c_port_t i2c_init(void *memory_block, void *hal_handle, void *config) {
    if (!memory_block || !hal_handle) {
        return NULL;
    }

    i2c_port_t port = (i2c_port_t)memory_block;
    utils_memset(port, 0, sizeof(struct i2c_context));

    port->hal_handle = hal_handle;
    port->state = I2C_STATE_IDLE;

    /* Initialize hardware via HAL */
    i2c_hal_init(port->hal_handle, config);

    return port;
}

i2c_port_t i2c_create(void *hal_handle, void *config) {
    void *mem = allocator_malloc(sizeof(struct i2c_context));
    if (!mem) {
        return NULL;
    }
    return i2c_init(mem, hal_handle, config);
}

void i2c_destroy(i2c_port_t port) {
    if (port) {
        allocator_free(port);
    }
}

int i2c_master_transmit(i2c_port_t port, uint16_t addr, const uint8_t *data,
                        size_t len) {
    if (!port || !port->hal_handle) {
        return -1;
    }
    if (!data && len != 0U) {
        return -1;
    }
    if (port->state != I2C_STATE_IDLE) {
        return -1;
    }

    return i2c_hal_master_transmit(port->hal_handle, addr, data, len);
}

int i2c_master_receive(i2c_port_t port, uint16_t addr, uint8_t *data,
                       size_t len) {
    if (!port || !port->hal_handle) {
        return -1;
    }
    if (!data && len != 0U) {
        return -1;
    }
    if (port->state != I2C_STATE_IDLE) {
        return -1;
    }

    return i2c_hal_master_receive(port->hal_handle, addr, data, len);
}

int i2c_master_transmit_async(i2c_port_t port, uint16_t addr,
                              const uint8_t *data, size_t len,
                              i2c_callback_t callback, void *arg) {
    if (!port || !port->hal_handle) {
        return -1;
    }
    if (!data || len == 0U) {
        return -1;
    }
    if (port->state != I2C_STATE_IDLE) {
        return -1;
    }

    port->addr = addr;
    port->tx_buf = data;
    port->rx_buf = NULL;
    port->transfer_len = len;
    port->transfer_idx = 0;
    port->callback = callback;
    port->cb_arg = arg;
    port->state = I2C_STATE_TX;

    if (i2c_hal_start_master_transfer(port->hal_handle, addr, len, 0) != 0) {
        port->state = I2C_STATE_IDLE;
        return -1;
    }

    /* Enable Interrupts */
    i2c_hal_enable_ev_irq(port->hal_handle, 1);
    i2c_hal_enable_er_irq(port->hal_handle, 1);

    return 0;
}

int i2c_master_receive_async(i2c_port_t port, uint16_t addr, uint8_t *data,
                             size_t len, i2c_callback_t callback, void *arg) {
    if (!port || !port->hal_handle) {
        return -1;
    }
    if (!data || len == 0U) {
        return -1;
    }
    if (port->state != I2C_STATE_IDLE) {
        return -1;
    }

    port->addr = addr;
    port->tx_buf = NULL;
    port->rx_buf = data;
    port->transfer_len = len;
    port->transfer_idx = 0;
    port->callback = callback;
    port->cb_arg = arg;
    port->state = I2C_STATE_RX;

    if (i2c_hal_start_master_transfer(port->hal_handle, addr, len, 1) != 0) {
        port->state = I2C_STATE_IDLE;
        return -1;
    }

    /* Enable Interrupts */
    i2c_hal_enable_ev_irq(port->hal_handle, 1);
    i2c_hal_enable_er_irq(port->hal_handle, 1);

    return 0;
}

static void i2c_complete(i2c_port_t port, i2c_status_t status) {
    i2c_hal_enable_ev_irq(port->hal_handle, 0);
    i2c_hal_enable_er_irq(port->hal_handle, 0);
    port->state = I2C_STATE_IDLE;

    /* Clear transfer configuration in the peripheral */
    i2c_hal_clear_config(port->hal_handle);

    if (port->callback) {
        port->callback(port->cb_arg, status);
    }
}

void i2c_core_ev_irq_handler(i2c_port_t port) {
    if (!port || port->state == I2C_STATE_IDLE) {
        return;
    }

    /* Handle NACK early to abort the transfer */
    if (i2c_hal_nack_detected(port->hal_handle)) {
        i2c_hal_clear_nack(port->hal_handle);
        i2c_complete(port, I2C_STATUS_NACK);
        return;
    }

    /* Handle TX */
    if (port->state == I2C_STATE_TX) {
        if (i2c_hal_tx_ready(port->hal_handle)) {
            if (port->transfer_idx < port->transfer_len) {
                i2c_hal_write_tx_byte(port->hal_handle, port->tx_buf[port->transfer_idx++]);
            }
        }
    }
    /* Handle RX */
    else if (port->state == I2C_STATE_RX) {
        if (i2c_hal_rx_ready(port->hal_handle)) {
            if (port->transfer_idx < port->transfer_len) {
                port->rx_buf[port->transfer_idx++] = i2c_hal_read_rx_byte(port->hal_handle);
            } else {
                /* Flush extra bytes? */
                (void)i2c_hal_read_rx_byte(port->hal_handle);
            }
        }
    }

    /* Handle STOPF (Stop Flag) - Indicates Transfer Complete due to AutoEnd */
    if (i2c_hal_stop_detected(port->hal_handle)) {
        /* Clear STOPF */
        i2c_hal_clear_stop(port->hal_handle);
        i2c_complete(port, I2C_STATUS_OK);
    }
}

void i2c_core_er_irq_handler(i2c_port_t port) {
    if (!port || port->state == I2C_STATE_IDLE) {
        return;
    }

    /* Clear Errors for now */
    /* Check NACK */
    if (i2c_hal_nack_detected(port->hal_handle)) {
        i2c_hal_clear_nack(port->hal_handle);
        i2c_complete(port, I2C_STATUS_NACK);
        return;
    }

    /* Complete with a generic error status. */
    i2c_complete(port, I2C_STATUS_ERR);
}
