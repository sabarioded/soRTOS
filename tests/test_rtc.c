#include "unity.h"
#include "rtc.h"
#include "rtc_hal.h"
#include "mock_drivers.h"
#include "test_common.h"
#include <stdio.h>

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_rtc_init_should_CallHalInit(void) {
    mock_rtc_init_return = 0;
    TEST_ASSERT_EQUAL(0, rtc_init());
    
    mock_rtc_init_return = -1;
    TEST_ASSERT_EQUAL(-1, rtc_init());
}

void test_rtc_time_accessors(void) {
    rtc_time_t set_t = {12, 30, 45};
    rtc_set_time(&set_t);
    
    TEST_ASSERT_EQUAL(12, mock_rtc_time_val.hours);
    TEST_ASSERT_EQUAL(30, mock_rtc_time_val.minutes);
    TEST_ASSERT_EQUAL(45, mock_rtc_time_val.seconds);

    rtc_time_t get_t;
    rtc_get_time(&get_t);
    TEST_ASSERT_EQUAL_MEMORY(&set_t, &get_t, sizeof(rtc_time_t));
}

void test_rtc_date_accessors(void) {
    rtc_date_t set_d = {15, 10, 23, 3}; /* 15th Oct 2023, Wed */
    rtc_set_date(&set_d);
    
    TEST_ASSERT_EQUAL(15, mock_rtc_date_val.day);
    TEST_ASSERT_EQUAL(10, mock_rtc_date_val.month);
    TEST_ASSERT_EQUAL(23, mock_rtc_date_val.year);

    rtc_date_t get_d;
    rtc_get_date(&get_d);
    TEST_ASSERT_EQUAL_MEMORY(&set_d, &get_d, sizeof(rtc_date_t));
}

void run_rtc_tests(void) {
    printf("\n=== Starting RTC Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_rtc.c");
    RUN_TEST(test_rtc_init_should_CallHalInit);
    RUN_TEST(test_rtc_time_accessors);
    RUN_TEST(test_rtc_date_accessors);

    printf("=== RTC Tests Complete ===\n");
}
