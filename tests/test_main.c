#include "unity.h"

/* Declare runner functions from other files */
extern void run_allocator_tests(void);
extern void run_mutex_tests(void);
extern void run_scheduler_tests(void);
extern void run_semaphore_tests(void);

int main(void) {
    UNITY_BEGIN();
    
    /* Run all kernel test suites */
    run_allocator_tests();
    run_mutex_tests();
    run_scheduler_tests();
    run_semaphore_tests();
    
    return UNITY_END();
}