#include "unity.h"
#include "semaphore.h"
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
static jmp_buf yield_jump;

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
    longjmp(yield_jump, 1);
}

/* --- Setup --- */
void sem_setUp(void) {
    memset(tasks, 0, sizeof(tasks));
    tasks[0].id = 1; tasks[0].state = TASK_RUNNING;
    tasks[1].id = 2; tasks[1].state = TASK_BLOCKED;
    current_task_ptr = &tasks[0];
    yield_count = 0;
}

/* --- Tests --- */
void test_sem_init(void) {
    sem_t s;
    sem_init(&s, 5, 10);
    TEST_ASSERT_EQUAL(5, s.count);
    TEST_ASSERT_EQUAL(10, s.max_count);
    TEST_ASSERT_EQUAL(0, s.q_count);
}

void test_sem_wait_decrements_count(void) {
    sem_t s;
    sem_init(&s, 1, 1);
    
    sem_wait(&s);
    TEST_ASSERT_EQUAL(0, s.count);
    TEST_ASSERT_EQUAL(0, yield_count);
}

void test_sem_wait_blocks_when_empty(void) {
    sem_t s;
    sem_init(&s, 0, 1);
    
    /* Expect sem_wait to block and call platform_yield */
    if (setjmp(yield_jump) == 0) {
        sem_wait(&s);
        TEST_FAIL_MESSAGE("sem_wait should have yielded/blocked");
    }
    
    TEST_ASSERT_EQUAL(1, yield_count);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, tasks[0].state);
    TEST_ASSERT_EQUAL(1, s.q_count);
}

void test_sem_signal_increments_count(void) {
    sem_t s;
    sem_init(&s, 0, 5);
    
    sem_signal(&s);
    TEST_ASSERT_EQUAL(1, s.count);
}

void test_sem_signal_wakes_task(void) {
    sem_t s;
    sem_init(&s, 0, 5);
    
    /* Manually queue a task */
    s.wait_queue[0] = &tasks[1];
    s.tail = 1;
    s.q_count = 1;
    
    sem_signal(&s);
    
    TEST_ASSERT_EQUAL(TASK_READY, tasks[1].state);
    TEST_ASSERT_EQUAL(0, s.q_count);
    TEST_ASSERT_EQUAL(0, s.count); /* Count shouldn't inc if we woke someone */
}

void test_sem_broadcast_wakes_all(void) {
    sem_t s;
    sem_init(&s, 0, 5);
    
    /* Queue 2 tasks */
    s.wait_queue[0] = &tasks[1];
    s.wait_queue[1] = &tasks[2];
    s.tail = 2;
    s.q_count = 2;
    tasks[1].state = TASK_BLOCKED;
    tasks[2].state = TASK_BLOCKED;
    
    sem_broadcast(&s);
    
    TEST_ASSERT_EQUAL(TASK_READY, tasks[1].state);
    TEST_ASSERT_EQUAL(TASK_READY, tasks[2].state);
    TEST_ASSERT_EQUAL(0, s.q_count);
    TEST_ASSERT_EQUAL(2, s.count); /* Should increment for each woken task */
}

void run_semaphore_tests(void) {
    UnitySetTestFile("tests/test_semaphore.c");
    RUN_TEST(test_sem_init);
    RUN_TEST(test_sem_wait_decrements_count);
    RUN_TEST(test_sem_wait_blocks_when_empty);
    RUN_TEST(test_sem_signal_increments_count);
    RUN_TEST(test_sem_signal_wakes_task);
    RUN_TEST(test_sem_broadcast_wakes_all);
}