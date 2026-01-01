#include "unity.h"
#include "scheduler.h"
#include "allocator.h"
#include "utils.h"
#include <string.h>

/* --- Mocks for Platform Dependencies --- */
static size_t mock_ticks = 0;
static int mock_yield_count = 0;
static int mock_panic_count = 0;

size_t platform_get_ticks(void) { return mock_ticks; }
void platform_yield(void) { mock_yield_count++; }
void platform_panic(void) { mock_panic_count++; }
void platform_cpu_idle(void) { }
void platform_start_scheduler(size_t stack_pointer) { (void)stack_pointer; }

/* Mock Stack Initialization: Just return the pointer as-is */
void *platform_initialize_stack(void *top_of_stack, void (*task_func)(void *), void *arg, void (*exit_handler)(void)) {
    (void)task_func; (void)arg; (void)exit_handler;
    return top_of_stack; 
}

/* --- Setup / Teardown --- */
static uint8_t heap_memory[8192];

void setUp(void) {
    mock_ticks = 0;
    mock_yield_count = 0;
    mock_panic_count = 0;
    
    /* Initialize dependencies */
    allocator_init(heap_memory, sizeof(heap_memory));
    scheduler_init();
}

void tearDown(void) {}

/* --- Helper --- */
static void dummy_task(void *arg) { (void)arg; }

/* --- Tests --- */

void test_scheduler_init_should_reset_ids(void) {
    /* Create a task to increment ID counter */
    task_create(dummy_task, NULL, 512);
    
    /* Re-init scheduler */
    scheduler_init();
    
    /* Next task should have ID 1 again */
    int32_t id = task_create(dummy_task, NULL, 512);
    TEST_ASSERT_EQUAL(1, id);
}

void test_task_create_should_allocate_stack(void) {
    size_t free_before = allocator_get_free_size();
    
    int32_t id = task_create(dummy_task, NULL, 512);
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
    task_create(dummy_task, NULL, 512);
    scheduler_start();
    
    /* Simulate calling sleep from the running task */
    int res = task_sleep_ticks(100);
    
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL(1, mock_yield_count); /* Should trigger context switch */
    
    task_t *t = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t));
}

void test_scheduler_wake_sleeping_tasks(void) {
    task_create(dummy_task, NULL, 512);
    scheduler_start();
    task_sleep_ticks(100);
    
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
    task_create(dummy_task, NULL, 512);
    task_create(dummy_task, NULL, 512);
    
    scheduler_start();
    
    void *t1 = task_get_current();
    schedule_next_task(); /* Force switch */
    void *t2 = task_get_current();
    
    TEST_ASSERT_NOT_EQUAL(t1, t2);
}

void test_task_delete_should_free_memory_after_gc(void) {
    int32_t id = task_create(dummy_task, NULL, 512);
    size_t free_after_create = allocator_get_free_size();
    
    task_delete((uint16_t)id);
    
    /* Task is ZOMBIE now. Trigger Garbage Collection manually. */
    task_garbage_collection();
    
    size_t free_after_gc = allocator_get_free_size();
    TEST_ASSERT_TRUE(free_after_gc > free_after_create);
}

void run_scheduler_tests(void) {
    UnitySetTestFile("tests/test_scheduler.c");
    RUN_TEST(test_scheduler_init_should_reset_ids);
    RUN_TEST(test_task_create_should_allocate_stack);
    RUN_TEST(test_scheduler_start_should_create_idle_task);
    RUN_TEST(test_task_sleep_should_block_and_yield);
    RUN_TEST(test_scheduler_wake_sleeping_tasks);
    RUN_TEST(test_round_robin_switching);
    RUN_TEST(test_task_delete_should_free_memory_after_gc);
}