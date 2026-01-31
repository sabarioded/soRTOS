#include "i2c.h"
#include "allocator.h"
#include "i2c_hal.h"
#include "utils.h"

typedef enum { I2C_STATE_IDLE, I2C_STATE_TX, I2C_STATE_RX } i2c_state_t;

struct i2c_context {
  void *hal_handle;

  /* Async State */
  const uint8_t *tx_buf;
  uint8_t *rx_buf;
  volatile size_t transfer_len;
  volatile size_t transfer_idx;
  volatile i2c_state_t state;
  uint16_t addr;

  i2c_callback_t callback;
  void *cb_arg;
};

size_t i2c_get_context_size(void) { return sizeof(struct i2c_context); }

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
  if (!port || !port->hal_handle)
    return -1;
  if (port->state != I2C_STATE_IDLE)
    return -1;

  return i2c_hal_master_transmit(port->hal_handle, addr, data, len);
}

int i2c_master_receive(i2c_port_t port, uint16_t addr, uint8_t *data,
                       size_t len) {
  if (!port || !port->hal_handle)
    return -1;
  if (port->state != I2C_STATE_IDLE)
    return -1;

  return i2c_hal_master_receive(port->hal_handle, addr, data, len);
}

int i2c_master_transmit_async(i2c_port_t port, uint16_t addr,
                              const uint8_t *data, size_t len,
                              i2c_callback_t callback, void *arg) {
  if (!port || !port->hal_handle || len > 255)
    return -1;
  if (port->state != I2C_STATE_IDLE)
    return -1;

  port->addr = addr;
  port->tx_buf = data;
  port->rx_buf = NULL;
  port->transfer_len = len;
  port->transfer_idx = 0;
  port->callback = callback;
  port->cb_arg = arg;
  port->state = I2C_STATE_TX;

  I2C_TypeDef *I2Cx = (I2C_TypeDef *)port->hal_handle;

  /* Configure CR2: SADD, NBYTES, AUTOEND, Write(RD_WRN=0), START */
  uint32_t cr2 = (addr << 1);
  cr2 |= (len << I2C_CR2_NBYTES_Pos);
  cr2 |= I2C_CR2_AUTOEND;
  cr2 |= I2C_CR2_START;

  I2Cx->CR2 = cr2;

  /* Enable Interrupts */
  i2c_hal_enable_ev_irq(port->hal_handle, 1);
  i2c_hal_enable_er_irq(port->hal_handle, 1);

  return 0;
}

int i2c_master_receive_async(i2c_port_t port, uint16_t addr, uint8_t *data,
                             size_t len, i2c_callback_t callback, void *arg) {
  if (!port || !port->hal_handle || len > 255)
    return -1;
  if (port->state != I2C_STATE_IDLE)
    return -1;

  port->addr = addr;
  port->tx_buf = NULL;
  port->rx_buf = data;
  port->transfer_len = len;
  port->transfer_idx = 0;
  port->callback = callback;
  port->cb_arg = arg;
  port->state = I2C_STATE_RX;

  I2C_TypeDef *I2Cx = (I2C_TypeDef *)port->hal_handle;

  /* Configure CR2: SADD, NBYTES, AUTOEND, Read(RD_WRN=1), START */
  uint32_t cr2 = (addr << 1);
  cr2 |= (len << I2C_CR2_NBYTES_Pos);
  cr2 |= I2C_CR2_AUTOEND;
  cr2 |= I2C_CR2_RD_WRN;
  cr2 |= I2C_CR2_START;

  I2Cx->CR2 = cr2;

  /* Enable Interrupts */
  i2c_hal_enable_ev_irq(port->hal_handle, 1);
  i2c_hal_enable_er_irq(port->hal_handle, 1);

  return 0;
}

static void i2c_complete(i2c_port_t port) {
  i2c_hal_enable_ev_irq(port->hal_handle, 0);
  i2c_hal_enable_er_irq(port->hal_handle, 0);
  port->state = I2C_STATE_IDLE;

  /* Clear CR2 (Good practice) */
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)port->hal_handle;
  I2Cx->CR2 = 0;

  if (port->callback) {
    port->callback(port->cb_arg);
  }
}

void i2c_core_ev_irq_handler(i2c_port_t port) {
  if (!port || port->state == I2C_STATE_IDLE)
    return;

  I2C_TypeDef *I2Cx = (I2C_TypeDef *)port->hal_handle;
  uint32_t isr = I2Cx->ISR;

  /* Handle TX */
  if (port->state == I2C_STATE_TX) {
    if (isr & I2C_ISR_TXE) {
      if (port->transfer_idx < port->transfer_len) {
        I2Cx->TXDR = port->tx_buf[port->transfer_idx++];
      }
    }
  }
  /* Handle RX */
  else if (port->state == I2C_STATE_RX) {
    if (isr & I2C_ISR_RXNE) {
      if (port->transfer_idx < port->transfer_len) {
        port->rx_buf[port->transfer_idx++] = (uint8_t)I2Cx->RXDR;
      } else {
        /* Flush extra bytes? */
        (void)I2Cx->RXDR;
      }
    }
  }

  /* Handle STOPF (Stop Flag) - Indicates Transfer Complete due to AutoEnd */
  if (isr & I2C_ISR_STOPF) {
    /* Clear STOPF */
    I2Cx->ICR |= I2C_ICR_STOPCF;
    i2c_complete(port);
  }
}

void i2c_core_er_irq_handler(i2c_port_t port) {
  if (!port)
    return;

  I2C_TypeDef *I2Cx = (I2C_TypeDef *)port->hal_handle;

  /* Clear Errors for now */
  /* Check NACK */
  if (I2Cx->ISR & I2C_ISR_NACKF) {
    I2Cx->ICR |= I2C_ICR_NACKCF;
  }

  /* Abort / Complete with error state?
     For now, we just reset state and callback.
     Ideally, we pass an error code to callback (todo). */
  i2c_complete(port);
}