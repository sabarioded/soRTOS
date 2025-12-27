#include "scheduler.h"
#include "utils.h"
#include "systick.h" 
#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
#include "allocator.h"
#endif

task_t   task_list[MAX_TASKS];
task_t  *task_current = NULL;
task_t  *task_next = NULL;

static uint32_t task_count = 0;
static uint32_t task_current_index = 0;
static uint16_t next_task_id = 0;
static task_t *idle_task = NULL;

void task_create_first(void); /* Forward declaration of the assembly entry */


/* Idle task function */
static void task_idle_function(void *arg) {
    (void)arg; /* Unused parameter */
    static uint32_t last_gc_tick = 0;
    while(1) {
        /* Run garbage collection */
        if ((systick_ticks - last_gc_tick) >= GARBAGE_COLLECTION_TICKS) {
            task_garbage_collection();
            last_gc_tick = systick_ticks;
        }
        __WFI(); /* Wait For Interrupt */
    }
}


/* Initialize the stack frame for a new task */
static uint32_t *initialize_stack(uint32_t *top_of_stack,
                                    void (*task_func)(void *),
                                    void *arg)
{
    /* Push initial stack frame as expected by the hardware on exception return */
    *(top_of_stack--)   = 0x01000000u;          /* xPSR: Thumb bit set */
    *(top_of_stack--)   = (uint32_t)task_func;  /* PC entry point */
    *(top_of_stack--)   = (uint32_t)task_exit;  /* LR: task exit handler */
    *(top_of_stack--)   = 0x0u;                 /* R12 */
    *(top_of_stack--)   = 0x0u;                 /* R3 */
    *(top_of_stack--)   = 0x0u;                 /* R2 */
    *(top_of_stack--)   = 0x0u;                 /* R1 */
    *(top_of_stack--)   = (uint32_t)arg;        /* R0 */

    /* 
     * Software Context (pushes 8 words: R4-R11) 
     * These are expected by PendSV_Handler LDMIA instruction 
     */
    *(top_of_stack--)   = 0;                    /* R11 */
    *(top_of_stack--)   = 0;                    /* R10 */
    *(top_of_stack--)   = 0;                    /* R9 */
    *(top_of_stack--)   = 0;                    /* R8 */
    *(top_of_stack--)   = 0;                    /* R7 */
    *(top_of_stack--)   = 0;                    /* R6 */
    *(top_of_stack--)   = 0;                    /* R5 */
    *(top_of_stack)     = 0;                    /* R4 */

    return top_of_stack;
}


/* Create the idle task */
static void task_create_idle(void) {
    if (idle_task != NULL) {
        return; 
    }

    int32_t task_id = task_create(task_idle_function, NULL, STACK_SIZE_512B);

    if (task_id < 0) {
        /* Failed to create idle task - this is a critical error */
        return;
    }

    /* find the idle task i just created */
    for (uint32_t i = 0; i < task_count; ++i) {
        if(task_list[i].task_id == task_id) {
            idle_task = &task_list[i];
            idle_task->is_idle = 1;
            return;
        }
    }
    
}


/* Initialize the scheduler */
void scheduler_init(void) {
    memset(task_list, 0, sizeof(task_list));
    task_current = NULL;
    task_next = NULL;
    task_count = 0;
    task_current_index = 0;
    next_task_id = 0;
    idle_task = NULL;

}


/* Create a new task */
int32_t task_create(void (*task_func)(void *), void *arg, size_t stack_size_bytes)
{
    if (task_count >= MAX_TASKS || task_func == NULL) {
        return -1;
    }

#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
    /* Validate and align stack size for dynamic mode */
    if (stack_size_bytes < STACK_MIN_SIZE_BYTES) {
        stack_size_bytes = STACK_MIN_SIZE_BYTES;
    }
    if (stack_size_bytes > STACK_MAX_SIZE_BYTES) {
        return -1;  /* Stack too large */
    }
    /* Align to 8-byte boundary */
    stack_size_bytes = (stack_size_bytes + 7) & ~7;
#else
    /* In static mode, ignore parameter and use compile-time size */
    (void)stack_size_bytes;  /* Suppress unused parameter warning */
    stack_size_bytes = STACK_SIZE_BYTES;
#endif

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    /* Find unused task slot */
    uint32_t unused_task_index = task_count;
    task_t *new_task = &task_list[task_count];

    for (uint32_t i = 0; i < task_count; ++i) {
        if (task_list[i].state == TASK_UNUSED) {
            new_task = &task_list[i];
            unused_task_index = i;
            break;
        }
    }

    uint32_t *stack_end = NULL;
    uint32_t *stack_base = NULL;

#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_STATIC
    /* Static allocation: use embedded stack */
    stack_base = new_task->stack;
    stack_end = &new_task->stack[STACK_SIZE_IN_WORDS - 1];
#else
    /* Dynamic allocation: allocate stack from heap */
    stack_base = (uint32_t*)allocator_malloc(stack_size_bytes);
    if (stack_base == NULL) {
        exit_critical_basepri(stat);
        return -1;  /* Allocation failed */
    }
    new_task->stack_ptr = stack_base;
    new_task->stack_size = stack_size_bytes;
    stack_end = (uint32_t*)((uint8_t*)stack_base + stack_size_bytes - sizeof(uint32_t));
#endif

    /* Enforce 8-byte alignment on stack top */
    uint32_t stack_addr = (uint32_t)stack_end;
    stack_addr &= ~0x7u;
    stack_end = (uint32_t *)stack_addr;

    /* Initialize task */
    new_task->psp = initialize_stack(stack_end, task_func, arg);
    new_task->state = TASK_READY;
    new_task->is_idle = 0;
    new_task->task_id = ++next_task_id;
    new_task->sleep_until_tick = 0;

    if (unused_task_index == task_count) {
        task_count++;
    }

    /* Set stack canary at the bottom for overflow detection */
    stack_base[0] = STACK_CANARY;

    exit_critical_basepri(stat);

    return new_task->task_id;
}


/* Start the scheduler */
void scheduler_start(void) {
    if (task_count == 0) {
        return;
    }

    task_create_idle();

    task_current_index = 0;
    task_current = &task_list[0];
    task_next = &task_list[0];
    task_current->state = TASK_RUNNING;
    task_create_first(); /* Assembly function to start the first task */
}


/* Called by PendSV to pick next task */ 
void schedule_next_task(void) {
    if (task_count == 0) {
        return;
    }

    if (task_current == NULL) {
        task_current_index = 0;
        task_current = &task_list[0];
        task_current->state = TASK_RUNNING;
        task_next = task_current;
        return;
    }
    
    uint32_t next = (task_current_index + 1) % task_count;
    if (task_current && task_current->state == TASK_RUNNING) {
        task_current->state = TASK_READY;
    }

    /* Simple round-robin: find next READY task that is not sleeping */
    for (uint32_t i = 0; i < task_count; ++i) {
        if(task_list[next].state == TASK_READY &&
           task_list[next].is_idle == 0 &&
           task_list[next].sleep_until_tick == 0) {  /* Not sleeping */

            task_current_index = next;
            task_next = &task_list[next];
            task_next->state = TASK_RUNNING;
            return;
        }
        next = (next + 1) % task_count;
    }

    /* If no READY task found, schedule idle task */
    if (idle_task != NULL && idle_task->state == TASK_READY) {
        task_next = idle_task;
        task_next->state = TASK_RUNNING;

        for (uint32_t i = 0; i < task_count; i++) {
            if (&task_list[i] == idle_task) {
                task_current_index = i;
                break;
            }
        }
        return;
    }

    /* Fallback: if all tasks are non-READY, just stay */
    task_next = &task_list[task_current_index];
    task_next->state = TASK_RUNNING;
}


/* Block a task */
void task_block(task_t *task) {
    if (task == NULL) {
        return;
    }

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    if (task->state != TASK_UNUSED && !task->is_idle) {
        task->state = TASK_BLOCKED;
    }

    exit_critical_basepri(stat);
}


/* Unblock a task */
void task_unblock(task_t *task) {
    if (task == NULL) {
        return;
    }

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    if (task->state == TASK_BLOCKED) {
        task->state = TASK_READY;
    }

    exit_critical_basepri(stat);
}


/* Block the current task */
void task_block_current(void) {
    if (task_current == NULL) {
        return;
    }

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    if (task_current && task_current->state != TASK_UNUSED && !task_current->is_idle) {
        task_current->state = TASK_BLOCKED;
    }

    exit_critical_basepri(stat);

    yield_cpu();
}


/* Delete a task */
int32_t task_delete(uint16_t task_id) {
    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    task_t *task_to_delete = NULL;

    for (uint32_t i = 0; i < task_count; ++i) {
        if (task_list[i].task_id == task_id) {
            task_to_delete = &task_list[i];
            break;
        }
    }

    if (task_to_delete == NULL) {
        exit_critical_basepri(stat);
        return TASK_DELETE_TASK_NOT_FOUND;
    }

    if (task_to_delete->is_idle) {
        exit_critical_basepri(stat);
        return TASK_DELETE_IS_IDLE;
    }
    
    if (task_to_delete == task_current) {
        exit_critical_basepri(stat);
        return TASK_DELETE_IS_CURRENT_TASK; 
    }

    /* Mark task as zombie and free its stack in garbage collection */
    task_to_delete->state = TASK_ZOMBIE;
    task_to_delete->task_id = 0;

    exit_critical_basepri(stat);

    return TASK_DELETE_SUCCESS;
}


/* Check all tasks for stack overflow */
void task_check_stack_overflow(void) {
    uint32_t current_task_overflow = 0;

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    for (uint32_t i = 0; i < task_count; ++i) {
        if (task_list[i].state != TASK_UNUSED) {
            uint32_t *stack_base = NULL;
#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_STATIC
            stack_base = task_list[i].stack;
#else
            stack_base = task_list[i].stack_ptr;
#endif
            
            if (stack_base != NULL && stack_base[0] != STACK_CANARY) {
                if (&task_list[i] == task_current) {
                    current_task_overflow = 1;
                } else {
                    uint16_t id_to_delete = task_list[i].task_id;
                    exit_critical_basepri(stat);
                    task_delete(id_to_delete);
                    stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);
                }
            }
        }
    }

    exit_critical_basepri(stat);

    if (current_task_overflow) {
        task_exit();
    }
}
 

/* task voluntarily exits */
void task_exit(void) {
    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    if (task_current != NULL) {
        task_current->state = TASK_ZOMBIE;
        task_current->task_id = 0;
    }

    exit_critical_basepri(stat);

    while(1) {
        yield_cpu();
    }
}


/* Rearrage active tasks and update their count */
void task_garbage_collection(void) {
    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);
    
    uint32_t read_idx = 0;
    uint32_t write_idx = 0;
    
    // Find first available slot
    while (read_idx < task_count) {
        if (task_list[read_idx].state == TASK_ZOMBIE) {
#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
            if (task_list[read_idx].stack_ptr != NULL) {
                allocator_free(task_list[read_idx].stack_ptr);
                task_list[read_idx].stack_ptr = NULL;
            }
            task_list[read_idx].state = TASK_UNUSED;
#endif
        }

        if (task_list[read_idx].state != TASK_UNUSED) {
            // Found an active task. If it's not where it should be, move it.
            if (read_idx != write_idx) {
                task_list[write_idx] = task_list[read_idx];
                
                // Update pointers and indices if the current task was moved
                if (&task_list[read_idx] == task_current) {
                    task_current = &task_list[write_idx];
                    task_current_index = write_idx;
                }

                if (&task_list[read_idx] == task_next) {
                    task_next = &task_list[write_idx];
                }

                if (idle_task == &task_list[read_idx]) {
                    idle_task = &task_list[write_idx];
                }
            }
            write_idx++;
        }
        read_idx++;
    }
    
    task_count = write_idx;

    // Clear the unused slots at the end
    memset(&task_list[task_count], 0, (MAX_TASKS - task_count) * sizeof(task_t));

    exit_critical_basepri(stat);
}


/**
 * @brief Sleep the current task for a specified number of SysTick ticks.
 * 
 * The task is moved to BLOCKED state and set to wake up at systick_ticks + ticks.
 * When the SysTick handler runs and detects the wake-up time has arrived, the task
 * is automatically moved back to READY state.
 * 
 * @param ticks Number of SysTick ticks to sleep (must be > 0)
 * @return 0 on success, -1 on error
 */
int task_sleep_ticks(uint32_t ticks)
{
    if (ticks == 0 || task_current == NULL) {
        return -1;
    }

    uint32_t stat = enter_critical_basepri(MAX_SYSCALL_PRIORITY);

    /* Set the wake-up time */
    task_current->sleep_until_tick = systick_ticks + ticks;

    /* Block the task */
    if (task_current->state != TASK_UNUSED && !task_current->is_idle) {
        task_current->state = TASK_BLOCKED;
    }

    exit_critical_basepri(stat);

    /* Yield to allow scheduler to pick next task */
    yield_cpu();

    return 0;
}

/**
 * @brief Wake up any sleeping tasks whose wake-up time has arrived.
 * Called by SysTick_Handler in systick.c.
 */
void scheduler_wake_sleeping_tasks(void)
{
    /* Check all tasks for wake-up conditions */
    for (uint32_t i = 0; i < task_count; ++i) {
        if (task_list[i].state == TASK_BLOCKED && 
            task_list[i].sleep_until_tick != 0 &&
            systick_ticks >= task_list[i].sleep_until_tick) {
            
            /* Wake up the task */
            task_list[i].state = TASK_READY;
            task_list[i].sleep_until_tick = 0;
        }
    }
}

