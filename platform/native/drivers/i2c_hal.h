#ifndef I2C_HAL_NATIVE_H
#define I2C_HAL_NATIVE_H

#include <stdint.h>
#include <stddef.h>

/* Minimal I2C register model for native simulation */
typedef struct {
    uint32_t CR1;
    uint32_t CR2;
    uint32_t ISR;
    uint32_t ICR;
    uint32_t TXDR;
    uint32_t RXDR;
    uint32_t TIMINGR;
} I2C_TypeDef;

extern I2C_TypeDef I2C1_Inst;
extern I2C_TypeDef I2C2_Inst;
extern I2C_TypeDef I2C3_Inst;

#define I2C1 (&I2C1_Inst)
#define I2C2 (&I2C2_Inst)
#define I2C3 (&I2C3_Inst)

/* I2C CR2 Bits */
#define I2C_CR2_RD_WRN (1U << 10)
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

void i2c_hal_init(void *hal_handle, void *config_ptr);
int i2c_hal_master_transmit(void *hal_handle, uint16_t addr, const uint8_t *data, size_t len);
int i2c_hal_master_receive(void *hal_handle, uint16_t addr, uint8_t *data, size_t len);
void i2c_hal_enable_ev_irq(void *hal_handle, uint8_t enable);
void i2c_hal_enable_er_irq(void *hal_handle, uint8_t enable);
int i2c_hal_master_transmit_dma(void *hal_handle, uint16_t addr, const uint8_t *data, size_t len, void (*done_cb)(void *), void *cb_arg);
int i2c_hal_master_receive_dma(void *hal_handle, uint16_t addr, uint8_t *data, size_t len, void (*done_cb)(void *), void *cb_arg);

#endif /* I2C_HAL_NATIVE_H */
