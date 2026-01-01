#include "unity.h"
#include "timer.h"
#include "scheduler.h"
#include "allocator.h"

/* Access shared mocks */
extern size_t mock_ticks;

static int callback_count = 0;
static void *callback_arg = NULL;

static void timer_cb(void *arg) {
    callback_arg = arg;
    callback_count++;
}

static uint8_t heap[4096];

extern void (*test_setUp_hook)(void);
extern void (*test_tearDown_hook)(void);

static void setUp_local(void) {
    allocator_init(heap, sizeof(heap));
    scheduler_init();
    timer_service_init();
    mock_ticks = 0;
    callback_count = 0;
    callback_arg = NULL;
}

static void tearDown_local(void) {}

void test_timer_create_delete(void) {
    sw_timer_t *t = timer_create("test", 100, 0, timer_cb, NULL);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(0, t->active);
    timer_delete(t);
}

void test_timer_one_shot_expiry(void) {
    sw_timer_t *t = timer_create("oneshot", 100, 0, timer_cb, (void*)0xDEAD);
    timer_start(t);
    TEST_ASSERT_EQUAL(1, t->active);
    
    /* Advance time partially */
    mock_ticks = 50;
    timer_test_run_callbacks();
    TEST_ASSERT_EQUAL(0, callback_count);
    
    /* Advance to expiry */
    mock_ticks = 100;
    timer_test_run_callbacks();
    TEST_ASSERT_EQUAL(1, callback_count);
    TEST_ASSERT_EQUAL_PTR((void*)0xDEAD, callback_arg);
    TEST_ASSERT_EQUAL(0, t->active); /* Should be inactive now */
    
    /* Advance more - shouldn't fire again */
    mock_ticks = 200;
    timer_test_run_callbacks();
    TEST_ASSERT_EQUAL(1, callback_count);
    
    timer_delete(t);
}

void test_timer_periodic(void) {
    sw_timer_t *t = timer_create("periodic", 50, 1, timer_cb, NULL);
    timer_start(t);
    
    /* 1st expiry */
    mock_ticks = 50;
    timer_test_run_callbacks();
    TEST_ASSERT_EQUAL(1, callback_count);
    TEST_ASSERT_EQUAL(1, t->active); /* Should still be active */
    
    /* 2nd expiry */
    mock_ticks = 100;
    timer_test_run_callbacks();
    TEST_ASSERT_EQUAL(2, callback_count);
    
    timer_stop(t);
    timer_delete(t);
}

void test_timer_restart(void) {
    sw_timer_t *t = timer_create("restart", 100, 0, timer_cb, NULL);
    timer_start(t);
    
    /* Advance 50 ticks */
    mock_ticks = 50;
    timer_test_run_callbacks();
    TEST_ASSERT_EQUAL(0, callback_count);
    
    /* Restart timer (should reset expiry to 50 + 100 = 150) */
    timer_start(t);
    
    /* Advance to 100 (original expiry) */
    mock_ticks = 100;
    timer_test_run_callbacks();
    TEST_ASSERT_EQUAL(0, callback_count); /* Should not fire yet */
    
    /* Advance to 150 */
    mock_ticks = 150;
    timer_test_run_callbacks();
    TEST_ASSERT_EQUAL(1, callback_count);
    
    timer_delete(t);
}

void test_timer_multiple_expiry(void) {
    sw_timer_t *t1 = timer_create("t1", 100, 0, timer_cb, NULL);
    sw_timer_t *t2 = timer_create("t2", 100, 0, timer_cb, NULL);
    
    timer_start(t1);
    timer_start(t2);
    
    mock_ticks = 100;
    timer_test_run_callbacks();
    
    TEST_ASSERT_EQUAL(2, callback_count);
    
    timer_delete(t1);
    timer_delete(t2);
}

void run_timer_tests(void) {
    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_timer.c");
    RUN_TEST(test_timer_create_delete);
    RUN_TEST(test_timer_one_shot_expiry);
    RUN_TEST(test_timer_periodic);
    RUN_TEST(test_timer_restart);
    RUN_TEST(test_timer_multiple_expiry);
}