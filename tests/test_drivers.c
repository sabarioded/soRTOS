#include "mock_drivers.h"
#include "exti_hal.h"

/* Watchdog */
int mock_watchdog_init_return = 0;
uint32_t mock_watchdog_init_timeout_arg = 0;
int mock_watchdog_kick_called = 0;

int watchdog_hal_init(uint32_t timeout_ms) {
    mock_watchdog_init_timeout_arg = timeout_ms;
    return mock_watchdog_init_return;
}

void watchdog_hal_kick(void) {
    mock_watchdog_kick_called++;
}

/* Systick */
int mock_systick_init_return = 0;
uint32_t mock_systick_init_reload_arg = 0;

int systick_hal_init(uint32_t reload_val) {
    mock_systick_init_reload_arg = reload_val;
    return mock_systick_init_return;
}

void systick_hal_irq_handler(void) {
}

/* Button */
int mock_button_init_called = 0;
uint32_t mock_button_read_return = 0;

void button_hal_init(void) {
    mock_button_init_called++;
}

uint32_t button_hal_read(void) {
    return mock_button_read_return;
}

/* LED */
int mock_led_init_called = 0;
int mock_led_on_called = 0;
int mock_led_off_called = 0;
int mock_led_toggle_called = 0;

void led_hal_init(void) { mock_led_init_called++; }
void led_hal_on(void) { mock_led_on_called++; }
void led_hal_off(void) { mock_led_off_called++; }
void led_hal_toggle(void) { mock_led_toggle_called++; }

/* UART */
int mock_uart_init_called = 0;
int mock_uart_enable_rx_irq_arg = -1;
int mock_uart_enable_tx_irq_arg = -1;
uint8_t mock_uart_last_byte_written = 0;

void uart_hal_init(void *hal_handle, void *config_ptr, uint32_t clock_freq) {
    (void)hal_handle; (void)config_ptr; (void)clock_freq;
    mock_uart_init_called++;
}

void uart_hal_enable_rx_interrupt(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    mock_uart_enable_rx_irq_arg = enable;
}

void uart_hal_enable_tx_interrupt(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    mock_uart_enable_tx_irq_arg = enable;
}

void uart_hal_write_byte(void *hal_handle, uint8_t byte) {
    (void)hal_handle;
    mock_uart_last_byte_written = byte;
}

/* I2C */
int mock_i2c_init_called = 0;
int mock_i2c_transmit_return = 0;
int mock_i2c_receive_return = 0;

void i2c_hal_init(void *hal_handle, void *config_ptr) {
    (void)hal_handle; (void)config_ptr;
    mock_i2c_init_called++;
}

int i2c_hal_master_transmit(void *hal_handle, uint16_t addr, const uint8_t *data, size_t len) {
    (void)hal_handle; (void)addr; (void)data; (void)len;
    return mock_i2c_transmit_return;
}

int i2c_hal_master_receive(void *hal_handle, uint16_t addr, uint8_t *data, size_t len) {
    (void)hal_handle; (void)addr; (void)data; (void)len;
    return mock_i2c_receive_return;
}

void i2c_hal_enable_ev_irq(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

void i2c_hal_enable_er_irq(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

/* SPI */
int mock_spi_init_called = 0;
uint8_t mock_spi_transfer_return = 0;

void spi_hal_init(void *hal_handle, void *config_ptr) {
    (void)hal_handle; (void)config_ptr;
    mock_spi_init_called++;
}

uint8_t spi_hal_transfer_byte(void *hal_handle, uint8_t byte) {
    (void)hal_handle; (void)byte;
    return mock_spi_transfer_return;
}

void spi_hal_enable_rx_irq(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

void spi_hal_enable_tx_irq(void *hal_handle, uint8_t enable) {
    (void)hal_handle;
    (void)enable;
}

/* ADC */
int mock_adc_init_return = 0;
int mock_adc_read_return = 0;
uint16_t mock_adc_read_val = 0;

int adc_hal_init(void *hal_handle, void *config_ptr) {
    (void)hal_handle; (void)config_ptr;
    return mock_adc_init_return;
}

int adc_hal_read(void *hal_handle, uint32_t channel, uint16_t *value) {
    (void)hal_handle; (void)channel;
    *value = mock_adc_read_val;
    return mock_adc_read_return;
}

/* DMA */
int mock_dma_init_called = 0;
int mock_dma_start_called = 0;
int mock_dma_stop_called = 0;

void dma_hal_init(void *hal_handle, void *config_ptr) { (void)hal_handle; (void)config_ptr; mock_dma_init_called++; }
void dma_hal_start(void *hal_handle, uint32_t src, uint32_t dst, uint32_t length) { (void)hal_handle; (void)src; (void)dst; (void)length; mock_dma_start_called++; }
void dma_hal_stop(void *hal_handle) { (void)hal_handle; mock_dma_stop_called++; }

/* EXTI */
int mock_exti_configure_called = 0;
int mock_exti_enable_called = 0;
int mock_exti_disable_called = 0;

void exti_hal_configure(uint8_t pin, uint8_t port, exti_trigger_t trigger) { (void)pin; (void)port; (void)trigger; mock_exti_configure_called++; }
void exti_hal_enable(uint8_t pin) { (void)pin; mock_exti_enable_called++; }
void exti_hal_disable(uint8_t pin) { (void)pin; mock_exti_disable_called++; }

/* DAC */
int mock_dac_init_return = 0;
uint16_t mock_dac_last_write_value = 0;

int dac_hal_init(dac_channel_t channel) {
    (void)channel;
    return mock_dac_init_return;
}

void dac_hal_write(dac_channel_t channel, uint16_t value) {
    (void)channel;
    mock_dac_last_write_value = value;
}

/* PWM */
int mock_pwm_init_return = 0;
uint8_t mock_pwm_last_duty = 0;
int mock_pwm_start_called = 0;
int mock_pwm_stop_called = 0;

int pwm_hal_init(void *hal_handle, uint8_t channel, uint32_t freq_hz) {
    (void)hal_handle; (void)channel; (void)freq_hz;
    return mock_pwm_init_return;
}
void pwm_hal_set_duty(void *hal_handle, uint8_t channel, uint8_t duty) {
    (void)hal_handle; (void)channel;
    mock_pwm_last_duty = duty;
}
void pwm_hal_start(void *hal_handle, uint8_t channel) { (void)hal_handle; (void)channel; mock_pwm_start_called++; }
void pwm_hal_stop(void *hal_handle, uint8_t channel) { (void)hal_handle; (void)channel; mock_pwm_stop_called++; }

/* RTC */
int mock_rtc_init_return = 0;
rtc_time_t mock_rtc_time_val = {0};
rtc_date_t mock_rtc_date_val = {0};

int rtc_hal_init(void) { return mock_rtc_init_return; }
void rtc_hal_get_time(rtc_time_t *time) { *time = mock_rtc_time_val; }
void rtc_hal_set_time(const rtc_time_t *time) { mock_rtc_time_val = *time; }
void rtc_hal_get_date(rtc_date_t *date) { *date = mock_rtc_date_val; }
void rtc_hal_set_date(const rtc_date_t *date) { mock_rtc_date_val = *date; }

/* Flash */
int mock_flash_unlock_called = 0;
int mock_flash_lock_called = 0;
int mock_flash_erase_return = 0;
uint32_t mock_flash_erase_addr = 0;
int mock_flash_program_return = 0;
uint32_t mock_flash_program_addr = 0;
size_t mock_flash_program_len = 0;

void flash_hal_unlock(void) {
    mock_flash_unlock_called++;
}

void flash_hal_lock(void) {
    mock_flash_lock_called++;
}

int flash_hal_erase_page(uint32_t page_addr) {
    mock_flash_erase_addr = page_addr;
    return mock_flash_erase_return;
}

int flash_hal_program(uint32_t addr, const void *data, size_t len) {
    (void)data;
    mock_flash_program_addr = addr;
    mock_flash_program_len = len;
    return mock_flash_program_return;
}

void mock_drivers_reset(void) {
    mock_watchdog_init_return = 0; mock_watchdog_init_timeout_arg = 0; mock_watchdog_kick_called = 0;
    mock_systick_init_return = 0; mock_systick_init_reload_arg = 0;
    mock_button_init_called = 0; mock_button_read_return = 0;
    mock_led_init_called = 0; mock_led_on_called = 0; mock_led_off_called = 0; mock_led_toggle_called = 0;
    
    mock_dac_init_return = 0;
    mock_dac_last_write_value = 0;

    mock_pwm_init_return = 0;
    mock_pwm_last_duty = 0;
    mock_pwm_start_called = 0;
    mock_pwm_stop_called = 0;

    mock_rtc_init_return = 0;
    /* Reset time/date structs */
    rtc_time_t zero_time = {0}; mock_rtc_time_val = zero_time;
    rtc_date_t zero_date = {0}; mock_rtc_date_val = zero_date;

    mock_flash_unlock_called = 0;
    mock_flash_lock_called = 0;
    mock_flash_erase_return = 0;
    mock_flash_erase_addr = 0;
    mock_flash_program_return = 0;
    mock_flash_program_addr = 0;
    mock_flash_program_len = 0;

    /* Reset others as needed */
}
