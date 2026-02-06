#include "adc_hal.h"
#include "button_hal.h"
#include "dac_hal.h"
#include "dma_hal.h"
#include "exti_hal.h"
#include "flash_hal.h"
#include "gpio_hal.h"
#include "i2c_hal.h"
#include "led_hal.h"
#include "pwm_hal.h"
#include "rtc_hal.h"
#include "spi_hal.h"
#include "systick_hal.h"
#include "uart_hal.h"
#include "watchdog_hal.h"

#include "systick.h"

#include <stdio.h>
#include <string.h>

/* --- Simple stub handles --- */
I2C_Handle_t I2C1_Inst;
I2C_Handle_t I2C2_Inst;
I2C_Handle_t I2C3_Inst;
SPI_Handle_t SPI1_Inst;
SPI_Handle_t SPI2_Inst;

/* --- GPIO --- */
static uint8_t gpio_state[GPIO_PORT_MAX][16];

void gpio_hal_init(gpio_port_t port, uint8_t pin, gpio_mode_t mode, gpio_pull_t pull, uint8_t af) {
    (void)mode;
    (void)pull;
    (void)af;
    if (port >= GPIO_PORT_MAX || pin >= 16) {
        return;
    }
    gpio_state[port][pin] = 0;
}

void gpio_hal_write(gpio_port_t port, uint8_t pin, uint8_t value) {
    if (port >= GPIO_PORT_MAX || pin >= 16) {
        return;
    }
    gpio_state[port][pin] = (value != 0) ? 1U : 0U;
}

void gpio_hal_toggle(gpio_port_t port, uint8_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= 16) {
        return;
    }
    gpio_state[port][pin] ^= 1U;
}

uint8_t gpio_hal_read(gpio_port_t port, uint8_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= 16) {
        return 0;
    }
    return gpio_state[port][pin];
}

/* --- LED --- */
static uint8_t led_state;

void led_hal_init(void) {
    led_state = 0;
}

void led_hal_on(void) {
    led_state = 1;
}

void led_hal_off(void) {
    led_state = 0;
}

void led_hal_toggle(void) {
    led_state ^= 1U;
}

/* --- Button --- */
static uint32_t button_state;

void button_hal_init(void) {
    button_state = 0;
}

uint32_t button_hal_read(void) {
    return button_state;
}

/* --- UART --- */
void uart_hal_init(void *hal_handle, void *config_ptr, uint32_t clock_freq) {
    (void)hal_handle;
    (void)config_ptr;
    (void)clock_freq;
}

void uart_hal_enable_rx_interrupt(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

void uart_hal_enable_tx_interrupt(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

void uart_hal_write_byte(void *hal_handle, uint8_t byte) {
    (void)hal_handle;
    putchar((int)byte);
    fflush(stdout);
}

/* --- Systick --- */
static uint32_t systick_reload;

int systick_hal_init(uint32_t reload_val) {
    systick_reload = reload_val;
    return (systick_reload == 0U) ? -1 : 0;
}

void systick_hal_irq_handler(void) {
    systick_core_tick();
}

/* --- I2C --- */
void i2c_hal_init(void *hal_handle, void *config_ptr) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    (void)config_ptr;
    if (!I2Cx) {
        return;
    }
    I2Cx->last_tx = 0;
    I2Cx->last_rx = 0;
    I2Cx->has_rx = 0;
}

int i2c_hal_master_transmit(void *hal_handle, uint16_t addr, const uint8_t *data, size_t len) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    (void)addr;
    if (!I2Cx || !data || len == 0U) {
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        I2Cx->last_tx = data[i];
    }
    I2Cx->has_rx = 0;
    return 0;
}

int i2c_hal_master_receive(void *hal_handle, uint16_t addr, uint8_t *data, size_t len) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    (void)addr;
    if (!I2Cx || !data || len == 0U) {
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        data[i] = I2Cx->last_rx;
    }
    return 0;
}

void i2c_hal_enable_ev_irq(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

void i2c_hal_enable_er_irq(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

int i2c_hal_start_master_transfer(void *hal_handle, uint16_t addr, size_t len, uint8_t read) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    (void)addr;
    (void)read;
    if (!I2Cx || len == 0U) {
        return -1;
    }
    /* Stub: mark STOPF immediately so a simulated IRQ can complete. */
    I2Cx->ISR |= I2C_ISR_STOPF;
    return 0;
}

uint8_t i2c_hal_tx_ready(void *hal_handle) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    return (I2Cx != NULL);
}

uint8_t i2c_hal_rx_ready(void *hal_handle) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    return (I2Cx != NULL);
}

void i2c_hal_write_tx_byte(void *hal_handle, uint8_t byte) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    if (I2Cx) {
        I2Cx->last_tx = byte;
    }
}

uint8_t i2c_hal_read_rx_byte(void *hal_handle) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    return I2Cx ? I2Cx->last_rx : 0U;
}

uint8_t i2c_hal_stop_detected(void *hal_handle) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    return (I2Cx != NULL) && (I2Cx->ISR & I2C_ISR_STOPF);
}

void i2c_hal_clear_stop(void *hal_handle) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    if (I2Cx) {
        I2Cx->ISR &= ~I2C_ISR_STOPF;
    }
}

uint8_t i2c_hal_nack_detected(void *hal_handle) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    return (I2Cx != NULL) && (I2Cx->ISR & I2C_ISR_NACKF);
}

void i2c_hal_clear_nack(void *hal_handle) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    if (I2Cx) {
        I2Cx->ISR &= ~I2C_ISR_NACKF;
    }
}

void i2c_hal_clear_config(void *hal_handle) {
    I2C_Handle_t *I2Cx = (I2C_Handle_t *)hal_handle;
    if (I2Cx) {
        I2Cx->CR2 = 0U;
    }
}

/* --- SPI --- */
void spi_hal_init(void *hal_handle, void *config_ptr) {
    SPI_Handle_t *SPIx = (SPI_Handle_t *)hal_handle;
    (void)config_ptr;
    if (!SPIx) {
        return;
    }
    SPIx->last_tx = 0;
    SPIx->last_rx = 0;
}

uint8_t spi_hal_transfer_byte(void *hal_handle, uint8_t byte) {
    SPI_Handle_t *SPIx = (SPI_Handle_t *)hal_handle;
    if (SPIx) {
        SPIx->last_tx = byte;
        SPIx->last_rx = byte;
    }
    return byte;
}

void spi_hal_enable_rx_irq(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

void spi_hal_enable_tx_irq(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

/* --- ADC --- */
static uint16_t adc_last_value;

int adc_hal_init(void *hal_handle, void *config_ptr) {
    (void)hal_handle;
    (void)config_ptr;
    adc_last_value = 0;
    return 0;
}

int adc_hal_read(void *hal_handle, uint32_t channel, uint16_t *value) {
    (void)hal_handle;
    (void)channel;
    if (!value) {
        return -1;
    }
    *value = adc_last_value;
    return 0;
}

/* --- DAC --- */
static uint16_t dac_last_value[2];

int dac_hal_init(dac_channel_t channel) {
    if (channel < DAC_CHANNEL_1 || channel > DAC_CHANNEL_2) {
        return -1;
    }
    dac_last_value[channel - 1] = 0;
    return 0;
}

void dac_hal_write(dac_channel_t channel, uint16_t value) {
    if (channel < DAC_CHANNEL_1 || channel > DAC_CHANNEL_2) {
        return;
    }
    dac_last_value[channel - 1] = value;
}

/* --- PWM --- */
static uint8_t pwm_last_duty[4];

int pwm_hal_init(void *hal_handle, uint8_t channel, uint32_t freq_hz) {
    (void)hal_handle;
    (void)freq_hz;
    if (channel == 0 || channel > 4) {
        return -1;
    }
    pwm_last_duty[channel - 1] = 0;
    return 0;
}

void pwm_hal_set_duty(void *hal_handle, uint8_t channel, uint8_t duty) {
    (void)hal_handle;
    if (channel == 0 || channel > 4) {
        return;
    }
    pwm_last_duty[channel - 1] = duty;
}

void pwm_hal_start(void *hal_handle, uint8_t channel) {
    (void)hal_handle;
    (void)channel;
}

void pwm_hal_stop(void *hal_handle, uint8_t channel) {
    (void)hal_handle;
    (void)channel;
}

/* --- RTC --- */
static rtc_time_t rtc_time_state;
static rtc_date_t rtc_date_state;

int rtc_hal_init(void) {
    memset(&rtc_time_state, 0, sizeof(rtc_time_state));
    memset(&rtc_date_state, 0, sizeof(rtc_date_state));
    return 0;
}

void rtc_hal_get_time(rtc_time_t *time) {
    if (time) {
        *time = rtc_time_state;
    }
}

void rtc_hal_set_time(const rtc_time_t *time) {
    if (time) {
        rtc_time_state = *time;
    }
}

void rtc_hal_get_date(rtc_date_t *date) {
    if (date) {
        *date = rtc_date_state;
    }
}

void rtc_hal_set_date(const rtc_date_t *date) {
    if (date) {
        rtc_date_state = *date;
    }
}

/* --- Flash --- */
void flash_hal_unlock(void) {
}

void flash_hal_lock(void) {
}

int flash_hal_erase_page(uint32_t page_addr) {
    (void)page_addr;
    return 0;
}

int flash_hal_program(uint32_t addr, const void *data, size_t len) {
    (void)addr;
    (void)data;
    (void)len;
    return 0;
}

/* --- Watchdog --- */
static uint32_t watchdog_timeout_ms;
static uint32_t watchdog_kicks;

int watchdog_hal_init(uint32_t timeout_ms) {
    watchdog_timeout_ms = timeout_ms;
    watchdog_kicks = 0;
    return (timeout_ms == 0U) ? -1 : 0;
}

void watchdog_hal_kick(void) {
    watchdog_kicks++;
}

/* --- DMA --- */
void dma_hal_init(void *hal_handle, void *config_ptr) {
    (void)hal_handle;
    (void)config_ptr;
}

void dma_hal_start(void *hal_handle, uintptr_t src, uintptr_t dst, size_t length) {
    (void)hal_handle;
    (void)src;
    (void)dst;
    (void)length;
}

void dma_hal_stop(void *hal_handle) {
    (void)hal_handle;
}

/* --- EXTI --- */
void exti_hal_configure(uint8_t pin, uint8_t port, exti_trigger_t trigger) {
    (void)pin;
    (void)port;
    (void)trigger;
}

void exti_hal_enable(uint8_t pin) {
    (void)pin;
}

void exti_hal_disable(uint8_t pin) {
    (void)pin;
}
