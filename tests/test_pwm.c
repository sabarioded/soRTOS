#include "unity.h"
#include "pwm.h"
#include "mock_drivers.h"
#include "allocator.h"
#include "test_common.h"
#include <stdio.h>

struct pwm_context {
    void *hal_handle;
    uint8_t channel;
};

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_pwm_create_should_InitHalAndSetDuty(void) {
    void* hal_handle = (void*)0x5000;
    mock_pwm_init_return = 0;

    pwm_port_t port = pwm_create(hal_handle, 1, 1000, 50);
    TEST_ASSERT_NOT_NULL(port);
    TEST_ASSERT_EQUAL(50, mock_pwm_last_duty);
}

void test_pwm_create_should_FailIfHalInitFails(void) {
    void* hal_handle = (void*)0x5000;
    mock_pwm_init_return = -1;

    pwm_port_t port = pwm_create(hal_handle, 1, 1000, 50);
    TEST_ASSERT_NULL(port);
}

void test_pwm_control_functions(void) {
    struct pwm_context ctx;
    ctx.hal_handle = (void*)0x5000;
    ctx.channel = 2;
    pwm_port_t port = (pwm_port_t)&ctx;

    pwm_set_duty(port, 75);
    TEST_ASSERT_EQUAL(75, mock_pwm_last_duty);

    pwm_start(port);
    TEST_ASSERT_EQUAL(1, mock_pwm_start_called);

    pwm_stop(port);
    TEST_ASSERT_EQUAL(1, mock_pwm_stop_called);
}

void run_pwm_tests(void) {
    printf("\n=== Starting PWM Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_pwm.c");
    RUN_TEST(test_pwm_create_should_InitHalAndSetDuty);
    RUN_TEST(test_pwm_create_should_FailIfHalInitFails);
    RUN_TEST(test_pwm_control_functions);

    printf("=== PWM Tests Complete ===\n");
}