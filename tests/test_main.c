/* 
 * Main Test Runner
 * Aggregates and executes all unit test suites for the kernel.
 */
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

    /* Return failure count (0 = success) */
    return UNITY_END();
}