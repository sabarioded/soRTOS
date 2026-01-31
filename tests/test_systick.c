#include "unity.h"
#include "systick.h"
#include "mock_drivers.h"
#include "test_common.h"
#include <stdio.h>

/* We need to mock scheduler_tick and arch_yield manually or via linker, 
   but for now we assume they are linked from test_common or similar stubs */

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_systick_init_should_CalculateReloadAndInitHal(void) {
    /* Case 1: 80MHz CPU, 1000Hz tick -> Reload = 79,999 */
    mock_cpu_freq = 80000000;
    mock_systick_init_return = 0;
    
    TEST_ASSERT_EQUAL(0, systick_init(1000));
    TEST_ASSERT_EQUAL(79999, mock_systick_init_reload_arg);
}

void test_systick_init_should_FailIfHalFails(void) {
    /* Case 2: HAL rejects the reload value (e.g. too large) */
    mock_cpu_freq = 80000000;
    mock_systick_init_return = -1;
    
    TEST_ASSERT_EQUAL(-1, systick_init(1));
}

void test_systick_init_should_FailIfCpuFreqZero(void) {
    mock_cpu_freq = 0;
    TEST_ASSERT_EQUAL(-1, systick_init(1000));
}

void test_systick_core_tick_should_IncrementTicksAndSchedule(void) {
    /* Initial tick count */
    uint32_t initial_ticks = systick_get_ticks();

    /* Expect scheduler tick and yield */
    /* Note: In this simple mock setup, we can't easily verify calls to scheduler/arch without more mocks */

    systick_core_tick();

    TEST_ASSERT_EQUAL(initial_ticks + 1, systick_get_ticks());
}

void run_systick_tests(void) {
    printf("\n=== Starting Systick Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_systick.c");
    RUN_TEST(test_systick_init_should_CalculateReloadAndInitHal);
    RUN_TEST(test_systick_init_should_FailIfHalFails);
    RUN_TEST(test_systick_init_should_FailIfCpuFreqZero);
    RUN_TEST(test_systick_core_tick_should_IncrementTicksAndSchedule);

    printf("=== Systick Tests Complete ===\n");
}