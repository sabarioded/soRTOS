#include "unity.h"
#include "event_group.h"
#include "scheduler.h"
#include "allocator.h"
#include "test_common.h"
#include <setjmp.h>

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
    mock_ticks = 0;
}

static void tearDown_local(void) {

}

void test_event_group_init(void) {
    event_group_t *eg = event_group_create();
    TEST_ASSERT_NOT_NULL(eg);
    TEST_ASSERT_EQUAL(0, event_group_get_bits(eg));
    event_group_delete(eg);
}

void test_event_group_set_get_bits(void) {
    event_group_t *eg = event_group_create();
    
    event_group_set_bits(eg, 0x05);
    TEST_ASSERT_EQUAL(0x05, event_group_get_bits(eg));
    
    event_group_clear_bits(eg, 0x01);
    TEST_ASSERT_EQUAL(0x04, event_group_get_bits(eg));
    
    event_group_delete(eg);
}

void test_event_group_wait_no_block(void) {
    event_group_t *eg = event_group_create();
    event_group_set_bits(eg, 0x01);
    
    /* Should return immediately */
    uint32_t bits = event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY, 100);
    TEST_ASSERT_EQUAL(0x01, bits);
    TEST_ASSERT_EQUAL(0, mock_yield_count);
    
    event_group_delete(eg);
}

void test_event_group_wait_block_and_wake(void) {
    event_group_t *eg = event_group_create();
    
    /* T1 waits for bit 0x02 */
    if (setjmp(yield_jump) == 0) {
        event_group_wait_bits(eg, 0x02, EVENT_WAIT_ANY, 100);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    TEST_ASSERT_EQUAL(1, mock_yield_count);
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t1));
    
    /* Set bit 0x02 */
    event_group_set_bits(eg, 0x02);
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
    
    event_group_delete(eg);
}

void test_event_group_wait_all(void) {
    event_group_t *eg = event_group_create();
    event_group_set_bits(eg, 0x01);
    
    /* T1 waits for 0x01 AND 0x02 */
    if (setjmp(yield_jump) == 0) {
        event_group_wait_bits(eg, 0x03, EVENT_WAIT_ALL, 100);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    /* Set 0x02 (0x01 already set) */
    event_group_set_bits(eg, 0x02);
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
    
    event_group_delete(eg);
}

void test_event_group_clear_on_exit(void) {
    event_group_t *eg = event_group_create();
    event_group_set_bits(eg, 0x01);
    
    /* Wait and clear */
    uint32_t bits = event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY | EVENT_CLEAR_ON_EXIT, 100);
    
    TEST_ASSERT_EQUAL(0x01, bits);
    TEST_ASSERT_EQUAL(0, event_group_get_bits(eg));
    
    event_group_delete(eg);
}

void test_event_group_timeout(void) {
    event_group_t *eg = event_group_create();
    
    /* Wait with timeout */
    if (setjmp(yield_jump) == 0) {
        event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY, 10);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    /* In real run, scheduler_tick wakes it. Here we just verify it blocked. */
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t1));
    
    event_group_delete(eg);
}

void test_event_group_delete_wakes_tasks(void) {
    event_group_t *eg = event_group_create();
    
    /* T1 waits forever */
    if (setjmp(yield_jump) == 0) {
        event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY, UINT32_MAX);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t1));
    
    /* Deleting the group should wake T1 */
    event_group_delete(eg);
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
}

void test_event_group_wait_timeout_zero(void) {
    event_group_t *eg = event_group_create();
    
    /* should not block */
    uint32_t bits = event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY, 0);
    
    TEST_ASSERT_EQUAL(0, bits);
    TEST_ASSERT_EQUAL(0, mock_yield_count);
    
    event_group_delete(eg);
}

void test_event_group_set_bits_from_isr(void) {
    event_group_t *eg = event_group_create();
    event_group_set_bits_from_isr(eg, 0x0A);
    TEST_ASSERT_EQUAL(0x0A, event_group_get_bits(eg));
    event_group_delete(eg);
}

void test_event_group_wait_all_incremental(void) {
    event_group_t *eg = event_group_create();
    
    /* T1 waits for 0x03 (Bit 0 and 1) */
    if (setjmp(yield_jump) == 0) {
        event_group_wait_bits(eg, 0x03, EVENT_WAIT_ALL, 100);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t1));
    
    /* Set Bit 0 - Should still be blocked */
    event_group_set_bits(eg, 0x01);
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t1));
    
    /* Set Bit 1 - Should wake */
    event_group_set_bits(eg, 0x02);
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
    
    event_group_delete(eg);
}

void test_event_group_multiple_waiters_broadcast(void) {
    event_group_t *eg = event_group_create();
    
    /* T1 waits */
    task_set_current(t1);
    if (setjmp(yield_jump) == 0) {
        event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY, 100);
    }
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t1));
    
    /* T2 waits */
    task_set_current(t2);
    if (setjmp(yield_jump) == 0) {
        event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY, 100);
    }
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t2));
    
    /* Set bits */
    event_group_set_bits(eg, 0x01);
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2));
    
    event_group_delete(eg);
}

void test_event_group_multiple_waiters_consume(void) {
    event_group_t *eg = event_group_create();
    
    /* T1 waits with CLEAR_ON_EXIT */
    task_set_current(t1);
    if (setjmp(yield_jump) == 0) {
        event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY | EVENT_CLEAR_ON_EXIT, 100);
    }
    
    /* T2 waits with CLEAR_ON_EXIT */
    task_set_current(t2);
    if (setjmp(yield_jump) == 0) {
        event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY | EVENT_CLEAR_ON_EXIT, 100);
    }
    
    /* Set bits */
    event_group_set_bits(eg, 0x01);
    
    /* T1 should wake and clear the bit. T2 should remain sleeping. */
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t2));
    TEST_ASSERT_EQUAL(0, event_group_get_bits(eg));
    
    event_group_delete(eg);
}

void run_event_group_tests(void) {
    printf("\n=== Starting Event Group Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_event_group.c");
    
    RUN_TEST(test_event_group_init);
    RUN_TEST(test_event_group_set_get_bits);
    RUN_TEST(test_event_group_wait_no_block);
    RUN_TEST(test_event_group_wait_block_and_wake);
    RUN_TEST(test_event_group_wait_all);
    RUN_TEST(test_event_group_clear_on_exit);
    RUN_TEST(test_event_group_timeout);
    RUN_TEST(test_event_group_delete_wakes_tasks);
    RUN_TEST(test_event_group_wait_timeout_zero);
    RUN_TEST(test_event_group_set_bits_from_isr);
    RUN_TEST(test_event_group_wait_all_incremental);
    RUN_TEST(test_event_group_multiple_waiters_broadcast);
    RUN_TEST(test_event_group_multiple_waiters_consume);
    
    printf("=== Event Group Tests Complete ===\n");
}
