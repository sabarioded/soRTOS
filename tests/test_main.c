#include "unity.h"

/* External test suite runners */
extern void run_queue_tests(void);
extern void run_scheduler_tests(void);
extern void run_mutex_tests(void);
extern void run_semaphore_tests(void);
extern void run_allocator_tests(void);
extern void run_timer_tests(void);
extern void run_logger_tests(void);
extern void run_utils_tests(void);
extern void run_cli_tests(void);
extern void run_event_group_tests(void);
extern void run_mempool_tests(void);
extern void run_watchdog_tests(void);
extern void run_systick_tests(void);
extern void run_button_tests(void);
extern void run_led_tests(void);
extern void run_uart_tests(void);
extern void run_i2c_tests(void);
extern void run_spi_tests(void);
extern void run_dma_tests(void);
extern void run_adc_tests(void);
extern void run_exti_tests(void);
extern void run_dac_tests(void);
extern void run_pwm_tests(void);
extern void run_rtc_tests(void);
extern void run_flash_tests(void);

/* Main entry point for the unit test executable */
int main(void) {
    /* Initialize Unity framework */
    UNITY_BEGIN();
    
    run_allocator_tests();
    run_scheduler_tests();
    run_queue_tests();
    run_mutex_tests();
    run_semaphore_tests();
    run_timer_tests();
    run_logger_tests();
    run_utils_tests();
    run_cli_tests();
    run_event_group_tests();
    run_mempool_tests();
    run_watchdog_tests();
    run_systick_tests();
    run_button_tests();
    run_led_tests();
    run_uart_tests();
    run_i2c_tests();
    run_spi_tests();
    run_dma_tests();
    run_adc_tests();
    run_exti_tests();
    run_dac_tests();
    run_pwm_tests();
    run_rtc_tests();
    run_flash_tests();

    /* Return failure count (0 = success) */
    return UNITY_END();
}