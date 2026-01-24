#include "unity.h"
#include "mutex.h"
#include "scheduler.h"
#include <string.h>
#include <setjmp.h>
#include "allocator.h"
#include <stdio.h>
#include "test_common.h"

/* Access global scheduler state (linked from scheduler.c) */
extern task_t *task_current;

static uint8_t heap[4096];
static task_t *t1;
static task_t *t2;
static void dummy_task(void *arg) { (void)arg; }

/* --- Setup / Teardown --- */

static void setUp_local(void) {
    allocator_init(heap, sizeof(heap));
    scheduler_init();
    
    /* Create real tasks using the scheduler */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL); /* ID 1 */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL); /* ID 2 */
    
    t1 = scheduler_get_task_by_index(1);
    t2 = scheduler_get_task_by_index(2);
    
    /* Manually set current task to T1 */
    task_current = t1;
    mock_yield_count = 0;
}

static void tearDown_local(void) {}

/* --- Tests --- */

void test_mutex_init(void) {
    mutex_t m;
    mutex_init(&m);
    
    TEST_ASSERT_EQUAL(0, m.locked);
    TEST_ASSERT_NULL(m.owner);
    TEST_ASSERT_EQUAL(0, m.count);
    TEST_ASSERT_EQUAL(0, m.head);
    TEST_ASSERT_EQUAL(0, m.tail);
}

void test_mutex_lock_success(void) {
    mutex_t m;
    mutex_init(&m);
    
    mutex_lock(&m);
    
    TEST_ASSERT_EQUAL(1, m.locked);
    TEST_ASSERT_EQUAL_PTR(t1, m.owner);
    TEST_ASSERT_EQUAL(0, mock_yield_count); /* Should not yield */
}

void test_mutex_lock_recursive_handoff(void) {
    mutex_t m;
    mutex_init(&m);
    
    mutex_lock(&m);
    /* Lock again (Recursive check) */
    mutex_lock(&m);
    
    TEST_ASSERT_EQUAL(1, m.locked);
    TEST_ASSERT_EQUAL_PTR(t1, m.owner);
    TEST_ASSERT_EQUAL(0, mock_yield_count);
}

void test_mutex_contention_blocking(void) {
    mutex_t m;
    mutex_init(&m);
    
    /* Task 0 takes lock */
    mutex_lock(&m);
    
    /* Switch context to Task 1 */
    task_current = t2;
    
    /* Task 1 tries to lock. Should block and yield. */
    if (setjmp(yield_jump) == 0) {
        mutex_lock(&m); 
        TEST_FAIL_MESSAGE("mutex_lock should have yielded/blocked");
    }
    
    /* Verify Task 1 was queued and blocked */
    TEST_ASSERT_EQUAL(1, m.count);
    /* Check the queue (tail points to next free, so check tail-1 or 0) */
    TEST_ASSERT_EQUAL_PTR(t2, m.wait_queue[0]);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t2));
    TEST_ASSERT_EQUAL(1, mock_yield_count);
}

void test_mutex_unlock_handoff(void) {
    mutex_t m;
    mutex_init(&m);
    
    /* Setup: Task 0 owns lock, Task 1 is waiting */
    m.locked = 1;
    m.owner = t1;
    m.wait_queue[0] = t2;
    m.head = 0;
    m.tail = 1;
    m.count = 1;
    task_set_state(t2, TASK_BLOCKED);
    
    /* Task 0 unlocks */
    mutex_unlock(&m);
    
    /* Verify Handoff */
    TEST_ASSERT_EQUAL(1, m.locked); /* Still locked (passed to T1) */
    TEST_ASSERT_EQUAL_PTR(t2, m.owner); /* Owner is now T1 */
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2)); /* T1 woke up */
    TEST_ASSERT_EQUAL(0, m.count); /* Queue empty */
}

void run_mutex_tests(void) {
    printf("\n=== Starting Mutex Tests ===\n");
    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_mutex.c");
    RUN_TEST(test_mutex_init);
    RUN_TEST(test_mutex_lock_success);
    RUN_TEST(test_mutex_lock_recursive_handoff);
    RUN_TEST(test_mutex_contention_blocking);
    RUN_TEST(test_mutex_unlock_handoff);
    printf("=== Mutex Tests Complete ===\n");
}