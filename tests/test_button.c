#include "unity.h"
#include "button.h"
#include "project_config.h"
#include "mock_drivers.h"
#include "test_common.h"
#include "systick.h"
#include <stdio.h>

static void setUp_local(void) {
    mock_drivers_reset();
    mock_ticks = 0;
}

static void tearDown_local(void) {
}

void test_button_init_should_CallHalInit(void) {
    button_init();
    TEST_ASSERT_EQUAL(1, mock_button_init_called);
}

void test_button_read_should_ReturnHalState(void) {
    mock_button_read_return = 1;
    TEST_ASSERT_EQUAL(1, button_read());
    
    mock_button_read_return = 0;
    TEST_ASSERT_EQUAL(0, button_read());
}

void test_button_poll_should_DebouncePress(void) {
    /* 1. Initial state: Released */
    mock_button_read_return = 0;
    /* We need to mock systick_get_ticks. In test_common.c, platform_get_ticks returns mock_ticks. 
       Assuming systick_get_ticks wraps that or we mock it. 
       Actually systick.c implements systick_get_ticks using a static variable. 
       We can't easily mock it without modifying systick.c or using a linker wrap.
       For this test, we assume systick_get_ticks() works and we can control it via systick_core_tick() or similar?
       No, systick.c is compiled. We can't control g_systick_ticks directly from here unless we add a setter.
       Let's assume we can't fully test time-dependent logic without a better mock for systick_get_ticks.
       Skipping detailed time checks for now or assuming we can't control time easily. */
    
    button_poll();
    TEST_ASSERT_EQUAL(0, button_is_held());
    TEST_ASSERT_EQUAL(0, button_was_pressed());

    /* 2. Press detected (Raw=1), but not stable yet */
    mock_button_read_return = 1;
    button_poll();
    TEST_ASSERT_EQUAL(0, button_is_held()); /* Still 0 */

    /* To properly test debounce, we need to advance time. 
       Since we can't easily mock systick_get_ticks() which is in systick.c, 
       we would need to call systick_core_tick() 50+ times. */
    for(int i=0; i<60; i++) systick_core_tick();
    
    button_poll();
    
    /* Now it should be held and event triggered */
    TEST_ASSERT_EQUAL(1, button_is_held());
    TEST_ASSERT_EQUAL(1, button_was_pressed());

    /* 5. Call again, event should be cleared */
    TEST_ASSERT_EQUAL(1, button_is_held());
    TEST_ASSERT_EQUAL(0, button_was_pressed());
}

void run_button_tests(void) {
    printf("\n=== Starting Button Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_button.c");
    RUN_TEST(test_button_init_should_CallHalInit);
    RUN_TEST(test_button_read_should_ReturnHalState);
    RUN_TEST(test_button_poll_should_DebouncePress);

    printf("=== Button Tests Complete ===\n");
}