#ifndef MOCK_DRIVERS_H
#define MOCK_DRIVERS_H

#include <stdint.h>
#include <stddef.h>
#include "rtc.h"
#include "dac.h"

/* Watchdog Mocks */
extern int mock_watchdog_init_return;
extern uint32_t mock_watchdog_init_timeout_arg;
extern int mock_watchdog_kick_called;

/* Systick Mocks */
extern int mock_systick_init_return;
extern uint32_t mock_systick_init_reload_arg;

/* Button Mocks */
extern int mock_button_init_called;
extern uint32_t mock_button_read_return;

/* LED Mocks */
extern int mock_led_init_called;
extern int mock_led_on_called;
extern int mock_led_off_called;
extern int mock_led_toggle_called;

/* UART Mocks */
extern int mock_uart_init_called;
extern int mock_uart_enable_rx_irq_arg;
extern int mock_uart_enable_tx_irq_arg;
extern uint8_t mock_uart_last_byte_written;

/* I2C Mocks */
extern int mock_i2c_init_called;
extern int mock_i2c_transmit_return;
extern int mock_i2c_receive_return;

/* SPI Mocks */
extern int mock_spi_init_called;
extern uint8_t mock_spi_transfer_return;

/* ADC Mocks */
extern int mock_adc_init_return;
extern int mock_adc_read_return;
extern uint16_t mock_adc_read_val;

/* DMA Mocks */
extern int mock_dma_init_called;
extern int mock_dma_start_called;
extern int mock_dma_stop_called;

/* EXTI Mocks */
extern int mock_exti_configure_called;
extern int mock_exti_enable_called;
extern int mock_exti_disable_called;

/* DAC Mocks */
extern int mock_dac_init_return;
extern uint16_t mock_dac_last_write_value;

/* PWM Mocks */
extern int mock_pwm_init_return;
extern uint8_t mock_pwm_last_duty;
extern int mock_pwm_start_called;
extern int mock_pwm_stop_called;

/* RTC Mocks */
extern int mock_rtc_init_return;
extern rtc_time_t mock_rtc_time_val;
extern rtc_date_t mock_rtc_date_val;

/* Flash Mocks */
extern int mock_flash_unlock_called;
extern int mock_flash_lock_called;
extern int mock_flash_erase_return;
extern uint32_t mock_flash_erase_addr;
extern int mock_flash_program_return;
extern uint32_t mock_flash_program_addr;
extern size_t mock_flash_program_len;

/* Helper to reset all mocks */
void mock_drivers_reset(void);

#endif /* MOCK_DRIVERS_H */