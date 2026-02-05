#include "unity.h"
#include "dac.h"
#include "dac_hal.h"
#include "mock_drivers.h"
#include "test_common.h"
#include <stdio.h>

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_dac_init_should_CallHalInit(void) {
    mock_dac_init_return = 0;
    TEST_ASSERT_EQUAL(0, dac_init(DAC_CHANNEL_1));
    
    mock_dac_init_return = -1;
    TEST_ASSERT_EQUAL(-1, dac_init(DAC_CHANNEL_2));
}

void test_dac_write_should_CallHalWrite(void) {
    dac_write(DAC_CHANNEL_1, 2048);
    TEST_ASSERT_EQUAL(2048, mock_dac_last_write_value);
    
    dac_write(DAC_CHANNEL_1, 4095);
    TEST_ASSERT_EQUAL(4095, mock_dac_last_write_value);
}

void run_dac_tests(void) {
    printf("\n=== Starting DAC Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_dac.c");
    RUN_TEST(test_dac_init_should_CallHalInit);
    RUN_TEST(test_dac_write_should_CallHalWrite);

    printf("=== DAC Tests Complete ===\n");
}
