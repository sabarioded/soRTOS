#include "unity.h"
#include "scheduler.h"
#include "allocator.h"
#include "utils.h"
#include <string.h>
#include <setjmp.h>

/* Access shared mocks */
extern size_t mock_ticks;
extern int mock_yield_count;
extern jmp_buf yield_jump;

extern task_t *task_current;
extern task_t *task_next;

extern void (*test_setUp_hook)(void);
extern void (*test_tearDown_hook)(void);

/* --- Setup / Teardown --- */
static uint8_t heap_memory[8192];

static void setUp_local(void) {
    mock_ticks = 0;
    mock_yield_count = 0;
    
    /* Initialize dependencies */
    allocator_init(heap_memory, sizeof(heap_memory));
    scheduler_init();
}

static void tearDown_local(void) {}

/* --- Helper --- */
static void dummy_task(void *arg) { (void)arg; }

/* --- Tests --- */

void test_scheduler_init_should_reset_ids(void) {
    /* Create a task to increment ID counter */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    /* Re-init scheduler */
    scheduler_init();
    
    /* Next task should have ID 1 again */
    int32_t id = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    TEST_ASSERT_EQUAL(1, id);
}

void test_task_create_should_allocate_stack(void) {
    size_t free_before = allocator_get_free_size();
    
    int32_t id = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    TEST_ASSERT_GREATER_THAN(0, id);
    
    size_t free_after = allocator_get_free_size();
    TEST_ASSERT_TRUE(free_after < free_before);
}

void test_scheduler_start_should_create_idle_task(void) {
    scheduler_start();
    /* scheduler_start sets task_current to the first task (usually Idle if no others) */
    TEST_ASSERT_NOT_NULL(task_get_current());
}

void test_task_sleep_should_block_and_yield(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    /* Simulate calling sleep from the running task */
    if (setjmp(yield_jump) == 0) {
        task_sleep_ticks(100);
    }
    
    TEST_ASSERT_EQUAL(1, mock_yield_count); /* Should trigger context switch */
    
    task_t *t = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t));
}

void test_scheduler_wake_sleeping_tasks(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    if (setjmp(yield_jump) == 0) {
        task_sleep_ticks(100);
    }
    
    task_t *t = (task_t*)task_get_current();
    
    /* Advance time partially - task should stay blocked */
    mock_ticks = 50;
    scheduler_wake_sleeping_tasks();
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t));
    
    /* Advance time past wake target - task should be ready */
    mock_ticks = 100;
    scheduler_wake_sleeping_tasks();
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t));
}

void test_round_robin_switching(void) {
    /* Create 2 tasks */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    scheduler_start();
    
    void *t1 = task_get_current();
    schedule_next_task(); /* Force switch */
    
    /* Simulate context switch (assembly usually does this) */
    task_current = task_next;
    
    void *t2 = task_get_current();
    
    TEST_ASSERT_NOT_EQUAL(t1, t2);
}

void test_stride_scheduling_fairness(void) {
    /* Create High Weight Task FIRST to ensure it is at top of heap if passes are equal */
    int32_t high_id = task_create(dummy_task, NULL, 512, TASK_WEIGHT_HIGH);
    /* Create Low Weight Task */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_LOW);
    
    scheduler_start();
    
    /* Scheduler should pick the High Weight task first (lowest pass) */
    task_t *current = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(high_id, task_get_id(current));
    TEST_ASSERT_EQUAL(TASK_WEIGHT_HIGH, task_get_weight(current));
}

void test_task_delete_should_free_memory_after_gc(void) {
    int32_t id = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    size_t free_after_create = allocator_get_free_size();
    
    task_delete((uint16_t)id);
    
    /* Task is ZOMBIE now. Trigger Garbage Collection manually. */
    task_garbage_collection();
    
    size_t free_after_gc = allocator_get_free_size();
    TEST_ASSERT_TRUE(free_after_gc > free_after_create);
}

void test_task_notify_wait_timeout(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    /* Wait with timeout */
    if (setjmp(yield_jump) == 0) {
        task_notify_wait(1, 50);
    }
    
    task_t *t = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t));
    
    /* Advance time past timeout */
    mock_ticks = 60;
    scheduler_wake_sleeping_tasks();
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t));
}

void test_task_notify_simple(void) {
    int32_t id1 = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    scheduler_start();
    
    /* Task 1 waits indefinitely */
    if (setjmp(yield_jump) == 0) {
        task_notify_wait(1, UINT32_MAX);
    }
    
    task_t *t1 = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t1));
    
    /* Simulate another task notifying Task 1 */
    task_notify((uint16_t)id1, 0x1234);
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
}

void run_scheduler_tests(void) {
    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_scheduler.c");
    RUN_TEST(test_scheduler_init_should_reset_ids);
    RUN_TEST(test_task_create_should_allocate_stack);
    RUN_TEST(test_scheduler_start_should_create_idle_task);
    RUN_TEST(test_task_sleep_should_block_and_yield);
    RUN_TEST(test_scheduler_wake_sleeping_tasks);
    RUN_TEST(test_round_robin_switching);
    RUN_TEST(test_stride_scheduling_fairness);
    RUN_TEST(test_task_delete_should_free_memory_after_gc);
    RUN_TEST(test_task_notify_wait_timeout);
    RUN_TEST(test_task_notify_simple);
}