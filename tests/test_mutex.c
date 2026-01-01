#include "unity.h"
#include "mutex.h"
#include "scheduler.h"
#include <string.h>
#include <setjmp.h>

/* --- Mocks & Stubs --- */

/* Define the task struct for testing (opaque in scheduler.h) */
struct task_struct {
    uint16_t id;
    task_state_t state;
};

static task_t tasks[3];
static task_t *current_task_ptr = NULL;
static int yield_count = 0;
static jmp_buf yield_jump; /* Used to break out of blocking loops */

/* Mock: Return the controlled current task */
void *task_get_current(void) {
    return current_task_ptr;
}

/* Mock: Update task state */
void task_set_state(task_t *t, task_state_t state) {
    if (t) t->state = state;
}

/* Mock: Simulate context switch */
void platform_yield(void) {
    yield_count++;
    /* In a real OS, this wouldn't return until scheduled. 
       In tests, we jump back to the test function to verify blocking behavior. */
    longjmp(yield_jump, 1);
}

/* --- Setup / Teardown --- */

void setUp(void) {
    memset(tasks, 0, sizeof(tasks));
    tasks[0].id = 1; tasks[0].state = TASK_RUNNING;
    tasks[1].id = 2; tasks[1].state = TASK_READY;
    tasks[2].id = 3; tasks[2].state = TASK_READY;
    
    current_task_ptr = &tasks[0];
    yield_count = 0;
}

void tearDown(void) {}

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
    TEST_ASSERT_EQUAL_PTR(&tasks[0], m.owner);
    TEST_ASSERT_EQUAL(0, yield_count); /* Should not yield */
}

void test_mutex_lock_recursive_handoff(void) {
    mutex_t m;
    mutex_init(&m);
    
    mutex_lock(&m);
    /* Lock again (Recursive check) */
    mutex_lock(&m);
    
    TEST_ASSERT_EQUAL(1, m.locked);
    TEST_ASSERT_EQUAL_PTR(&tasks[0], m.owner);
    TEST_ASSERT_EQUAL(0, yield_count);
}

void test_mutex_contention_blocking(void) {
    mutex_t m;
    mutex_init(&m);
    
    /* Task 0 takes lock */
    mutex_lock(&m);
    
    /* Switch context to Task 1 */
    current_task_ptr = &tasks[1];
    
    /* Task 1 tries to lock. Should block and yield. */
    if (setjmp(yield_jump) == 0) {
        mutex_lock(&m); 
        TEST_FAIL_MESSAGE("mutex_lock should have yielded/blocked");
    }
    
    /* Verify Task 1 was queued and blocked */
    TEST_ASSERT_EQUAL(1, m.count);
    /* Check the queue (tail points to next free, so check tail-1 or 0) */
    TEST_ASSERT_EQUAL_PTR(&tasks[1], m.wait_queue[0]);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, tasks[1].state);
    TEST_ASSERT_EQUAL(1, yield_count);
}

void test_mutex_unlock_handoff(void) {
    mutex_t m;
    mutex_init(&m);
    
    /* Setup: Task 0 owns lock, Task 1 is waiting */
    m.locked = 1;
    m.owner = &tasks[0];
    m.wait_queue[0] = &tasks[1];
    m.head = 0;
    m.tail = 1;
    m.count = 1;
    tasks[1].state = TASK_BLOCKED;
    
    /* Task 0 unlocks */
    mutex_unlock(&m);
    
    /* Verify Handoff */
    TEST_ASSERT_EQUAL(1, m.locked); /* Still locked (passed to T1) */
    TEST_ASSERT_EQUAL_PTR(&tasks[1], m.owner); /* Owner is now T1 */
    TEST_ASSERT_EQUAL(TASK_READY, tasks[1].state); /* T1 woke up */
    TEST_ASSERT_EQUAL(0, m.count); /* Queue empty */
}

void run_mutex_tests(void) {
    UnitySetTestFile("tests/test_mutex.c");
    RUN_TEST(test_mutex_init);
    RUN_TEST(test_mutex_lock_success);
    RUN_TEST(test_mutex_lock_recursive_handoff);
    RUN_TEST(test_mutex_contention_blocking);
    RUN_TEST(test_mutex_unlock_handoff);
}