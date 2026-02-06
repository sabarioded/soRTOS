#ifndef I2C_HAL_STM32_H
#define I2C_HAL_STM32_H

#include "device_registers.h"
#include "gpio.h"
#include "gpio_hal.h"
#include <stdint.h>
#include <stddef.h>

/* RCC Definitions */
#define RCC_APB1ENR1_I2C1EN (1U << 21)
#define RCC_APB1ENR1_I2C2EN (1U << 22)
#define RCC_APB1ENR1_I2C3EN (1U << 23)

/* I2C CR1 Bits */
#define I2C_CR1_PE (1U << 0)
#define I2C_CR1_TXIE (1U << 1)
#define I2C_CR1_RXIE (1U << 2)
#define I2C_CR1_ADDRIE (1U << 3) /* Not used in basic master but good to have  \
                                  */
#define I2C_CR1_NACKIE (1U << 4)
#define I2C_CR1_STOPIE (1U << 5)
#define I2C_CR1_TCIE (1U << 6)
#define I2C_CR1_ERRIE (1U << 7)

/* I2C CR2 Bits */
#define I2C_CR2_RD_WRN (1U << 10) /* 0=Write, 1=Read */
#define I2C_CR2_START (1U << 13)
#define I2C_CR2_STOP (1U << 14)
#define I2C_CR2_NBYTES_Pos (16U)
#define I2C_CR2_NBYTES_Msk (0xFFU << I2C_CR2_NBYTES_Pos)
#define I2C_CR2_AUTOEND (1U << 25)

/* I2C ISR Bits */
#define I2C_ISR_TXE (1U << 0)
#define I2C_ISR_RXNE (1U << 2)
#define I2C_ISR_NACKF (1U << 4)
#define I2C_ISR_STOPF (1U << 5)
#define I2C_ISR_TC (1U << 6)
#define I2C_ISR_BUSY (1U << 15)

/* I2C ICR Bits */
#define I2C_ICR_STOPCF (1U << 5)
#define I2C_ICR_NACKCF (1U << 4)

typedef enum {
  I2C_SPEED_STANDARD = 100000,
  I2C_SPEED_FAST = 400000
} I2C_Speed_t;

typedef struct {
  I2C_Speed_t Speed;
} I2C_Config_t;

/**
 * @brief Enable clocks and configure pins based on the I2C instance.
 */
static inline void i2c_hal_enable_clocks_and_pins(I2C_TypeDef *I2Cx) {
  if (I2Cx == I2C1) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;
    /* PB6=SCL, PB7=SDA (AF4) on Nucleo-L476RG */
    gpio_init(GPIO_PORT_B, 6, GPIO_MODE_AF, GPIO_PULL_UP, 4);
    gpio_init(GPIO_PORT_B, 7, GPIO_MODE_AF, GPIO_PULL_UP, 4);
  } else if (I2Cx == I2C2) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C2EN;
    /* PB10=SCL, PB11=SDA (AF4) */
    gpio_init(GPIO_PORT_B, 10, GPIO_MODE_AF, GPIO_PULL_UP, 4);
    gpio_init(GPIO_PORT_B, 11, GPIO_MODE_AF, GPIO_PULL_UP, 4);
  } else if (I2Cx == I2C3) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C3EN;
    /* PC0=SCL, PC1=SDA (AF4) */
    gpio_init(GPIO_PORT_C, 0, GPIO_MODE_AF, GPIO_PULL_UP, 4);
    gpio_init(GPIO_PORT_C, 1, GPIO_MODE_AF, GPIO_PULL_UP, 4);
  }
}

static inline void i2c_hal_init(void *hal_handle, void *config_ptr) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  I2C_Config_t *cfg = (I2C_Config_t *)config_ptr;
  if (!I2Cx || !cfg)
    return;

  i2c_hal_enable_clocks_and_pins(I2Cx);

  /* Disable I2C */
  I2Cx->CR1 &= ~I2C_CR1_PE;

  /* Timing Configuration */
  /* Note: TIMINGR calculation is complex and depends on source clock.
     For 80MHz SysClk (APB1), here are approximate values.
  */
  if (cfg->Speed == I2C_SPEED_STANDARD) {
    /* 100kHz @ 80MHz: Example value */
    I2Cx->TIMINGR = 0x10909CEC;
  } else {
    /* 400kHz @ 80MHz: Example value */
    I2Cx->TIMINGR = 0x00702991;
  }

  /* Enable I2C */
  I2Cx->CR1 |= I2C_CR1_PE;
}

static inline int i2c_hal_master_transmit(void *hal_handle, uint16_t addr,
                                          const uint8_t *data, size_t len) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;

  if (!I2Cx || !data || len == 0U || len > 255U)
    return -1; /* Simple driver supports max 255 bytes */

  /* Wait if busy */
  while (I2Cx->ISR & I2C_ISR_BUSY)
    ;

  /* Configure CR2: SADD, NBYTES, AUTOEND, Write(RD_WRN=0), START */
  uint32_t cr2 = (addr << 1); /* SADD is 7-bit address shifted left by 1 */
  cr2 |= (len << I2C_CR2_NBYTES_Pos);
  cr2 |= I2C_CR2_AUTOEND;
  cr2 |= I2C_CR2_START;
  /* RD_WRN bit 0 is Write, so we leave it 0 */

  I2Cx->CR2 = cr2;

  for (size_t i = 0; i < len; i++) {
    /* Wait for TXE */
    while (!(I2Cx->ISR & I2C_ISR_TXE)) {
      if (I2Cx->ISR & I2C_ISR_NACKF) {
        I2Cx->ICR |= I2C_ICR_NACKCF;
        return -1;
      }
    }
    I2Cx->TXDR = data[i];
  }

  /* Wait for STOPF */
  while (!(I2Cx->ISR & I2C_ISR_STOPF)) {
    if (I2Cx->ISR & I2C_ISR_NACKF) {
      I2Cx->ICR |= I2C_ICR_NACKCF;
      return -1;
    }
  }
  I2Cx->ICR |= I2C_ICR_STOPCF;
  I2Cx->CR2 = 0; /* Clear CR2 */
  return 0;
}

static inline int i2c_hal_master_receive(void *hal_handle, uint16_t addr,
                                         uint8_t *data, size_t len) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;

  if (!I2Cx || !data || len == 0U || len > 255U)
    return -1;

  while (I2Cx->ISR & I2C_ISR_BUSY)
    ;

  uint32_t cr2 = (addr << 1);
  cr2 |= (len << I2C_CR2_NBYTES_Pos);
  cr2 |= I2C_CR2_AUTOEND;
  cr2 |= I2C_CR2_START;
  cr2 |= I2C_CR2_RD_WRN; /* Read */

  I2Cx->CR2 = cr2;

  for (size_t i = 0; i < len; i++) {
    while (!(I2Cx->ISR & I2C_ISR_RXNE))
      ;
    data[i] = (uint8_t)I2Cx->RXDR;
  }

  while (!(I2Cx->ISR & I2C_ISR_STOPF))
    ;
  I2Cx->ICR |= I2C_ICR_STOPCF;
  I2Cx->CR2 = 0;
  return 0;
}

/**
 * @brief Enable/Disable Event Interrupts (TXIE, RXIE, TCIE, STOPIE, NACKIE).
 * For simplicity, we enable all relevant event sources.
 * In a more complex driver, we might toggle TXIE/RXIE individually.
 */
static inline void i2c_hal_enable_ev_irq(void *hal_handle, uint8_t enable) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  uint32_t mask = (I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_TCIE | I2C_CR1_STOPIE |
                   I2C_CR1_NACKIE);
  if (enable) {
    I2Cx->CR1 |= mask;
  } else {
    I2Cx->CR1 &= ~mask;
  }
}

/**
 * @brief Enable/Disable Error Interrupts (ERRIE).
 */
static inline void i2c_hal_enable_er_irq(void *hal_handle, uint8_t enable) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  if (enable) {
    I2Cx->CR1 |= I2C_CR1_ERRIE;
  } else {
    I2Cx->CR1 &= ~I2C_CR1_ERRIE;
  }
}

/**
 * @brief Configure and start an async master transfer.
 * @param hal_handle I2C peripheral base.
 * @param addr 7-bit I2C address.
 * @param len Number of bytes to transfer.
 * @param read 0 for write, 1 for read.
 * @return 0 on success, -1 on error.
 */
static inline int i2c_hal_start_master_transfer(void *hal_handle, uint16_t addr,
                                                size_t len, uint8_t read) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;

  if (!I2Cx || len == 0U || len > 255U)
    return -1;

  if (I2Cx->ISR & I2C_ISR_BUSY)
    return -1;

  uint32_t cr2 = (addr << 1);
  cr2 |= ((uint32_t)len << I2C_CR2_NBYTES_Pos);
  cr2 |= I2C_CR2_AUTOEND;
  cr2 |= I2C_CR2_START;
  if (read) {
    cr2 |= I2C_CR2_RD_WRN;
  }

  I2Cx->CR2 = cr2;
  return 0;
}

static inline uint8_t i2c_hal_tx_ready(void *hal_handle) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  return (I2Cx != NULL) && ((I2Cx->ISR & I2C_ISR_TXE) != 0U);
}

static inline uint8_t i2c_hal_rx_ready(void *hal_handle) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  return (I2Cx != NULL) && ((I2Cx->ISR & I2C_ISR_RXNE) != 0U);
}

static inline void i2c_hal_write_tx_byte(void *hal_handle, uint8_t byte) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  if (I2Cx) {
    I2Cx->TXDR = byte;
  }
}

static inline uint8_t i2c_hal_read_rx_byte(void *hal_handle) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  if (!I2Cx) {
    return 0U;
  }
  return (uint8_t)I2Cx->RXDR;
}

static inline uint8_t i2c_hal_stop_detected(void *hal_handle) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  return (I2Cx != NULL) && ((I2Cx->ISR & I2C_ISR_STOPF) != 0U);
}

static inline void i2c_hal_clear_stop(void *hal_handle) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  if (I2Cx) {
    I2Cx->ICR |= I2C_ICR_STOPCF;
  }
}

static inline uint8_t i2c_hal_nack_detected(void *hal_handle) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  return (I2Cx != NULL) && ((I2Cx->ISR & I2C_ISR_NACKF) != 0U);
}

static inline void i2c_hal_clear_nack(void *hal_handle) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  if (I2Cx) {
    I2Cx->ICR |= I2C_ICR_NACKCF;
  }
}

static inline void i2c_hal_clear_config(void *hal_handle) {
  I2C_TypeDef *I2Cx = (I2C_TypeDef *)hal_handle;
  if (I2Cx) {
    I2Cx->CR2 = 0U;
  }
}

#endif /* I2C_HAL_STM32_H */
