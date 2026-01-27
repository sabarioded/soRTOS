#include "unity.h"
#include "timer.h"
#include "scheduler.h"
#include "allocator.h"
#include <stdio.h>
#include "test_common.h"

static int callback_count = 0;
static void *callback_arg = NULL;

static void timer_cb(void *arg) {
    callback_arg = arg;
    callback_count++;
}

static uint8_t heap[4096];

/* Initialize allocator, scheduler, and timer service */
static void setUp_local(void) {
    allocator_init(heap, sizeof(heap));
    scheduler_init();
    timer_service_init();
    mock_ticks = 0;
    callback_count = 0;
    callback_arg = NULL;
}

/* Cleanup resources */
static void tearDown_local(void) {

}

/* Verify timer creation and deletion */
void test_timer_create_delete(void) {
    sw_timer_t *t = timer_create("test", 100, 0, timer_cb, NULL);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(0, timer_is_active(t));
    TEST_ASSERT_EQUAL_STRING("test", timer_get_name(t));
    TEST_ASSERT_EQUAL(100, timer_get_period(t));
    timer_delete(t);
}

/* Verify getters handle NULL gracefully */
void test_timer_getters_null(void) {
    TEST_ASSERT_NULL(timer_get_name(NULL));
    TEST_ASSERT_EQUAL(0, timer_get_period(NULL));
    TEST_ASSERT_EQUAL(0, timer_is_active(NULL));
}

/* Verify one-shot timer expiry */
void test_timer_one_shot_expiry(void) {
    sw_timer_t *t = timer_create("oneshot", 100, 0, timer_cb, (void*)0xDEAD);
    timer_start(t);
    TEST_ASSERT_EQUAL(1, timer_is_active(t));
    
    /* Check return value of check_expiries (should be 100 ticks remaining) */
    TEST_ASSERT_EQUAL(100, timer_check_expiries());
    
    /* Advance time partially */
    mock_ticks = 50;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(0, callback_count);
    
    /* Advance to expiry */
    mock_ticks = 100;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(1, callback_count);
    TEST_ASSERT_EQUAL_PTR((void*)0xDEAD, callback_arg);
    TEST_ASSERT_EQUAL(0, timer_is_active(t)); /* Should be inactive now */
    
    /* Advance more and shouldn't fire again */
    mock_ticks = 200;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(1, callback_count);
    
    timer_delete(t);
}

/* Verify periodic timer behavior */
void test_timer_periodic(void) {
    sw_timer_t *t = timer_create("periodic", 50, 1, timer_cb, NULL);
    timer_start(t);
    
    /* 1st expiry */
    mock_ticks = 50;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(1, callback_count);
    TEST_ASSERT_EQUAL(1, timer_is_active(t)); /* Should still be active */
    
    /* 2nd expiry */
    mock_ticks = 100;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(2, callback_count);
    
    timer_stop(t);
    timer_delete(t);
}

/* Verify timer restart behavior */
void test_timer_restart(void) {
    sw_timer_t *t = timer_create("restart", 100, 0, timer_cb, NULL);
    timer_start(t);
    
    /* Advance 50 ticks */
    mock_ticks = 50;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(0, callback_count);
    
    /* Restart timer (should reset expiry to 50 + 100 = 150) */
    timer_start(t);
    
    /* Advance to 100 (original expiry) */
    mock_ticks = 100;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(0, callback_count); /* Should not fire yet */
    
    /* Advance to 150 */
    mock_ticks = 150;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(1, callback_count);
    
    timer_delete(t);
}

/* Verify multiple timers expiring */
void test_timer_multiple_expiry(void) {
    sw_timer_t *t1 = timer_create("t1", 100, 0, timer_cb, NULL);
    sw_timer_t *t2 = timer_create("t2", 100, 0, timer_cb, NULL);
    
    timer_start(t1);
    timer_start(t2);
    
    mock_ticks = 100;
    timer_check_expiries();
    
    TEST_ASSERT_EQUAL(2, callback_count);
    
    timer_delete(t1);
    timer_delete(t2);
}

/* Verify timers fire in correct order regardless of start order */
void test_timer_ordering(void) {
    sw_timer_t *t1 = timer_create("t1", 30, 0, timer_cb, (void*)1);
    sw_timer_t *t2 = timer_create("t2", 10, 0, timer_cb, (void*)2);
    sw_timer_t *t3 = timer_create("t3", 20, 0, timer_cb, (void*)3);
    
    /* Start in reverse order of expiry */
    timer_start(t1); // Expire at 30
    timer_start(t3); // Expire at 20
    timer_start(t2); // Expire at 10
    
    /* Tick 10. T2 should fire */
    mock_ticks = 10;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(1, callback_count);
    TEST_ASSERT_EQUAL_PTR((void*)2, callback_arg);
    
    /* Tick 20. T3 should fire */
    mock_ticks = 20;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(2, callback_count);
    TEST_ASSERT_EQUAL_PTR((void*)3, callback_arg);
    
    /* Tick 30. T1 should fire */
    mock_ticks = 30;
    timer_check_expiries();
    TEST_ASSERT_EQUAL(3, callback_count);
    TEST_ASSERT_EQUAL_PTR((void*)1, callback_arg);
    
    timer_delete(t1);
    timer_delete(t2);
    timer_delete(t3);
}

/* Verify tick counter wrap-around handling */
void test_timer_wrap_around(void) {
    /* Start near max uint32 */
    mock_ticks = UINT32_MAX - 10;
    
    /* Create timer for 20 ticks. Expiry will wrap to 9 */
    sw_timer_t *t = timer_create("wrap", 20, 0, timer_cb, NULL);
    timer_start(t);
    
    /* Advance to max (10 ticks elapsed). Should NOT fire */
    mock_ticks = UINT32_MAX;
    uint32_t next = timer_check_expiries();
    TEST_ASSERT_EQUAL(0, callback_count);
    TEST_ASSERT_EQUAL(10, next); /* 10 ticks remaining until wrap+9 */
    
    /* Wrap around to 0 (1 tick elapsed since max) */
    mock_ticks = 0;
    next = timer_check_expiries();
    TEST_ASSERT_EQUAL(0, callback_count);
    TEST_ASSERT_EQUAL(9, next);
    
    /* Advance to expiry (9) */
    mock_ticks = 9;
    next = timer_check_expiries();
    TEST_ASSERT_EQUAL(1, callback_count);
    TEST_ASSERT_EQUAL(UINT32_MAX, next); /* List empty */
    
    timer_delete(t);
}


/* Run the timer test suite */
void run_timer_tests(void) {
    printf("\n=== Starting Timer Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_timer.c");

    RUN_TEST(test_timer_create_delete);
    RUN_TEST(test_timer_getters_null);
    RUN_TEST(test_timer_one_shot_expiry);
    RUN_TEST(test_timer_periodic);
    RUN_TEST(test_timer_restart);
    RUN_TEST(test_timer_multiple_expiry);
    RUN_TEST(test_timer_ordering);
    RUN_TEST(test_timer_wrap_around);

    printf("=== Timer Tests Complete ===\n");
}
