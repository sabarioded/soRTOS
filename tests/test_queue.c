#include "unity.h"
#include "queue.h"
#include "allocator.h"
#include "scheduler.h"
#include "platform.h"
#include <setjmp.h>
#include "spinlock.h"
#include <stdio.h>
#include "test_common.h"


static queue_t *q;
static uint8_t heap[4096];

static void dummy_task(void *arg) {
    (void)arg;
    while(1) {
        platform_yield();
    } 
}

static void setUp_queue(void) {
    mock_yield_count = 0;
    // Initialize allocator and scheduler
    allocator_init(heap, sizeof(heap));
    scheduler_init();
    task_create(dummy_task, NULL, 512, 1);
    scheduler_start(); // Sets task_current
    q = queue_create(sizeof(int), 5);
}

static void tearDown_queue(void) {
    queue_delete(q);
}

void test_queue_creation(void) {
    TEST_ASSERT_NOT_NULL(q);
}

void test_queue_send_receive(void) {
    int tx_val = 42;
    int rx_val = 0;

    /* Send should succeed */
    int res = queue_push_from_isr(q, &tx_val);
    TEST_ASSERT_EQUAL(0, res);

    /* Receive should succeed */
    res = queue_pop_from_isr(q, &rx_val);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL(42, rx_val);
}

void test_queue_full(void) {
    int val = 10;
    /* Fill the queue */
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(0, queue_push_from_isr(q, &val));
    }

    /* Next send should fail (using ISR version to avoid blocking loop in test) */
    TEST_ASSERT_EQUAL(-1, queue_push_from_isr(q, &val));
}

void test_queue_empty(void) {
    int val;
    /* Receive from empty queue should fail */
    TEST_ASSERT_EQUAL(-1, queue_pop_from_isr(q, &val));
}

void test_queue_peek(void) {
    int tx_val = 99;
    int rx_val = 0;

    queue_push_from_isr(q, &tx_val);

    /* Peek should get value but not remove it */
    TEST_ASSERT_EQUAL(0, queue_peek(q, &rx_val));
    TEST_ASSERT_EQUAL(99, rx_val);

    /* Receive should still get it */
    TEST_ASSERT_EQUAL(0, queue_pop_from_isr(q, &rx_val));
    TEST_ASSERT_EQUAL(99, rx_val);
}

void test_queue_send_blocking(void) {
    int val = 10;
    /* Fill the queue */
    for (int i = 0; i < 5; i++) {
        queue_push_from_isr(q, &val);
    }

    /* Next send should block */
    if (setjmp(yield_jump) == 0) {
        queue_push(q, &val);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    TEST_ASSERT_EQUAL(1, mock_yield_count);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic((task_t*)task_get_current()));
}

void test_queue_receive_blocking(void) {
    int val;
    /* Receive from empty queue should block */
    if (setjmp(yield_jump) == 0) {
        queue_pop(q, &val);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    TEST_ASSERT_EQUAL(1, mock_yield_count);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic((task_t*)task_get_current()));
}

static int cb_count = 0;
static void my_queue_cb(void *arg) {
    (void)arg;
    cb_count++;
}

void test_queue_callback(void) {
    cb_count = 0;
    queue_set_push_callback(q, my_queue_cb, NULL);
    
    int val = 1;
    queue_push_from_isr(q, &val);
    TEST_ASSERT_EQUAL(1, cb_count);
}

void test_queue_reset(void) {
    int val = 10;
    for (int i = 0; i < 5; i++) {
        queue_push_from_isr(q, &val);
    }
    
    queue_reset(q);
    
    /* Should be empty now */
    TEST_ASSERT_EQUAL(-1, queue_pop_from_isr(q, &val));
}

void test_queue_push_arr(void) {
    int vals[] = {100, 200, 300};
    
    /* Push array of 3 items */
    TEST_ASSERT_EQUAL(0, queue_push_arr(q, vals, 3));
    
    int rx_val;
    TEST_ASSERT_EQUAL(0, queue_pop(q, &rx_val));
    TEST_ASSERT_EQUAL(100, rx_val);
    
    TEST_ASSERT_EQUAL(0, queue_pop(q, &rx_val));
    TEST_ASSERT_EQUAL(200, rx_val);
    
    TEST_ASSERT_EQUAL(0, queue_pop(q, &rx_val));
    TEST_ASSERT_EQUAL(300, rx_val);
}

void run_queue_tests(void) {
    UnitySetTestFile("tests/test_queue.c");
    test_setUp_hook = setUp_queue;
    test_tearDown_hook = tearDown_queue;

    printf("\n=== Starting Queue Tests ===\n");
    RUN_TEST(test_queue_creation);
    RUN_TEST(test_queue_send_receive);
    RUN_TEST(test_queue_full);
    RUN_TEST(test_queue_empty);
    RUN_TEST(test_queue_peek);
    RUN_TEST(test_queue_send_blocking);
    RUN_TEST(test_queue_receive_blocking);
    RUN_TEST(test_queue_callback);
    RUN_TEST(test_queue_reset);
    RUN_TEST(test_queue_push_arr);
    printf("=== Queue Tests Complete ===\n");
}