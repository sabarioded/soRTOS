#include "unity.h"
#include "logger.h"
#include "queue.h"
#include "allocator.h"
#include "scheduler.h"
#include "platform.h"
#include <string.h>
#include <stdio.h>
#include "test_common.h"

static uint8_t heap[8192];

static void setUp_local(void) {
    mock_ticks = 0;
    allocator_init(heap, sizeof(heap));
    scheduler_init();
    logger_init();
    
    /* Drain any logs generated during initialization (e.g. "Scheduler Init", "Task Create") */
    queue_t *q = logger_get_queue();
    if (q) {
        log_entry_t dummy;
        while(queue_receive_from_isr(q, &dummy) == 0);
    }
}

static void tearDown_local(void) {
    /* 
     * since we re-init the allocator and scheduler in setUp, 
     * the memory is effectively reset for the next test.
     */
}

void test_logger_init_creates_queue_and_task(void) {
    queue_t *q = logger_get_queue();
    TEST_ASSERT_NOT_NULL(q);
    
    /* logger task should be first task here */
    task_t *t = scheduler_get_task_by_index(0);
    TEST_ASSERT_NOT_NULL(t);
}

void test_logger_log_pushes_to_queue(void) {
    queue_t *q = logger_get_queue();
    
    mock_ticks = 1234;
    const char *msg = "Test Message";
    
    logger_log(msg, 10, 20);
    
    /* Verify queue has 1 item */
    log_entry_t entry;
    TEST_ASSERT_EQUAL(0, queue_receive_from_isr(q, &entry));
    
    TEST_ASSERT_EQUAL(1234, entry.timestamp);
    TEST_ASSERT_EQUAL_STRING(msg, entry.fmt);
    TEST_ASSERT_EQUAL(10, entry.arg1);
    TEST_ASSERT_EQUAL(20, entry.arg2);
}

void test_logger_log_fifo_order(void) {
    queue_t *q = logger_get_queue();
    
    logger_log("First", 1, 1);
    logger_log("Second", 2, 2);
    
    log_entry_t entry;
    
    /* Pop First */
    TEST_ASSERT_EQUAL(0, queue_receive_from_isr(q, &entry));
    TEST_ASSERT_EQUAL_STRING("First", entry.fmt);
    
    /* Pop Second */
    TEST_ASSERT_EQUAL(0, queue_receive_from_isr(q, &entry));
    TEST_ASSERT_EQUAL_STRING("Second", entry.fmt);
}

void test_logger_log_drops_when_full_non_blocking(void) {
    queue_t *q = logger_get_queue();
    
    /* Fill the queue */
    for (int i = 0; i < LOG_QUEUE_SIZE; i++) {
        logger_log("Fill", i, 0);
    }
    
    /* Try to log one more */
    logger_log("Overflow", 99, 99);
    
    /* 
     * Should not have crashed or blocked. 
     * The queue should still contain the first LOG_QUEUE_SIZE items. 
     * The "Overflow" item should be dropped. 
     */
       
    log_entry_t entry;
    TEST_ASSERT_EQUAL(0, queue_receive_from_isr(q, &entry));
    TEST_ASSERT_EQUAL_STRING("Fill", entry.fmt);
    TEST_ASSERT_EQUAL(0, entry.arg1);
}

void run_logger_tests(void) {
    printf("\n=== Starting Logger Tests ===\n");
    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_logger.c");
    
    RUN_TEST(test_logger_init_creates_queue_and_task);
    RUN_TEST(test_logger_log_pushes_to_queue);
    RUN_TEST(test_logger_log_fifo_order);
    RUN_TEST(test_logger_log_drops_when_full_non_blocking);
    
    printf("=== Logger Tests Complete ===\n");
}