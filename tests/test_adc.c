#include "unity.h"
#include "adc.h"
#include "mock_drivers.h"
#include "allocator.h"
#include "test_common.h"
#include <stdio.h>

struct adc_context {
    void *hal_handle;
};

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_adc_create_should_InitHal(void) {
    void* hal_handle = (void*)0x1000;
    void* config = (void*)0x2000;

    mock_adc_init_return = 0;

    adc_port_t port = adc_create(hal_handle, config);
    TEST_ASSERT_NOT_NULL(port);
}

void test_adc_create_should_FailIfHalInitFails(void) {
    mock_adc_init_return = -1;

    adc_port_t port = adc_create((void*)1, (void*)2);
    TEST_ASSERT_NULL(port);
}

void test_adc_read_channel_should_ReturnHalValue(void) {
    struct adc_context ctx;
    ctx.hal_handle = (void*)0x1000;
    
    mock_adc_read_val = 1234;
    uint16_t val = 0;
    adc_read_channel((adc_port_t)&ctx, 5, &val);
    TEST_ASSERT_EQUAL(1234, val);
}

void run_adc_tests(void) {
    printf("\n=== Starting ADC Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_adc.c");
    RUN_TEST(test_adc_create_should_InitHal);
    RUN_TEST(test_adc_create_should_FailIfHalInitFails);
    RUN_TEST(test_adc_read_channel_should_ReturnHalValue);

    printf("=== ADC Tests Complete ===\n");
}