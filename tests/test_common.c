#include "unity.h"
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include "queue.h"

/* Shared Mock State */
size_t mock_ticks = 0;
int mock_yield_count = 0;
jmp_buf yield_jump;

/* Platform Mocks */

/**
 * @brief Mock implementation of platform_get_ticks.
 * 
 * Returns the simulated tick count.
 */
size_t platform_get_ticks(void) { 
    return mock_ticks; 
}

/**
 * @brief Mock implementation of platform_yield.
 * 
 * Simulates a context switch by jumping back to the test harness.
 */
void platform_yield(void) { 
    mock_yield_count++; 
    /* Jump back to test function to simulate blocking */
    longjmp(yield_jump, 1);
}

/**
 * @brief Mock implementation of platform_panic.
 * 
 * Fails the test immediately.
 */
void platform_panic(void) { 
    TEST_FAIL_MESSAGE("Platform Panic Triggered"); 
}

/**
 * @brief Mock implementation of platform_cpu_idle.
 */
void platform_cpu_idle(void) { 
}

/**
 * @brief Mock implementation of platform_start_scheduler.
 */
void platform_start_scheduler(size_t stack_pointer) { 
    (void)stack_pointer; 
}

/**
 * @brief Mock implementation of platform_uart_init.
 */
void platform_uart_init(void) {
}

/**
 * @brief Mock implementation of platform_uart_set_rx_notify.
 */
void platform_uart_set_rx_notify(uint16_t task_id) { 
    (void)task_id; 
}

/**
 * @brief Mock implementation of platform_uart_set_rx_queue.
 */
void platform_uart_set_rx_queue(queue_t *q) { 
    (void)q; 
}

/* Arch Mocks */

/**
 * @brief Mock implementation of arch_irq_lock.
 */
uint32_t arch_irq_lock(void) { 
    return 0; 
}

/**
 * @brief Mock implementation of arch_irq_unlock.
 */
void arch_irq_unlock(uint32_t key) { 
    (void)key; 
}

/**
 * @brief Mock implementation of arch_irq_lock_soft.
 */
uint32_t arch_irq_lock_soft(uint32_t priority_threshold) {
    (void)priority_threshold;
    return 0;
}

/**
 * @brief Mock implementation of arch_irq_unlock_soft.
 */
void arch_irq_unlock_soft(uint32_t old) {
    (void)old;
}

/**
 * @brief Mock implementation of arch_dsb.
 */
void arch_dsb(void) {
}

/**
 * @brief Mock implementation of arch_isb.
 */
void arch_isb(void) {
}

/**
 * @brief Mock implementation of arch_dmb.
 */
void arch_dmb(void) {
}

/**
 * @brief Mock implementation of arch_wfi.
 */
void arch_wfi(void) {
}

/**
 * @brief Mock implementation of arch_nop.
 */
void arch_nop(void) {
}

/**
 * @brief Mock implementation of arch_set_psp.
 */
void arch_set_psp(uint32_t psp) {
    (void)psp;
}

/**
 * @brief Mock implementation of arch_irq_disable.
 */
void arch_irq_disable(void) {
}

/**
 * @brief Mock implementation of arch_yield.
 */
void arch_yield(void) {
    platform_yield();
}

/**
 * @brief Mock implementation of arch_cpu_relax.
 */
void arch_cpu_relax(void) {
}

/**
 * @brief Mock implementation of arch_memory_barrier.
 */
void arch_memory_barrier(void) {
}

/**
 * @brief Mock implementation of arch_test_and_set.
 * 
 * Simulates atomic test-and-set behavior.
 */
uint32_t arch_test_and_set(volatile uint32_t *ptr) {
    uint32_t old = *ptr;
    *ptr = 1;
    return old;
}

/**
 * @brief Mock implementation of arch_reset.
 */
void arch_reset(void) {
    TEST_FAIL_MESSAGE("Arch Reset Triggered");
}

/**
 * @brief Mock implementation of arch_initialize_stack.
 */
void *arch_initialize_stack(void *top_of_stack, 
                            void (*task_func)(void *), 
                            void *arg, 
                            void (*exit_handler)(void)) {
    (void)task_func; (void)arg; (void)exit_handler;
    return top_of_stack;
}

/* Global hooks for setUp/tearDown to support multiple test suites in one binary */
void (*test_setUp_hook)(void) = NULL;
void (*test_tearDown_hook)(void) = NULL;

/**
 * @brief Unity setUp callback.
 */
void setUp(void) {
    if (test_setUp_hook) test_setUp_hook();
}

/**
 * @brief Unity tearDown callback.
 */
void tearDown(void) {
    if (test_tearDown_hook) test_tearDown_hook();
}
