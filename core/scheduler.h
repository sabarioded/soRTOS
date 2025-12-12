#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <string.h> /* for memset */
#include "utils.h"
#include "device_registers.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SRAM1 has 96 KB = 98304 bytes
 * Each task stack is 256 words = 1024 bytes
 * Each TCB is (4 + 4 + 1020 + 1 + 1 + 2) bytes = 1032 bytes.
 * Heap is 32k and other data is about 4k, so about 60k left for tasks.
 * So we can have about 58 tasks max.
*/
#define MAX_TASKS             58
#define STACK_SIZE_IN_WORDS   255 /* 1020 bytes per task stack for 8-byte alignment */
#define EXC_RETURN_THREAD_PSP 0xFFFFFFFDu   /* EXC_RETURN: return to Thread mode, use PSP */
#define STACK_CANARY          0xDEADBEEF

typedef enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
} task_state_t;

typedef enum task_return {
    TASK_DELETE_SUCCESS,
    TASK_DELETE_TASK_NOT_FOUND,
    TASK_DELETE_IS_IDLE,
    TASK_DELETE_IS_CURRENT_TASK,
} task_return_t;

typedef struct task_struct {
    uint32_t *psp;
    uint32_t *psp_min;          /* deepest PSP ever seen, still not implemented */
    uint32_t  stack[STACK_SIZE_IN_WORDS] __attribute__((aligned(8)));
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
 * @brief Create a new runnable task
 * 
 * @param task_func Entry function for the task
 * @param arg Argument to pass to the task function
 * @return Task ID on success, -1 on failure
 */
int32_t task_create(void (*task_func)(void *), void *arg);


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
task_return_t task_delete(uint16_t task_id);


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


#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
