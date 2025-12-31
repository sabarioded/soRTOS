#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    void     *psp;              /* Platform-agnostic Stack Pointer (Current Top) */
    uint32_t  sleep_until_tick; /* System Tick count when task should wake */
    
    void     *stack_ptr;        /* Pointer to start of allocated memory */
    size_t    stack_size;       /* Size of allocated stack in bytes */
    
    uint8_t   state;            /* Current task state */
    uint8_t   is_idle;          /* Flag for idle task identification */
    uint16_t  task_id;          /* Unique Task ID */
} task_t;

/**
 * @brief Initialize the scheduler internal structures.
 */
void scheduler_init(void);

/**
 * @brief Start the scheduler and run the first task.
 * @note This function DOES NOT RETURN.
 */
void scheduler_start(void);

/**
 * @brief Schedule the next task to run.
 * @note Internal function usually called by Context Switch handler or Tick Handler.
 */
void schedule_next_task(void);

/**
 * @brief Internal function: wake up any sleeping tasks whose wake-up time has arrived.
 * @note Called by System Tick Handler.
 */
void scheduler_wake_sleeping_tasks(void);

/**
 * @brief Create a new task.
 * @param task_func Entry function for the task.
 * @param arg Argument to pass to the task function.
 * @param stack_size_bytes Stack size in bytes.
 * @return Task ID on success, -1 on failure.
 */
int32_t task_create(void (*task_func)(void *), void *arg, size_t stack_size_bytes);

/**
 * @brief Delete a task.
 * @param task_id ID of the task to delete.
 * @return TASK_DELETE_SUCCESS on success, otherwise error code.
 */
int32_t task_delete(uint16_t task_id);

/**
 * @brief Allows a running task to voluntarily delete itself.
 * This function marks the task for deletion and yields the CPU.
 */
void task_exit(void);

/**
 * @brief Rearrange the task list, delete unused (ZOMBIE) tasks.
 * Should be called by the Idle Task.
 */
void task_garbage_collection(void);

/**
 * @brief Check all tasks for stack overflow (can be called periodically).
 */
void task_check_stack_overflow(void);

/**
 * @brief Block a specific task (prevent it from being scheduled).
 * @param task Pointer to the task to block.
 */
void task_block(task_t *task);

/**
 * @brief Unblock a specific task (make it ready to run).
 * @param task Pointer to the task to unblock.
 */
void task_unblock(task_t *task);

/**
 * @brief Block the currently running task.
 */
void task_block_current(void);

/**
 * @brief Sleep the current task for a specified number of System Ticks.
 * The task will be moved to BLOCKED state and automatically unblocked
 * when the system tick counter reaches the wake-up time.
 * * @param ticks Number of ticks to sleep (must be > 0).
 * @return 0 on success, -1 on error.
 */
int task_sleep_ticks(uint32_t ticks);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */