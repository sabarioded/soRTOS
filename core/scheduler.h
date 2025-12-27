#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <string.h> /* for memset */
#include "device_registers.h"
#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Task Memory Usage (STM32L476RG - 96 KB SRAM1):
 * ================================================
 * 
 * STATIC ALLOCATION MODE (TASK_ALLOC_STATIC):
 * --------------------------------------------
 * - Each task TCB: ~1032 bytes
 *   - psp: 4 bytes
 *   - sleep_until_tick: 4 bytes
 *   - stack[255]: 1020 bytes (255 words * 4 bytes)
 *   - state: 1 byte
 *   - is_idle: 1 byte
 *   - task_id: 2 bytes
 * 
 * - Global task_list[58]: 58 * 1032 = ~60 KB
 * - Other globals (.data/.bss): ~4 KB
 * - Remaining for heap: ~32 KB (mostly unused)
 * - Max tasks: ~58 (limited by task_list array size)
 * 
 * DYNAMIC ALLOCATION MODE (TASK_ALLOC_DYNAMIC):
 * ----------------------------------------------
 * - Each task TCB: ~16 bytes
 *   - psp: 4 bytes
 *   - sleep_until_tick: 4 bytes
 *   - stack_ptr: 4 bytes (pointer only)
 *   - stack_size: 4 bytes
 *   - state: 1 byte
 *   - is_idle: 1 byte
 *   - task_id: 2 bytes
 * 
 * - Global task_list[58]: 58 * 16 = ~928 bytes
 * - Each task stack (heap): 1020 bytes
 * - Other globals (.data/.bss): ~4 KB
 * - Total heap available: ~92 KB
 * - Actual max tasks: min(58, 92KB/1020bytes) = min(58, 92) = 58 tasks
 * 
 * Note: Dynamic mode is more memory efficient for systems with fewer
 * active tasks, as stacks are only allocated when tasks are created.
 */
#define EXC_RETURN_THREAD_PSP       0xFFFFFFFDu   /* EXC_RETURN: return to Thread mode, use PSP */

typedef enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_ZOMBIE,
} task_state_t;

typedef enum task_return {
    TASK_DELETE_SUCCESS         = 0,
    TASK_DELETE_TASK_NOT_FOUND  = -1,
    TASK_DELETE_IS_IDLE         = -2,
    TASK_DELETE_IS_CURRENT_TASK = -3
} task_return_t;

typedef struct task_struct {
    uint32_t *psp;
    uint32_t  sleep_until_tick; /* SysTick count when task should wake (0 = not sleeping) */
#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_STATIC
    uint32_t  stack[STACK_SIZE_IN_WORDS] __attribute__((aligned(8)));  /* Embedded stack for static mode */
#else
    uint32_t *stack_ptr;  /* Pointer to dynamically allocated stack for dynamic mode */
    uint32_t  stack_size; /* Size of allocated stack in bytes */
#endif
    uint8_t   state;
    uint8_t   is_idle;          /* Flag for idle task */
    uint16_t  task_id;
} task_t;

/* Globals */
extern task_t task_list[MAX_TASKS];
extern task_t  *task_current;
extern task_t  *task_next;


/**
 * @brief Initialize the scheduler
 * 
 */
void scheduler_init(void);


/**
 * @brief Create a new task
 * 
 * @param task_func Entry function for the task
 * @param arg Argument to pass to the task function
 * @param stack_size_bytes Stack size in bytes (DYNAMIC mode only, ignored in STATIC mode)
 * @return Task ID on success, -1 on failure
 * 
 * @note In STATIC mode, stack_size_bytes is ignored (uses STACK_SIZE_BYTES)
 * @note In DYNAMIC mode, stack_size_bytes must be >= STACK_MIN_SIZE_BYTES
 */
int32_t task_create(void (*task_func)(void *), void *arg, size_t stack_size_bytes);


/**
 * @brief Start the scheduler and run the first task
 * 
 */
void scheduler_start(void);


/**
 * @brief Schedule the next task to run
 * 
 */
void schedule_next_task(void);


/**
 * @brief Block a task (prevent it from being scheduled)
 * 
 * @param task Pointer to the task to block
 */
void task_block(task_t *task);


/**
 * @brief Unblock a task (make it ready to run)
 * 
 * @param task Pointer to the task to unblock
 */
void task_unblock(task_t *task);


/**
 * @brief Block the current task
 * 
 */
void task_block_current(void);


/**
 * @brief Delete a task
 * 
 * @param task_id ID of the task to delete
 * @return TASK_DELETE_SUCCESS on success, otherwise failure
 */
int32_t task_delete(uint16_t task_id);


/**
 * @brief Check all tasks for stack overflow
 * 
 */
void task_check_stack_overflow(void);


/**
 * @brief Allows a running task to voluntarily delete itself.
 * This function marks the task for deletion and yields the CPU.
 */
void task_exit(void);


/**
 * @brief Rearrange the task list, delete unused tasks.
 * 
 */
void task_garbage_collection(void);


/**
 * @brief Sleep the current task for a specified number of SysTick ticks.
 * 
 * The task will be moved to BLOCKED state and will be automatically unblocked
 * when the SysTick counter reaches the wake-up time. During sleep, the task
 * will not be selected for execution.
 * 
 * @param ticks Number of SysTick ticks to sleep (must be > 0)
 * @return 0 on success, -1 on error (e.g., invalid task or ticks = 0)
 */
int task_sleep_ticks(uint32_t ticks);


/**
 * @brief Internal function: wake up any sleeping tasks whose wake-up time has arrived.
 * Called by SysTick_Handler in systick.c.
 */
void scheduler_wake_sleeping_tasks(void);


#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
