#include "unity.h"
#include "queue.h"
#include "scheduler.h"
#include "allocator.h"
#include <string.h>
#include <setjmp.h>

/* Access shared mocks */
extern jmp_buf yield_jump;
extern int mock_yield_count;
extern task_t *task_current;

static uint8_t heap[4096];
static task_t *t1;
static task_t *t2;
static void dummy_task(void *arg) { (void)arg; }

extern void (*test_setUp_hook)(void);
extern void (*test_tearDown_hook)(void);

/* --- Setup --- */
static void setUp_local(void) {
    allocator_init(heap, sizeof(heap));
    scheduler_init();
    
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL); /* ID 1 */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL); /* ID 2 */
    t1 = scheduler_get_task_by_index(1);
    t2 = scheduler_get_task_by_index(2);
    
    task_current = t1;
    mock_yield_count = 0;
}

static void tearDown_local(void) {}

/* --- Tests --- */

void test_queue_create_delete(void) {
    queue_t *q = queue_create(sizeof(int), 5);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_NOT_NULL(q->buffer);
    TEST_ASSERT_EQUAL(5, q->capacity);
    TEST_ASSERT_EQUAL(sizeof(int), q->item_size);
    
    queue_delete(q);
}

void test_queue_send_receive_basic(void) {
    queue_t *q = queue_create(sizeof(int), 5);
    int tx_val = 42;
    int rx_val = 0;
    
    TEST_ASSERT_EQUAL(0, queue_send(q, &tx_val));
    TEST_ASSERT_EQUAL(1, q->count);
    
    TEST_ASSERT_EQUAL(0, queue_receive(q, &rx_val));
    TEST_ASSERT_EQUAL(42, rx_val);
    TEST_ASSERT_EQUAL(0, q->count);
    
    queue_delete(q);
}

void test_queue_receive_blocks_when_empty(void) {
    queue_t *q = queue_create(sizeof(int), 5);
    int rx_val = 0;
    
    if (setjmp(yield_jump) == 0) {
        queue_receive(q, &rx_val);
        TEST_FAIL_MESSAGE("Should have blocked");
    }
    
    TEST_ASSERT_EQUAL(1, mock_yield_count);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t1));
    TEST_ASSERT_EQUAL(1, q->rx_count);
    
    queue_delete(q);
}

void test_queue_send_blocks_when_full(void) {
    queue_t *q = queue_create(sizeof(int), 2);
    int val = 10;
    
    /* Fill queue */
    queue_send(q, &val);
    queue_send(q, &val);
    TEST_ASSERT_EQUAL(2, q->count);
    
    /* Try sending 3rd item */
    if (setjmp(yield_jump) == 0) {
        queue_send(q, &val);
        TEST_FAIL_MESSAGE("Should have blocked");
    }
    
    TEST_ASSERT_EQUAL(1, mock_yield_count);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t1));
    TEST_ASSERT_EQUAL(1, q->tx_count);
    
    queue_delete(q);
}

void test_queue_send_from_isr_non_blocking(void) {
    queue_t *q = queue_create(sizeof(int), 1);
    int val = 10;
    
    /* Fill queue */
    TEST_ASSERT_EQUAL(0, queue_send_from_isr(q, &val));
    
    /* Try sending again - should fail immediately, NOT block */
    int res = queue_send_from_isr(q, &val);
    TEST_ASSERT_EQUAL(-1, res);
    TEST_ASSERT_EQUAL(0, mock_yield_count); /* Should not have yielded */
    
    queue_delete(q);
}

void test_queue_send_from_isr_wakes_receiver(void) {
    queue_t *q = queue_create(sizeof(int), 1);
    int val = 10;
    
    /* Manually queue a blocked receiver to simulate a waiting task */
    q->rx_wait_queue[0] = t2;
    q->rx_tail = 1;
    q->rx_count = 1;
    task_set_state(t2, TASK_BLOCKED);
    
    /* Send from ISR */
    TEST_ASSERT_EQUAL(0, queue_send_from_isr(q, &val));
    
    /* Verify task was unblocked */
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2));
    TEST_ASSERT_EQUAL(0, q->rx_count);
    
    queue_delete(q);
}

void test_queue_peek_does_not_remove(void) {
    queue_t *q = queue_create(sizeof(int), 5);
    int tx_val = 99;
    int peek_val = 0;
    
    /* Peek empty queue should fail */
    TEST_ASSERT_EQUAL(-1, queue_peek(q, &peek_val));
    
    queue_send(q, &tx_val);
    
    /* Peek should succeed and return value */
    TEST_ASSERT_EQUAL(0, queue_peek(q, &peek_val));
    TEST_ASSERT_EQUAL(99, peek_val);
    
    /* Count should remain 1 */
    TEST_ASSERT_EQUAL(1, q->count);
    
    /* Receive should still get the value */
    int rx_val = 0;
    queue_receive(q, &rx_val);
    TEST_ASSERT_EQUAL(99, rx_val);
    
    queue_delete(q);
}

void test_queue_peek_full(void) {
    queue_t *q = queue_create(sizeof(int), 2);
    int val1 = 10, val2 = 20, peek_val = 0;
    
    queue_send(q, &val1);
    queue_send(q, &val2);
    
    TEST_ASSERT_EQUAL(0, queue_peek(q, &peek_val));
    TEST_ASSERT_EQUAL(10, peek_val);
    
    queue_delete(q);
}

void test_queue_reset_clears_and_wakes_senders(void) {
    queue_t *q = queue_create(sizeof(int), 2);
    int val = 10;
    
    /* Fill queue */
    queue_send(q, &val);
    queue_send(q, &val);
    TEST_ASSERT_EQUAL(2, q->count);

    /* Mock a blocked sender */
    q->tx_wait_queue[0] = t2;
    q->tx_tail = 1;
    q->tx_count = 1;
    task_set_state(t2, TASK_BLOCKED);

    queue_reset(q);

    TEST_ASSERT_EQUAL(0, q->count);
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2));
    TEST_ASSERT_EQUAL(0, q->tx_count);

    queue_delete(q);
}

void run_queue_tests(void) {
    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_queue.c");
    RUN_TEST(test_queue_create_delete);
    RUN_TEST(test_queue_send_receive_basic);
    RUN_TEST(test_queue_receive_blocks_when_empty);
    RUN_TEST(test_queue_send_blocks_when_full);
    RUN_TEST(test_queue_send_from_isr_non_blocking);
    RUN_TEST(test_queue_send_from_isr_wakes_receiver);
    RUN_TEST(test_queue_peek_does_not_remove);
    RUN_TEST(test_queue_peek_full);
    RUN_TEST(test_queue_reset_clears_and_wakes_senders);
}