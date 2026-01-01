#include "unity.h"
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include "queue.h"

/* Shared Mock State */
size_t mock_ticks = 0;
int mock_yield_count = 0;
jmp_buf yield_jump;

/* --- Platform Mocks --- */

size_t platform_get_ticks(void) { return mock_ticks; }

void platform_yield(void) { 
    mock_yield_count++; 
    /* Jump back to test function to simulate blocking */
    longjmp(yield_jump, 1);
}

void platform_panic(void) { TEST_FAIL_MESSAGE("Platform Panic Triggered"); }
void platform_cpu_idle(void) { }
void platform_start_scheduler(size_t stack_pointer) { (void)stack_pointer; }

void *platform_initialize_stack(void *top_of_stack, void (*task_func)(void *), void *arg, void (*exit_handler)(void)) {
    (void)task_func; (void)arg; (void)exit_handler;
    return top_of_stack; 
}

void platform_uart_init(void) {}
void platform_uart_set_rx_notify(uint16_t task_id) { (void)task_id; }
void platform_uart_set_rx_queue(queue_t *q) { (void)q; }

/* Global hooks for setUp/tearDown to support multiple test suites in one binary */
void (*test_setUp_hook)(void) = NULL;
void (*test_tearDown_hook)(void) = NULL;

void setUp(void) {
    if (test_setUp_hook) test_setUp_hook();
}

void tearDown(void) {
    if (test_tearDown_hook) test_tearDown_hook();
}