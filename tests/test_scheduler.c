#include "unity.h"
#include "scheduler.h"
#include "allocator.h"
#include "utils.h"
#include <string.h>
#include <setjmp.h>
#include <stdio.h>
#include "platform.h"
#include <stdlib.h>
#include "test_common.h"

/* 
* In this unit test harness, this function is NEVER actually executed.
* It serves only as a valid function pointer for task_create().
*/
static void dummy_task(void *arg) {
    (void)arg; 
    while(1) {
        platform_yield();
    }
}

/* Set up and Tear down */
static uint8_t *heap_memory = NULL;

static void setUp_local(void) {
    mock_ticks = 0;
    mock_yield_count = 0;
    
    /* Initialize dependencies */
    heap_memory = malloc(65536);
    allocator_init(heap_memory, 65536);
    scheduler_init();
}

static void tearDown_local(void) {
    if (heap_memory) {
        free(heap_memory);
    }
    heap_memory = NULL;
}

/* ##### Tests ##### */

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

void test_task_create_should_fail_on_invalid_args(void) {
    /* Test NULL function pointer */
    int32_t id = task_create(NULL, NULL, 512, TASK_WEIGHT_NORMAL);
    TEST_ASSERT_EQUAL(-1, id);

    /* Test stack size too small (assuming min size > 0) */
    id = task_create(dummy_task, NULL, 0, TASK_WEIGHT_NORMAL);
    TEST_ASSERT_EQUAL(-1, id);
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
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    TEST_ASSERT_EQUAL(1, mock_yield_count); /* Should trigger context switch */
    
    task_t *t = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t));
}

void test_scheduler_wake_sleeping_tasks(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    if (setjmp(yield_jump) == 0) {
        task_sleep_ticks(100);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    task_t *t = (task_t*)task_get_current();
    
    /* Advance time partially - task should stay blocked */
    mock_ticks = 50;
    scheduler_tick();
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t));
    
    /* Advance time past wake target - task should be ready */
    mock_ticks = 100;
    scheduler_tick();
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t));
}

void test_equal_weight_switching(void) {
    /* Create 2 tasks */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    scheduler_start();
    
    void *t1 = task_get_current();
    
    /* Running task's vruntime will increase, causing a switch to the other task */
    schedule_next_task(); 
    
    void *t2 = task_get_current();
    
    TEST_ASSERT_NOT_EQUAL(t1, t2);
}

void test_weighted_scheduling_initial_pick(void) {
    /* Create High Weight Task FIRST to ensure it is at top of heap if passes are equal */
    int32_t high_id = task_create(dummy_task, NULL, 512, TASK_WEIGHT_HIGH);
    /* Create Low Weight Task */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_LOW);
    
    scheduler_start();
    
    /* Scheduler should pick the High Weight task first (lowest vruntime/insertion order) */
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
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    task_t *t = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t));
    
    /* Advance time past timeout */
    mock_ticks = 60;
    scheduler_tick();
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t));
}

void test_task_notify_simple(void) {
    int32_t id1 = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    scheduler_start();
    
    /* Task 1 waits indefinitely */
    if (setjmp(yield_jump) == 0) {
        task_notify_wait(1, UINT32_MAX);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    task_t *t1 = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t1));
    
    /* Simulate another task notifying Task 1 */
    task_notify((uint16_t)id1, 0x1234);
    
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
}

void test_stack_overflow_detection_should_kill_other_task(void) {
    /* Create current task (Index 0) */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    /* Create victim task (Index 1) */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    scheduler_start();

    /* Index 0: T1, Index 1: T2, Index 2: Idle */
    task_t *victim = scheduler_get_task_by_index(1);
    
    /* Corrupt the stack canary */
    uint32_t *stack_base = (uint32_t*)task_get_stack_ptr(victim);
    TEST_ASSERT_NOT_NULL(stack_base);  
    
    /* Invert the canary to guarantee corruption regardless of the canary value */
    *stack_base = ~(*stack_base); 
    
    /* Run check */
    task_check_stack_overflow();
    
    /* Victim should be ZOMBIE now */
    TEST_ASSERT_EQUAL(TASK_ZOMBIE, task_get_state_atomic(victim));
}

void test_preemption_on_time_slice_expiry(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    task_t *current = (task_t*)task_get_current();
    uint32_t slice = task_get_time_slice(current);
    TEST_ASSERT_GREATER_THAN(0, slice);
    
    /* Tick until 1 tick before expiry */
    for (uint32_t i = 0; i < slice - 1; i++) {
        uint32_t res = scheduler_tick();
        TEST_ASSERT_EQUAL(0, res); /* No switch yet */
    }
    
    /* Final tick to expire slice */
    uint32_t res = scheduler_tick();
    TEST_ASSERT_EQUAL(1, res); /* Should request reschedule */
}

void test_sleep_list_ordering(void) {
    /* Create two tasks */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    scheduler_start();
    
    /* Task 1 (Current) sleeps for 20 ticks */
    if (setjmp(yield_jump) == 0) {
        task_sleep_ticks(20);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    /* Task 1 yielded. Manually switch to Task 2 */
    schedule_next_task();
    task_t *t2 = (task_t*)task_get_current();
    
    /* Task 2 sleeps for 10 ticks */
    if (setjmp(yield_jump) == 0) {
        task_sleep_ticks(10);
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    /* Get handle for Task 1 */
    task_t *t1 = scheduler_get_task_by_index(0);
    
    /* Advance 10 ticks */
    mock_ticks = 10;
    scheduler_tick();
    
    /* T2 (10 ticks) should wake. T1 (20 ticks) should block. */
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2));
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t1));
    
    /* Advance to 20 ticks */
    mock_ticks = 20;
    scheduler_tick();
    
    /* T1 should wake */
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
}

void test_task_notify_should_accumulate_bits_and_pending_state(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    task_t *curr = (task_t*)task_get_current();
    uint16_t id = task_get_id(curr);
    
    /* Send notifications while task is running (not waiting) */
    task_notify(id, 0x01);
    task_notify(id, 0x02);
    
    /* Should return immediately with accumulated bits */
    uint32_t val = task_notify_wait(1, 100);
    
    TEST_ASSERT_EQUAL(0x03, val);
    /* Should not have yielded since data was pending */
    TEST_ASSERT_EQUAL(0, mock_yield_count);
}

void test_task_notify_wait_should_persist_if_not_cleared(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    task_t *curr = (task_t*)task_get_current();
    uint16_t id = task_get_id(curr);
    
    task_notify(id, 0x0F);
    
    /* Wait WITHOUT clearing (clear_on_exit = 0) */
    uint32_t val = task_notify_wait(0, 0);
    TEST_ASSERT_EQUAL(0x0F, val);
    
    /* Read again - bits should still be set */
    val = task_notify_wait(1, 0); /* Now clear them */
    TEST_ASSERT_EQUAL(0x0F, val);
    
    /* Read again - should be empty */
    val = task_notify_wait(0, 0);
    TEST_ASSERT_EQUAL(0, val);
}

void test_task_block_current_should_block_and_yield(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    if (setjmp(yield_jump) == 0) {
        task_block_current();
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    TEST_ASSERT_EQUAL(1, mock_yield_count);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic((task_t*)task_get_current()));
}

void test_task_exit_should_mark_zombie_and_yield(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    if (setjmp(yield_jump) == 0) {
        task_exit();
        TEST_FAIL_MESSAGE("Should have yielded");
    }
    
    TEST_ASSERT_EQUAL(1, mock_yield_count);
    TEST_ASSERT_EQUAL(TASK_ZOMBIE, task_get_state_atomic((task_t*)task_get_current()));
}

void test_task_block_and_unblock_apis(void) {
    /* Create two tasks */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    int32_t id2 = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    scheduler_start();
    
    task_t *t2 = scheduler_get_task_by_index(1);
    
    /* Verify it's the right task */
    TEST_ASSERT_EQUAL(id2, task_get_id(t2));
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2));
    
    task_block(t2);
    TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state_atomic(t2));
    
    task_unblock(t2);
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2));
}

void test_stress_task_churn(void) {
    /* 1. Fill the system with tasks until creation fails */
    int32_t ids[MAX_TASKS];
    int count = 0;
    
    for (int i = 0; i < MAX_TASKS; i++) {
        int32_t id = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
        if (id < 0) break;
        ids[count++] = id;
    }
    
    /* Verify we created a significant number of tasks */
    TEST_ASSERT_GREATER_THAN(2, count);
    
    /* 2. Delete every even-numbered task index to fragment the list */
    for (int i = 0; i < count; i += 2) {
        task_delete((uint16_t)ids[i]);
    }
    
    /* 3. Run GC to reclaim slots */
    task_garbage_collection();
    
    /* 4. Try to refill the holes */
    int new_count = 0;

    for (int i = 0; i < MAX_TASKS; i++) {
        int32_t id = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
        if (id < 0) break;
        new_count++;
    }
    
    /* We should have been able to create roughly count/2 new tasks */
    TEST_ASSERT_EQUAL(MAX_TASKS, count);

    int expected_deleted = count / 2;
    int tolerance = 2; /* Tolerate small differences */

    TEST_ASSERT_INT_WITHIN(tolerance, expected_deleted, new_count);
}

void test_stress_interleaved_sleep_wakeups(void) {
    /* Create 3 tasks */
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    int32_t id2 = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    int32_t id3 = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    scheduler_start();
    
    /* T1 sleeps 30 ticks */
    if (setjmp(yield_jump) == 0) {
        task_sleep_ticks(30); 
        TEST_FAIL_MESSAGE("Should have yielded"); 
    }
    
    /* Switch to T3 (Heap behavior with equal weights: T3 pops before T2) */
    schedule_next_task();
    task_t *curr = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(id3, task_get_id(curr));
    
    /* T3 sleeps 10 ticks */
    if (setjmp(yield_jump) == 0) { 
        task_sleep_ticks(10); 
        TEST_FAIL_MESSAGE("Should have yielded"); 
    }
    
    /* Switch to T2 */
    schedule_next_task();
    curr = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(id2, task_get_id(curr));
    
    /* T2 sleeps 20 ticks */
    if (setjmp(yield_jump) == 0) { 
        task_sleep_ticks(20); 
        TEST_FAIL_MESSAGE("Should have yielded"); 
    }
    
    /* Get handles */
    task_t *t1 = scheduler_get_task_by_index(0);
    task_t *t2 = scheduler_get_task_by_index(1);
    task_t *t3 = scheduler_get_task_by_index(2);
    
    /* Tick 10: T3 should wake */
    mock_ticks = 10;
    scheduler_tick();
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t3));
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t2));
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t1));
    
    /* Tick 20: T2 should wake */
    mock_ticks = 20;
    scheduler_tick();
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t2));
    TEST_ASSERT_EQUAL(TASK_SLEEPING, task_get_state_atomic(t1));
    
    /* Tick 30: T1 should wake */
    mock_ticks = 30;
    scheduler_tick();
    TEST_ASSERT_EQUAL(TASK_READY, task_get_state_atomic(t1));
}

void test_stress_vruntime_chain(void) {
    /* Create 3 equal weight tasks */
    int32_t id1 = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    int32_t id2 = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    int32_t id3 = task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    
    scheduler_start();
    
    int t1_ran = 0;
    int t2_ran = 0;
    int t3_ran = 0;
    
    /* Run for a number of switches to verify fairness */
    for (int i = 0; i < 12; i++) {
        task_t *curr = (task_t*)task_get_current();
        uint16_t id = task_get_id(curr);
        
        if (id == id1) {
            t1_ran++;
        }
        else if (id == id2) { 
            t2_ran++;
        }
        else if (id == id3) {
            t3_ran++;
        }
        
        /* Burn time slice */
        uint32_t slice = task_get_time_slice(curr);
        for(uint32_t k=0; k<slice; k++) {
            scheduler_tick();
        }
        
        schedule_next_task();
    }
    
    /* Verify everyone got a chance to run */
    TEST_ASSERT_GREATER_THAN(0, t1_ran);
    TEST_ASSERT_GREATER_THAN(0, t2_ran);
    TEST_ASSERT_GREATER_THAN(0, t3_ran);
}

void test_task_set_weight(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    task_t *curr = (task_t*)task_get_current();
    TEST_ASSERT_EQUAL(TASK_WEIGHT_NORMAL, task_get_weight(curr));
    
    task_set_weight(curr, TASK_WEIGHT_HIGH);
    TEST_ASSERT_EQUAL(TASK_WEIGHT_HIGH, task_get_weight(curr));
    
    /* Verify 0 is clamped to 1 */
    task_set_weight(curr, 0);
    TEST_ASSERT_EQUAL(1, task_get_weight(curr));
}

void test_task_cpu_usage(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    task_t *curr = (task_t*)task_get_current();
    
    /* Initial usage should be 0 */
    TEST_ASSERT_EQUAL_UINT64(0, task_get_cpu_ticks(curr));
    
    /* Advance time by 100 ticks */
    mock_ticks += 100;
    
    /* Trigger a schedule event (which updates stats for the running task) */
    schedule_next_task();
    
    /* Verify usage increased */
    TEST_ASSERT_EQUAL_UINT64(100, task_get_cpu_ticks(curr));
}

void test_task_create_static_success(void) {
    uint8_t *static_stack = malloc(STACK_SIZE_1KB + 16);
    TEST_ASSERT_NOT_NULL(static_stack);

    size_t free_before = allocator_get_free_size();
    
    /* Create task with provided stack */
    int32_t id = task_create_static(dummy_task, NULL, static_stack, STACK_SIZE_1KB + 16, TASK_WEIGHT_NORMAL);
    TEST_ASSERT_GREATER_THAN(0, id);
    
    /* Should NOT have consumed heap memory */
    size_t free_after = allocator_get_free_size();
    TEST_ASSERT_EQUAL(free_before, free_after);
    
    /* Cleanup */
    task_delete((uint16_t)id);
    task_garbage_collection();
    free(static_stack);
}

void test_task_create_static_gc_safety(void) {
    uint8_t *static_stack = malloc(STACK_SIZE_1KB + 16);
    TEST_ASSERT_NOT_NULL(static_stack);

    /* Fill stack with known pattern */
    memset(static_stack, 0xAA, STACK_SIZE_1KB + 16);
    
    int32_t id = task_create_static(dummy_task, NULL, static_stack, STACK_SIZE_1KB + 16, TASK_WEIGHT_NORMAL);
    TEST_ASSERT_GREATER_THAN(0, id);
    
    task_delete((uint16_t)id);
    
    /* If logic is correct, it will skip freeing static_stack_buffer. */
    task_garbage_collection();
    
    /* Verify heap is still healthy */
    TEST_ASSERT_EQUAL(0, allocator_check_integrity());
    
    free(static_stack);
}

void test_task_create_static_invalid_args(void) {
    uint8_t dummy_buf[16];
    TEST_ASSERT_EQUAL(-1, task_create_static(dummy_task, NULL, NULL, 1024, 1));
    TEST_ASSERT_EQUAL(-1, task_create_static(dummy_task, NULL, dummy_buf, 10, 1)); /* Too small */
}

void test_idle_task_cannot_sleep(void) {
    /* Ensure clean state */
    scheduler_init();
    
    /* Start scheduler with NO user tasks. This forces it to pick the IDLE task. */
    scheduler_start();
    
    /* Verify we are running (as the idle task) */
    TEST_ASSERT_NOT_NULL(task_get_current());
    
    /* Try to sleep. Should fail because Idle task cannot sleep. */
    int res = task_sleep_ticks(10);
    TEST_ASSERT_EQUAL(-1, res);
}

void test_task_accessors(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    task_t *t = (task_t*)task_get_current();
    
    /* Notify Value */
    task_set_notify_val(t, 0xDEADBEEF);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, task_get_notify_val(t));
    
    /* Notify State */
    task_set_notify_state(t, 1);
    TEST_ASSERT_EQUAL_UINT8(1, task_get_notify_state(t));
    task_set_notify_state(t, 0);
    TEST_ASSERT_EQUAL_UINT8(0, task_get_notify_state(t));
    
    /* Stack Size */
    TEST_ASSERT_GREATER_OR_EQUAL(512, task_get_stack_size(t));
    
    /* Stack Pointer */
    TEST_ASSERT_NOT_NULL(task_get_stack_ptr(t));
    
    /* Wait Node */
    TEST_ASSERT_NOT_NULL(task_get_wait_node(t));
}

void test_task_weight_boosting(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_LOW);
    scheduler_start();
    
    task_t *t = (task_t*)task_get_current();
    
    TEST_ASSERT_EQUAL_UINT8(TASK_WEIGHT_LOW, task_get_weight(t));
    TEST_ASSERT_EQUAL_UINT8(TASK_WEIGHT_LOW, task_get_base_weight(t));
    
    /* Boost */
    task_boost_weight(t, TASK_WEIGHT_HIGH);
    TEST_ASSERT_EQUAL_UINT8(TASK_WEIGHT_HIGH, task_get_weight(t));
    TEST_ASSERT_EQUAL_UINT8(TASK_WEIGHT_LOW, task_get_base_weight(t)); /* Base should be unchanged */
    
    /* Boost lower (should be ignored) */
    task_boost_weight(t, TASK_WEIGHT_NORMAL);
    TEST_ASSERT_EQUAL_UINT8(TASK_WEIGHT_HIGH, task_get_weight(t));
    
    /* Restore */
    task_restore_base_weight(t);
    TEST_ASSERT_EQUAL_UINT8(TASK_WEIGHT_LOW, task_get_weight(t));
}

void test_task_event_accessors(void) {
    task_create(dummy_task, NULL, 512, TASK_WEIGHT_NORMAL);
    scheduler_start();
    
    task_t *t = (task_t*)task_get_current();
    
    task_set_event_wait(t, 0x0F, 0x01);
    TEST_ASSERT_EQUAL_UINT32(0x0F, task_get_event_bits(t));
    TEST_ASSERT_EQUAL_UINT8(0x01, task_get_event_flags(t));
}

void test_task_notify_invalid_id(void) {
    /* Should not crash or hang */
    task_notify(0, 1);
    task_notify(9999, 1);

    TEST_PASS();
}

void run_scheduler_tests(void) {
    printf("\n=== Starting Scheduler Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_scheduler.c");
    RUN_TEST(test_scheduler_init_should_reset_ids);
    RUN_TEST(test_task_create_should_allocate_stack);
    RUN_TEST(test_task_create_should_fail_on_invalid_args);
    RUN_TEST(test_scheduler_start_should_create_idle_task);
    RUN_TEST(test_task_sleep_should_block_and_yield);
    RUN_TEST(test_scheduler_wake_sleeping_tasks);
    RUN_TEST(test_equal_weight_switching);
    RUN_TEST(test_weighted_scheduling_initial_pick);
    RUN_TEST(test_task_delete_should_free_memory_after_gc);
    RUN_TEST(test_task_notify_wait_timeout);
    RUN_TEST(test_task_notify_simple);
    RUN_TEST(test_stack_overflow_detection_should_kill_other_task);
    RUN_TEST(test_preemption_on_time_slice_expiry);
    RUN_TEST(test_sleep_list_ordering);
    RUN_TEST(test_task_notify_should_accumulate_bits_and_pending_state);
    RUN_TEST(test_task_notify_wait_should_persist_if_not_cleared);
    RUN_TEST(test_task_block_current_should_block_and_yield);
    RUN_TEST(test_task_exit_should_mark_zombie_and_yield);
    RUN_TEST(test_task_block_and_unblock_apis);
    RUN_TEST(test_stress_task_churn);
    RUN_TEST(test_stress_interleaved_sleep_wakeups);
    RUN_TEST(test_stress_vruntime_chain);
    RUN_TEST(test_task_set_weight);
    RUN_TEST(test_task_cpu_usage);
    RUN_TEST(test_task_create_static_success);
    RUN_TEST(test_task_create_static_gc_safety);
    RUN_TEST(test_task_create_static_invalid_args);
    RUN_TEST(test_idle_task_cannot_sleep);
    RUN_TEST(test_task_accessors);
    RUN_TEST(test_task_weight_boosting);
    RUN_TEST(test_task_event_accessors);
    RUN_TEST(test_task_notify_invalid_id);

    printf("=== Scheduler Tests Complete ===\n");
}
