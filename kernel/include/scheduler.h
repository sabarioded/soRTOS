#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Weights for Stride Scheduling (Higher weight = More CPU time) */
#define TASK_WEIGHT_IDLE        1
#define TASK_WEIGHT_LOW         10
#define TASK_WEIGHT_NORMAL      20
#define TASK_WEIGHT_HIGH        50

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

typedef struct task_struct task_t;

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
 * @param weight Task weight (higher number = more CPU time).
 * @return Task ID on success, -1 on failure.
 */
int32_t task_create(void (*task_func)(void *), void *arg, size_t stack_size_bytes, uint8_t weight);

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

/**
 * @brief Wait for a notification.
 * Blocks the current task until it receives a notification.
 * 
 * @param clear_on_exit If 1, the notification value is reset to 0 after reading.
 * @param wait_ticks Max ticks to wait (0 for no wait, UINT32_MAX for infinite).
 * @return The notification value, or 0 if timed out.
 */
uint32_t task_notify_wait(uint8_t clear_on_exit, uint32_t wait_ticks);

/**
 * @brief Send a notification to a specific task.
 * Wakes the task if it was blocked waiting for a notification.
 * 
 * @param task_id The ID of the task to notify.
 * @param value The value to OR into the task's notification value.
 */
void task_notify(uint16_t task_id, uint32_t value);

/**
 * @brief Get the handle of the currently running task.
 * @return void* Opaque pointer to the current task (can be cast to task_t* internally).
 */
void *task_get_current(void);

/**
 * @brief Change the state of a specific task.
 * @warning This function does NOT lock interrupts. The caller must ensure 
 * thread safety (usually by disabling interrupts) before calling.
 * @param t Pointer to the task.
 * @param state New state to set.
 */
void task_set_state(task_t *t, task_state_t state);

/**
 * @brief Retrieve the state of a task atomically.
 * @param t Pointer to the task.
 * @return The current state of the task.
 */
task_state_t task_get_state_atomic(task_t *t);

/**
 * @brief Get the unique ID of a task.
 * @param t Pointer to the task.
 * @return The task ID.
 */
uint16_t task_get_id(task_t *t);

/**
 * @brief Get the weight of a task.
 * @param t Pointer to the task.
 * @return The task weight.
 */
uint8_t task_get_weight(task_t *t);

/**
 * @brief Get the allocated stack size of a task.
 * @param t Pointer to the task.
 * @return Stack size in bytes.
 */
size_t task_get_stack_size(task_t *t);

/**
 * @brief Get the pointer to the start of the task's stack memory.
 * @param t Pointer to the task.
 * @return Pointer to the stack base.
 */
void* task_get_stack_ptr(task_t *t);

/**
 * @brief Get a task handle by index (for iteration/diagnostics).
 * @param index Index in the task list (0 to MAX_TASKS-1).
 * @return Pointer to task_t or NULL if index out of bounds.
 */
task_t *scheduler_get_task_by_index(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */