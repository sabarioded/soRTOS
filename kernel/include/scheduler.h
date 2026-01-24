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
    TASK_SLEEPING,
    TASK_ZOMBIE,
} task_state_t;

typedef struct wait_node {
    void *task;
    struct wait_node *next;
} wait_node_t;

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
 */
void schedule_next_task(void);

/**
 * @brief Process a system tick. Updates time slices and wakes sleeping tasks.
 * @note Called by System Tick Handler (ISR).
 * @return 1 if a context switch is required (slice expired or preemption), 0 otherwise.
 */
uint32_t scheduler_tick(void);

/**
 * @brief Create a new task.
 * @param task_func Entry function for the task.
 * @param arg Argument to pass to the task function.
 * @param stack_size_bytes Stack size in bytes.
 * @param weight Task weight (higher number = more CPU time).
 * @return Task ID on success, -1 on failure.
 */
int32_t task_create(void (*task_func)(void *), 
                    void *arg, 
                    size_t stack_size_bytes, 
                    uint8_t weight);

/**
 * @brief Create a new task with a statically allocated stack.
 * @param task_func Entry function for the task.
 * @param arg Argument to pass to the task function.
 * @param stack_buffer Pointer to the user-provided stack memory.
 * @param stack_size_bytes Size of the stack buffer.
 * @param weight Task weight.
 * @return Task ID on success, -1 on failure.
 */
int32_t task_create_static(void (*task_func)(void *), 
                           void *arg, 
                           void *stack_buffer, 
                           size_t stack_size_bytes, 
                           uint8_t weight);

/**
 * @brief Delete a task.
 * @param task_id ID of the task to delete.
 * @return 0 on success, otherwise error code.
 */
int32_t task_delete(uint16_t task_id);

/**
 * @brief Allows a running task to voluntarily delete itself.
 * This function marks the task for deletion and yields the CPU.
 */
void task_exit(void);

/**
 * @brief Rearrange the task list, delete unused (ZOMBIE) tasks.
 */
void task_garbage_collection(void);

/**
 * @brief Check all tasks for stack overflow.
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
 * @brief Put the current task to sleep for a specific number of ticks.
 * 
 * @param ticks Number of system ticks to sleep.
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
 * @return void* Opaque pointer to the current task.
 */
void *task_get_current(void);

/**
 * @brief Change the state of a specific task.
 * @warning This function does NOT lock interrupts. The caller must ensure 
 * thread safety before calling.
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
 * @brief Set the weight of a task.
 * @param t Pointer to the task.
 * @param weight New weight.
 */
void task_set_weight(task_t *t, uint8_t weight);

/**
 * @brief Get the remaining time slice of a task.
 * @param t Pointer to the task.
 * @return Remaining ticks.
 */
uint32_t task_get_time_slice(task_t *t);

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
 * @brief Get a task handle by index.
 * @param index Index in the task list.
 * @return Pointer to task_t or NULL if index out of bounds.
 */
task_t *scheduler_get_task_by_index(uint32_t index);

/**
 * @brief Get the embedded wait node for a task.
 * Used by queues/mutexes to link the task into a wait list without dynamic allocation.
 * @param task Pointer to the task.
 * @return Pointer to the task's wait node.
 */
wait_node_t* task_get_wait_node(task_t *task);

/**
 * @brief Set the notification value for a task.
 * @param t Pointer to the task.
 * @param val Value to set.
 */
void task_set_notify_val(task_t *t, uint32_t val);

/**
 * @brief Get the notification value for a task.
 * @param t Pointer to the task.
 * @return Notification value.
 */
uint32_t task_get_notify_val(task_t *t);

/**
 * @brief Set the notification state for a task.
 * @param t Pointer to the task.
 * @param state State to set (0 or 1).
 */
void task_set_notify_state(task_t *t, uint8_t state);

/**
 * @brief Get the notification state for a task.
 * @param t Pointer to the task.
 * @return Notification state.
 */
uint8_t task_get_notify_state(task_t *t);

/**
 * @brief Get the total CPU ticks consumed by a task.
 * @param t Pointer to the task.
 * @return Total ticks.
 */
uint64_t task_get_cpu_ticks(task_t *t);

/**
 * @brief Get the base weight of a task.
 * @param t Pointer to the task.
 * @return Base weight.
 */
uint8_t task_get_base_weight(task_t *t);

/**
 * @brief Restore a task's weight to its base weight.
 * @param t Pointer to the task.
 */
void task_restore_base_weight(task_t *t);

/**
 * @brief Temporarily boost a task's weight.
 * @param t Pointer to the task.
 * @param weight New effective weight (ignored if lower than current).
 */
void task_boost_weight(task_t *t, uint8_t weight);

/**
 * @brief Set event group wait parameters for a task.
 * @param t Pointer to the task.
 * @param bits Bits to wait for.
 * @param flags Wait options (wait_all, clear_on_exit).
 */
void task_set_event_wait(task_t *t, uint32_t bits, uint8_t flags);

/**
 * @brief Get the event bits a task is waiting for (or the result).
 * @param t Pointer to the task.
 * @return Event bits.
 */
uint32_t task_get_event_bits(task_t *t);

/**
 * @brief Get the event flags for a task.
 * @param t Pointer to the task.
 * @return Flags.
 */
uint8_t task_get_event_flags(task_t *t);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */