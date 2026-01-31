#include "unity.h"
#include "watchdog.h"
#include "mock_drivers.h"
#include "test_common.h"
#include <stdio.h>

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_watchdog_init_should_CallHalInit(void) {
    mock_watchdog_init_return = 0;
    TEST_ASSERT_EQUAL(0, watchdog_init(1000));
    TEST_ASSERT_EQUAL(1000, mock_watchdog_init_timeout_arg);
    
    mock_watchdog_init_return = -1;
    TEST_ASSERT_EQUAL(-1, watchdog_init(50000));
    TEST_ASSERT_EQUAL(50000, mock_watchdog_init_timeout_arg);
}

void test_watchdog_kick_should_CallHalKick(void) {
    watchdog_kick();
    TEST_ASSERT_EQUAL(1, mock_watchdog_kick_called);
}

void run_watchdog_tests(void) {
    printf("\n=== Starting Watchdog Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_watchdog.c");
    RUN_TEST(test_watchdog_init_should_CallHalInit);
    RUN_TEST(test_watchdog_kick_should_CallHalKick);

    printf("=== Watchdog Tests Complete ===\n");
}