#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "spinlock.h"
#include "arch_ops.h"
#include <setjmp.h>
#include "test_common.h"

/* Platform Mock: No hardware to initialize on host */
void platform_init(void) {
}

/* Platform Mock: Panic just exits the test runner with failure */
void platform_panic(void) {
    fprintf(stderr, "PLATFORM PANIC TRIGGERED!\n");
    exit(EXIT_FAILURE);
}

/* Mock State: CPU Frequency */
size_t mock_cpu_freq = 1000000;

/* Platform Mock: Return the mock frequency */
size_t platform_get_cpu_freq(void) {
    return mock_cpu_freq; 
}

/* Platform Mock: Systick init is a no-op on host */
void platform_systick_init(size_t tick_hz) {
    (void)tick_hz;
}

/* Mock State: Current system tick count */
size_t mock_ticks = 0;

/* Platform Mock: Return the controlled mock tick count */
size_t platform_get_ticks(void) {
    return mock_ticks;
}

/* Platform Mock: CPU Idle does nothing in single-threaded test */
void platform_cpu_idle(void) {

}

/* Platform Mock: Scheduler start just returns (we drive it manually) */
void platform_start_scheduler(size_t stack_pointer) {
    (void)stack_pointer;
}

/* Mock State: Jump buffer for context switch simulation */
jmp_buf yield_jump;
int mock_yield_count = 0;

/* Test Hooks: Pointers to setUp/tearDown functions for the active suite */
void (*test_setUp_hook)(void) = NULL;
void (*test_tearDown_hook)(void) = NULL;

void setUp(void) {
    if (test_setUp_hook) {
        test_setUp_hook();
    }
}

void tearDown(void) {
    if (test_tearDown_hook) {
        test_tearDown_hook();
    }
}

/* 
 * Platform Mock: Yield
 * Simulates a context switch by jumping back to the test function 
 * via longjmp. This allows tests to verify blocking behavior.
 */
void platform_yield(void) {
    mock_yield_count++;
    longjmp(yield_jump, 1);
}

/* Platform Mock: UART init */
void platform_uart_init(void) {}

/* Platform Mock: UART receive (always empty) */
int platform_uart_getc(char *out_ch) {
    return -1; 
}

/* Platform Mock: UART send (prints to stdout) */
int platform_uart_puts(const char *s) {
    return printf("%s", s);
}

/* Platform Mock: Reset exits the process */
void platform_reset(void) {
    exit(0);
}

/* Platform Mock: UART callbacks (no-op) */
void platform_uart_set_rx_notify(uint16_t task_id) {
    (void)task_id;
}

void platform_uart_set_rx_queue(queue_t *q) {
    (void)q;
}

void platform_uart_set_tx_queue(queue_t *q) {
    (void)q;
}
