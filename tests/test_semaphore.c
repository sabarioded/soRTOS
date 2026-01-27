#include "unity.h"
#include "semaphore.h"
#include "scheduler.h"
#include <string.h>
#include <setjmp.h>
#include "allocator.h"
#include <stdio.h>
#include "test_common.h"

static uint8_t heap[4096];
static task_t *t1;
static task_t *t2;

static void dummy_task(void *arg) { 
    (void)arg; 
}

static void setUp_local(void) {
    allocator_init(heap, sizeof(heap));
    scheduler_init();
    
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL); /* ID 1 */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL); /* ID 2 */
    t1 = scheduler_get_task_by_index(1);
    t2 = scheduler_get_task_by_index(2);
    
    task_set_current(t1);
    mock_yield_count = 0;
}

static void tearDown_local(void) {

}

/* Verify semaphore initialization */
void test_sem_init(void) {
    so_sem_t s;
    so_sem_init(&s, 5, 10);
    TEST_ASSERT_EQUAL(5, s.count);
    TEST_ASSERT_EQUAL(10, s.max_count);
    TEST_ASSERT_NULL(s.wait_head);
}

/* Verify wait decrements count and doesn't block if available */
void test_sem_wait_decrements_count(void) {
    so_sem_t s;
    so_sem_init(&s, 1, 1);
    
    so_sem_wait(&s);
    TEST_ASSERT_EQUAL(0, s.count);
    TEST_ASSERT_EQUAL(0, mock_yield_count);
}

/* Verify wait blocks if count is 0 */
void test_sem_wait_blocks_when_empty(void) {
    so_sem_t s;
    so_sem_init(&s, 0, 1);
    
    /* Expect sem_wait to block and call platform_yield */
    if (setjmp(yield_jump) == 0) {
        so_sem_wait(&s);
        TEST_FAIL_MESSAGE("sem_wait should have yielded/blocked");
    }
    
    TEST_ASSERT_EQUAL(1, mock_yield_count);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t1));
    TEST_ASSERT_NOT_NULL(s.wait_head);
}

/* Verify signal increments count */
void test_sem_signal_increments_count(void) {
    so_sem_t s;
    so_sem_init(&s, 0, 5);
    
    so_sem_signal(&s);
    TEST_ASSERT_EQUAL(1, s.count);
}

/* Verify signal wakes up a waiting task */
void test_sem_signal_wakes_task(void) {
    so_sem_t s;
    so_sem_init(&s, 0, 5);
    
    /* Manually queue a task */
    wait_node_t *node = task_get_wait_node(t2);
    node->task = t2;
    node->next = NULL;
    s.wait_head = node;
    s.wait_tail = node;
    
    task_set_state(t2, TASK_BLOCKED);
    
    so_sem_signal(&s);
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2));
    TEST_ASSERT_NULL(s.wait_head);
    TEST_ASSERT_EQUAL(1, s.count); /* Count increments */
}

/* Verify broadcast wakes all waiting tasks */
void test_sem_broadcast_wakes_all(void) {
    so_sem_t s;
    so_sem_init(&s, 0, 5);
    
    /* Queue 2 tasks */
    wait_node_t *n1 = task_get_wait_node(t1);
    wait_node_t *n2 = task_get_wait_node(t2);
    
    n1->task = t1; n1->next = n2;
    n2->task = t2; n2->next = NULL;
    
    s.wait_head = n1;
    s.wait_tail = n2;
    
    task_set_state(t1, TASK_BLOCKED);
    task_set_state(t2, TASK_BLOCKED);
    
    so_sem_broadcast(&s);
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2));
    TEST_ASSERT_NULL(s.wait_head);
    TEST_ASSERT_EQUAL(2, s.count); /* Should increment for each woken task */
}

void run_semaphore_tests(void) {
    printf("\n=== Starting Semaphore Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_semaphore.c");

    RUN_TEST(test_sem_init);
    RUN_TEST(test_sem_wait_decrements_count);
    RUN_TEST(test_sem_wait_blocks_when_empty);
    RUN_TEST(test_sem_signal_increments_count);
    RUN_TEST(test_sem_signal_wakes_task);
    RUN_TEST(test_sem_broadcast_wakes_all);
    
    printf("=== Semaphore Tests Complete ===\n");
}