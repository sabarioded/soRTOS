#include "unity.h"
#include "mutex.h"
#include "scheduler.h"
#include <string.h>
#include <setjmp.h>
#include "allocator.h"
#include <stdio.h>
#include "test_common.h"

static uint8_t heap[4096];
static task_t *t1;
static task_t *t2;
static task_t *t3;

static void dummy_task(void *arg) { 
    (void)arg; 
}

/* Initialize allocator, scheduler, and create test tasks */
static void setUp_local(void) {
    allocator_init(heap, sizeof(heap));
    scheduler_init();
    
    /* Create real tasks using the scheduler */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL); /* ID 1 */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL); /* ID 2 */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL); /* ID 3 */
    
    t1 = scheduler_get_task_by_index(1);
    t2 = scheduler_get_task_by_index(2);
    t3 = scheduler_get_task_by_index(3);
    
    /* Manually set current task to T1 */
    task_set_current(t1);
    mock_yield_count = 0;
}

/* Cleanup resources */
static void tearDown_local(void) {

}

/* Verify mutex initialization state */
void test_mutex_init(void) {
    mutex_t m;
    mutex_init(&m);
    
    TEST_ASSERT_NULL(m.owner);
    TEST_ASSERT_NULL(m.wait_head);
    TEST_ASSERT_NULL(m.wait_tail);
}

/* Verify successful lock acquisition by a task */
void test_mutex_lock_success(void) {
    mutex_t m;
    mutex_init(&m);
    
    mutex_lock(&m);
    
    TEST_ASSERT_EQUAL_PTR(t1, m.owner);
    TEST_ASSERT_EQUAL(0, mock_yield_count); /* Should not yield */
}

/* Verify recursive locking behavior (owner stays the same) */
void test_mutex_lock_recursive_handoff(void) {
    mutex_t m;
    mutex_init(&m);
    
    mutex_lock(&m);
    /* Lock again (Recursive check) */
    mutex_lock(&m);
    
    TEST_ASSERT_EQUAL_PTR(t1, m.owner);
    TEST_ASSERT_EQUAL(0, mock_yield_count);
}

/* Verify that a second task blocks when trying to acquire a locked mutex */
void test_mutex_contention_blocking(void) {
    mutex_t m;
    mutex_init(&m);
    
    /* Task 0 takes lock */
    mutex_lock(&m);
    
    /* Switch context to Task 1 */
    task_set_current(t2);
    
    /* Task 1 tries to lock. Should block and yield. */
    if (setjmp(yield_jump) == 0) {
        mutex_lock(&m); 
        TEST_FAIL_MESSAGE("mutex_lock should have yielded/blocked");
    }
    
    /* Verify Task 1 was queued and blocked */
    TEST_ASSERT_NOT_NULL(m.wait_head);
    TEST_ASSERT_EQUAL_PTR(t2, m.wait_head->task);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t2));
    TEST_ASSERT_EQUAL(1, mock_yield_count);
}

/* Verify that unlocking hands off ownership to the waiting task */
void test_mutex_unlock_handoff(void) {
    mutex_t m;
    mutex_init(&m);
    
    /* Setup: Task 0 owns lock, Task 1 is waiting */
    m.owner = t1;
    
    wait_node_t *node = task_get_wait_node(t2);
    node->task = t2;
    node->next = NULL;
    m.wait_head = node;
    m.wait_tail = node;
    task_set_state(t2, TASK_BLOCKED);
    
    /* Task 0 unlocks */
    mutex_unlock(&m);
    
    /* Verify Handoff */
    TEST_ASSERT_EQUAL_PTR(t2, m.owner); /* Owner is now T2 */
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2)); /* T1 woke up */
    TEST_ASSERT_NULL(m.wait_head); /* Queue empty */
}

/* Verify Priority Inheritance */
void test_mutex_priority_inheritance(void) {
    mutex_t m;
    mutex_init(&m);
    
    /* Set T1 to LOW priority, T2 to HIGH priority */
    task_set_weight(t1, TASK_WEIGHT_LOW);
    task_set_weight(t2, TASK_WEIGHT_HIGH);
    
    /* T1 takes the lock */
    task_set_current(t1);
    mutex_lock(&m);
    TEST_ASSERT_EQUAL(TASK_WEIGHT_LOW, task_get_weight(t1));
    
    /* T2 tries to take lock and blocks */
    task_set_current(t2);
    if (setjmp(yield_jump) == 0) {
        mutex_lock(&m);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    /* Verify T1 inherited T2's HIGH priority */
    TEST_ASSERT_EQUAL(TASK_WEIGHT_HIGH, task_get_weight(t1));
    
    /* T1 runs again and unlocks */
    task_set_current(t1);
    mutex_unlock(&m);
    
    /* Verify T1 returned to LOW priority */
    TEST_ASSERT_EQUAL(TASK_WEIGHT_LOW, task_get_weight(t1));
    
    /* Verify T2 is now the owner (handoff) */
    TEST_ASSERT_EQUAL_PTR(t2, m.owner);
}

/* Verify Chained Priority Inheritance (New Owner Boost) */
void test_mutex_chained_priority_inheritance(void) {
    mutex_t m;
    mutex_init(&m);
    
    /* Set weights: T1 < T2 < T3 */
    task_set_weight(t1, TASK_WEIGHT_LOW);
    task_set_weight(t2, TASK_WEIGHT_NORMAL);
    task_set_weight(t3, TASK_WEIGHT_HIGH);
    
    /* 1. T1 takes lock */
    task_set_current(t1);
    mutex_lock(&m);
    
    /* 2. T2 tries to lock -> Blocks. T1 boosted to NORMAL. */
    task_set_current(t2);
    if (setjmp(yield_jump) == 0) {
        mutex_lock(&m);
        TEST_FAIL_MESSAGE("T2 should have yielded");
    }
    TEST_ASSERT_EQUAL(TASK_WEIGHT_NORMAL, task_get_weight(t1));
    
    /* 3. T3 tries to lock -> Blocks. T1 boosted to HIGH. */
    task_set_current(t3);
    if (setjmp(yield_jump) == 0) {
        mutex_lock(&m);
        TEST_FAIL_MESSAGE("T3 should have yielded");
    }
    TEST_ASSERT_EQUAL(TASK_WEIGHT_HIGH, task_get_weight(t1));
    
    /* 4. T1 unlocks. 
     * T1 restores LOW.
     * T2 (Head of queue) becomes owner.
     * T2 sees T3 (HIGH) waiting. T2 boosted to HIGH.
     */
    task_set_current(t1);
    mutex_unlock(&m);
    
    TEST_ASSERT_EQUAL(TASK_WEIGHT_LOW, task_get_weight(t1));
    TEST_ASSERT_EQUAL_PTR(t2, m.owner);
    TEST_ASSERT_EQUAL(TASK_WEIGHT_HIGH, task_get_weight(t2)); /* T2 inherited T3's weight */
}

/* Run the mutex test suite */
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
    RUN_TEST(test_mutex_priority_inheritance);
    RUN_TEST(test_mutex_chained_priority_inheritance);

    printf("=== Mutex Tests Complete ===\n");
}