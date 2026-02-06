#include "unity.h"
#include "exti.h"
#include "exti_hal.h"
#include "mock_drivers.h"
#include "test_common.h"
#include <stdio.h>

static int callback_called = 0;
static void test_callback(void *arg) {
    callback_called++;
    TEST_ASSERT_EQUAL_PTR((void*)0x1234, arg);
}

static void setUp_local(void) {
    mock_drivers_reset();
    callback_called = 0;
}

static void tearDown_local(void) {
}

void test_exti_configure_should_RegisterCallbackAndConfigHal(void) {
    int res = exti_configure(5, 0, EXTI_TRIGGER_RISING, test_callback, (void*)0x1234);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL(1, mock_exti_configure_called);
}

void test_exti_configure_should_Fail_OnInvalidPin(void) {
    int res = exti_configure(EXTI_HAL_MAX_LINES, 0, EXTI_TRIGGER_RISING, test_callback, (void*)0x1234);
    TEST_ASSERT_EQUAL(-1, res);
    TEST_ASSERT_EQUAL(0, mock_exti_configure_called);
}

void test_exti_enable_disable_should_CallHal(void) {
    exti_enable(5);
    TEST_ASSERT_EQUAL(1, mock_exti_enable_called);
    
    exti_disable(5);
    TEST_ASSERT_EQUAL(1, mock_exti_disable_called);
}

void test_exti_core_irq_handler_should_InvokeCallback(void) {
    /* Setup callback first */
    exti_configure(10, 0, EXTI_TRIGGER_FALLING, test_callback, (void*)0x1234);
    
    /* Simulate IRQ */
    exti_core_irq_handler(10);
    TEST_ASSERT_EQUAL(1, callback_called);
}

void test_exti_core_irq_handler_should_IgnoreInvalidPin(void) {
    exti_core_irq_handler(16);
    TEST_ASSERT_EQUAL(0, callback_called);
}

void run_exti_tests(void) {
    printf("\n=== Starting EXTI Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_exti.c");
    RUN_TEST(test_exti_configure_should_RegisterCallbackAndConfigHal);
    RUN_TEST(test_exti_configure_should_Fail_OnInvalidPin);
    RUN_TEST(test_exti_enable_disable_should_CallHal);
    RUN_TEST(test_exti_core_irq_handler_should_InvokeCallback);
    RUN_TEST(test_exti_core_irq_handler_should_IgnoreInvalidPin);

    printf("=== EXTI Tests Complete ===\n");
}
