#ifndef I2C_HAL_NATIVE_H
#define I2C_HAL_NATIVE_H

#include <stdint.h>
#include <stddef.h>

/* Minimal I2C handle for native stubs */
typedef struct {
    uint8_t last_tx;
    uint8_t last_rx;
    uint8_t has_rx;
    volatile uint32_t CR2;
    volatile uint32_t ISR;
    volatile uint32_t ICR;
    volatile uint8_t TXDR;
    volatile uint8_t RXDR;
} I2C_Handle_t;

typedef I2C_Handle_t I2C_TypeDef;

extern I2C_Handle_t I2C1_Inst;
extern I2C_Handle_t I2C2_Inst;
extern I2C_Handle_t I2C3_Inst;

#define I2C1 (&I2C1_Inst)
#define I2C2 (&I2C2_Inst)
#define I2C3 (&I2C3_Inst)

typedef enum {
    I2C_SPEED_STANDARD = 100000,
    I2C_SPEED_FAST = 400000
} I2C_Speed_t;

typedef struct {
    I2C_Speed_t Speed;
} I2C_Config_t;

/* Minimal register bit definitions for native builds */
#define I2C_CR2_RD_WRN     (1U << 10)
#define I2C_CR2_START      (1U << 13)
#define I2C_CR2_NBYTES_Pos (16U)
#define I2C_CR2_NBYTES_Msk (0xFFU << I2C_CR2_NBYTES_Pos)
#define I2C_CR2_AUTOEND    (1U << 25)

#define I2C_ISR_TXE   (1U << 0)
#define I2C_ISR_RXNE  (1U << 2)
#define I2C_ISR_STOPF (1U << 5)
#define I2C_ISR_NACKF (1U << 4)

#define I2C_ICR_STOPCF (1U << 5)
#define I2C_ICR_NACKCF (1U << 4)

void i2c_hal_init(void *hal_handle, void *config_ptr);
int i2c_hal_master_transmit(void *hal_handle, uint16_t addr, const uint8_t *data, size_t len);
int i2c_hal_master_receive(void *hal_handle, uint16_t addr, uint8_t *data, size_t len);
void i2c_hal_enable_ev_irq(void *hal_handle, uint8_t enable);
void i2c_hal_enable_er_irq(void *hal_handle, uint8_t enable);
int i2c_hal_start_master_transfer(void *hal_handle, uint16_t addr, size_t len, uint8_t read);
uint8_t i2c_hal_tx_ready(void *hal_handle);
uint8_t i2c_hal_rx_ready(void *hal_handle);
void i2c_hal_write_tx_byte(void *hal_handle, uint8_t byte);
uint8_t i2c_hal_read_rx_byte(void *hal_handle);
uint8_t i2c_hal_stop_detected(void *hal_handle);
void i2c_hal_clear_stop(void *hal_handle);
uint8_t i2c_hal_nack_detected(void *hal_handle);
void i2c_hal_clear_nack(void *hal_handle);
void i2c_hal_clear_config(void *hal_handle);
#endif /* I2C_HAL_NATIVE_H */
