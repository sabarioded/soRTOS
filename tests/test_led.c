#include "unity.h"
#include "led.h"
#include "mock_drivers.h"
#include "test_common.h"
#include <stdio.h>

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_led_init_should_CallHalInit(void) {
    led_init();
    TEST_ASSERT_EQUAL(1, mock_led_init_called);
}

void test_led_on_should_CallHalOn(void) {
    led_on();
    TEST_ASSERT_EQUAL(1, mock_led_on_called);
}

void test_led_off_should_CallHalOff(void) {
    led_off();
    TEST_ASSERT_EQUAL(1, mock_led_off_called);
}

void test_led_toggle_should_CallHalToggle(void) {
    led_toggle();
    TEST_ASSERT_EQUAL(1, mock_led_toggle_called);
}

void run_led_tests(void) {
    printf("\n=== Starting LED Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_led.c");
    RUN_TEST(test_led_init_should_CallHalInit);
    RUN_TEST(test_led_on_should_CallHalOn);
    RUN_TEST(test_led_off_should_CallHalOff);
    RUN_TEST(test_led_toggle_should_CallHalToggle);

    printf("=== LED Tests Complete ===\n");
}