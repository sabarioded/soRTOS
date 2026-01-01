#include "scheduler.h"
#include <stdint.h>
#include "utils.h"
#include "allocator.h"
#include "platform.h"
#include "arch_ops.h"

typedef struct task_struct {
    void     *psp;              /* Platform-agnostic Stack Pointer (Current Top) */
    uint32_t  sleep_until_tick; /* System Tick count when task should wake */
    
    void     *stack_ptr;        /* Pointer to start of allocated memory */
    size_t    stack_size;       /* Size of allocated stack in bytes */
    
    uint8_t   state;            /* Current task state */
    uint8_t   is_idle;          /* Flag for idle task identification */
    uint16_t  task_id;          /* Unique Task ID */
} task_t;

task_t   task_list[MAX_TASKS];
task_t  *task_current = NULL;
task_t  *task_next = NULL;

static uint32_t task_count = 0;
static uint32_t task_current_index = 0;
static uint16_t next_task_id = 0;
static task_t *idle_task = NULL;

/* Idle task function */
static void task_idle_function(void *arg) {
    (void)arg; /* Unused parameter */
    
    static size_t last_gc_tick = 0;
    
    while(1) {
        /* Run garbage collection periodically */
        size_t current_ticks = platform_get_ticks();
        
        if ((current_ticks - last_gc_tick) >= GARBAGE_COLLECTION_TICKS) {
            task_garbage_collection();
            last_gc_tick = current_ticks;
        }
        
        /* Put CPU to sleep to save power until next interrupt */
        platform_cpu_idle();
    }
}

/* Create the idle task */
static void task_create_idle(void) {
    if (idle_task != NULL) {
        return; 
    }

    /* Create idle task with minimal stack */
    int32_t task_id = task_create(task_idle_function, NULL, STACK_SIZE_512B);

    if (task_id < 0) {
        /* Failed to create idle task - this is a critical system failure */
        platform_panic();
        return;
    }

    /* Mark the newly created task as the IDLE task */
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

/* Start the scheduler */
void scheduler_start(void) {
    /* Ensure we have at least one task (or create idle if allowed) */
    if (task_count == 0) {
        task_create_idle(); 
    } else {
        /* Even if user tasks exist, we MUST have an idle task for fallback */
        task_create_idle();
    }

    task_current_index = 0;
    task_current = &task_list[0];
    task_next = &task_list[0];
    
    /* Mark first task as running */
    task_current->state = TASK_RUNNING;
    
    /* Hand over control to the platform scheduler start */
    platform_start_scheduler((uint32_t)task_current->psp);
}

/* Create a new task */
int32_t task_create(void (*task_func)(void *), void *arg, size_t stack_size_bytes)
{
    if (task_func == NULL) {
        return -1;
    }

    if (stack_size_bytes < STACK_MIN_SIZE_BYTES || stack_size_bytes > STACK_MAX_SIZE_BYTES) {
        return -1;
    }

    /* Align stack size request to platform alignment boundary */
    size_t align_mask = (size_t)(PLATFORM_STACK_ALIGNMENT - 1);
    stack_size_bytes = (stack_size_bytes + align_mask) & ~align_mask;

    uint32_t stat = arch_irq_lock();

    /* Check limit inside lock to prevent race condition */
    if (task_count >= MAX_TASKS) {
        arch_irq_unlock(stat);
        return -1;
    }

    /* Find unused task slot */
    uint32_t unused_task_index = task_count;
    task_t *new_task = &task_list[task_count]; /* Default to appending */

    for (uint32_t i = 0; i < task_count; ++i) {
        if (task_list[i].state == TASK_UNUSED) {
            new_task = &task_list[i];
            unused_task_index = i;
            break;
        }
    }

    /* Allocate stack from heap */
    uint32_t *stack_base = (uint32_t*)allocator_malloc(stack_size_bytes);
    if (stack_base == NULL) {
        arch_irq_unlock(stat);
        return -1;  /* Allocation failed */
    }

    /* Calculate Top of Stack */
    /* Stack grows down, so "Top" is at the highest address */
    uint32_t *stack_end = (uint32_t*)((uint8_t*)stack_base + stack_size_bytes);

    /* Enforce platform alignment on stack top address */
    /* Note: casting to uintptr_t is safer for bitwise ops on addresses */
    uintptr_t stack_addr = (uintptr_t)stack_end;
    stack_addr &= ~(uintptr_t)(PLATFORM_STACK_ALIGNMENT - 1);
    stack_end = (uint32_t *)stack_addr;

    /* Initialize Task Structure */
    new_task->stack_ptr = stack_base;     /* Save base for free() */
    new_task->stack_size   = stack_size_bytes;
    
    /* Initialize Task Context (Platform Specific) */
    new_task->psp = platform_initialize_stack(stack_end, task_func, arg, task_exit);
    
    new_task->state = TASK_READY;
    new_task->is_idle = 0;
    new_task->task_id = ++next_task_id;
    new_task->sleep_until_tick = 0;

    /* Set stack canary at the bottom for overflow detection */
    stack_base[0] = STACK_CANARY;

    /* Update count if we appended a new task */
    if (unused_task_index == task_count) {
        task_count++;
    }

    arch_irq_unlock(stat);

    return new_task->task_id;
}

/* Delete a task */
int32_t task_delete(uint16_t task_id) {
    uint32_t stat = arch_irq_lock();

    task_t *task_to_delete = NULL;

    for (uint32_t i = 0; i < task_count; ++i) {
        if (task_list[i].task_id == task_id) {
            task_to_delete = &task_list[i];
            break;
        }
    }

    if (task_to_delete == NULL) {
        arch_irq_unlock(stat);
        return TASK_DELETE_TASK_NOT_FOUND;
    }

    if (task_to_delete->is_idle) {
        arch_irq_unlock(stat);
        return TASK_DELETE_IS_IDLE;
    }
    
    if (task_to_delete == task_current) {
        arch_irq_unlock(stat);
        return TASK_DELETE_IS_CURRENT_TASK; 
    }

    /* Mark task as zombie. Its stack will be freed in garbage collection. */
    task_to_delete->state = TASK_ZOMBIE;
    task_to_delete->task_id = 0;

    arch_irq_unlock(stat);

    return TASK_DELETE_SUCCESS;
}

/* Task voluntarily exits */
void task_exit(void) {
    uint32_t stat = arch_irq_lock();
    
    if (task_current != NULL) {
        task_current->state = TASK_ZOMBIE;
        task_current->task_id = 0;
    }
    
    arch_irq_unlock(stat);

    /* Yield immediately to allow scheduler to switch away */
    while(1) {
        platform_yield();
    }
}

/* Called by Platform Context Switcher to pick next task */ 
void schedule_next_task(void) {
    if (task_count == 0) {
        return;
    }

    if (task_current == NULL) {
        /* Should happen only at very first start */
        task_current_index = 0;
        task_current = &task_list[0];
        task_current->state = TASK_RUNNING;
        task_next = task_current;
        return;
    }
    
    /* If current task was running, move it to READY (unless blocked/zombie) */
    if (task_current && task_current->state == TASK_RUNNING) {
        task_current->state = TASK_READY;
    }

    /* Round-Robin: Start searching from next index */
    uint32_t next = (task_current_index + 1) % task_count;
    
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

    /* If no READY user task found, schedule IDLE task */
    if (idle_task != NULL && idle_task->state == TASK_READY) {
        task_next = idle_task;
        task_next->state = TASK_RUNNING;

        /* Update index to point to idle task so next search starts after it */
        for (uint32_t i = 0; i < task_count; i++) {
            if (&task_list[i] == idle_task) {
                task_current_index = i;
                break;
            }
        }
        return;
    }

    /* Fallback: if absolutely nothing is ready (shouldn't happen if idle exists), just stay */
    /* Ensure we don't switch to a blocked task */
    if (task_current->state == TASK_READY || task_current->state == TASK_RUNNING) {
        task_next = task_current;
        task_next->state = TASK_RUNNING;
    } else {
        /* If current is blocked and no idle task, we have a critical failure */
        platform_panic();
    }
}

/* Rearrange active tasks and free zombie stacks */
void task_garbage_collection(void) {
    uint32_t stat = arch_irq_lock();
    
    uint32_t read_idx = 0;
    uint32_t write_idx = 0;
    
    while (read_idx < task_count) {
        /* Check if task is dead */
        if (task_list[read_idx].state == TASK_ZOMBIE) {
            /* Free the stack memory */
            if (task_list[read_idx].stack_ptr != NULL) {
                allocator_free(task_list[read_idx].stack_ptr);
                task_list[read_idx].stack_ptr = NULL;
            }
            task_list[read_idx].state = TASK_UNUSED;
        }

        /* If task is active (or just freed/unused), compact list */
        if (task_list[read_idx].state != TASK_UNUSED) {
            if (read_idx != write_idx) {
                /* Move task struct to fill gap */
                task_list[write_idx] = task_list[read_idx];
                
                /* Fix up global pointers if they pointed to the moved task */
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

    /* Zero out the now-unused slots at the end */
    memset(&task_list[task_count], 0, (MAX_TASKS - task_count) * sizeof(task_t));

    arch_irq_unlock(stat);
}

/* Check all tasks for stack overflow */
void task_check_stack_overflow(void) {
    uint32_t current_task_overflow = 0;
    uint32_t stat = arch_irq_lock();

    for (uint32_t i = 0; i < task_count; ++i) {
        if (task_list[i].state != TASK_UNUSED) {
            /* Use stack_ptr (void*) and cast to check canary */
            uint32_t *stack_base = (uint32_t*)task_list[i].stack_ptr;

            if (stack_base != NULL && stack_base[0] != STACK_CANARY) {
                if (&task_list[i] == task_current) {
                    current_task_overflow = 1;
                } else {
                    /* Kill other tasks immediately */
                    uint16_t id_to_delete = task_list[i].task_id;
                    arch_irq_unlock(stat);
                    task_delete(id_to_delete);
                    stat = arch_irq_lock();
                }
            }
        }
    }

    arch_irq_unlock(stat);

    /* If current task overflowed, we cannot continue safely */
    if (current_task_overflow) {
        task_exit();
    }
}

/* Block a task */
void task_block(task_t *task) {
    if (task == NULL) return;

    uint32_t stat = arch_irq_lock();
    if (task->state != TASK_UNUSED && !task->is_idle) {
        task->state = TASK_BLOCKED;
    }
    arch_irq_unlock(stat);
}

/* Unblock a task */
void task_unblock(task_t *task) {
    if (task == NULL) return;

    uint32_t stat = arch_irq_lock();
    if (task->state == TASK_BLOCKED) {
        task->state = TASK_READY;
    }
    arch_irq_unlock(stat);
}

/* Block current task */
void task_block_current(void) {
    uint32_t stat = arch_irq_lock();
    if (task_current && task_current->state != TASK_UNUSED && !task_current->is_idle) {
        task_current->state = TASK_BLOCKED;
    }
    arch_irq_unlock(stat);
    
    platform_yield();
}

/* Sleep for N ticks */
int task_sleep_ticks(uint32_t ticks)
{
    if (ticks == 0 || task_current == NULL) {
        return -1;
    }

    uint32_t stat = arch_irq_lock();

    /* Set wake-up time */
    uint32_t wake_tick = (uint32_t)platform_get_ticks() + ticks;
    if (wake_tick == 0) {
        wake_tick = 1; /* Ensure 0 is not used as it indicates 'not sleeping' */
    }
    task_current->sleep_until_tick = wake_tick;

    /* Block task */
    if (task_current->state != TASK_UNUSED && !task_current->is_idle) {
        task_current->state = TASK_BLOCKED;
    }

    arch_irq_unlock(stat);

    /* Yield immediately */
    platform_yield();

    return 0;
}

/* Wake up sleeping tasks (Called by System Tick Handler) */
void scheduler_wake_sleeping_tasks(void)
{
    /* Note: Tick Handler is usually in an interrupt context, 
       so we might not strictly need irq_lock if priorities are right,
       but locking here is safe practice. */
    
    uint32_t current_ticks = (uint32_t)platform_get_ticks();

    for (uint32_t i = 0; i < task_count; ++i) {
        if (task_list[i].state == TASK_BLOCKED && 
            task_list[i].sleep_until_tick != 0 &&
            (int32_t)(current_ticks - task_list[i].sleep_until_tick) >= 0) {
            
            task_list[i].state = TASK_READY;
            task_list[i].sleep_until_tick = 0;
        }
    }
}

/* Get the handle of the currently running task */
void *task_get_current(void) {
    return (void *)task_current;
}

/* Change the task state. Must be called with IRQs ALREADY disabled. */
void task_set_state(task_t *t, task_state_t state) {
    t->state = state;
}

/* Atomically get the task state */
task_state_t task_get_state_atomic(task_t *t) {
    return (task_state_t)t->state;
}

/* Get the unique ID of a task */
uint16_t task_get_id(task_t *t) {
    return t->task_id;
}

/* Get the allocated stack size of a task */
size_t task_get_stack_size(task_t *t) {
    return t->stack_size;
}

/* Get the pointer to the start of the task's stack memory */
void* task_get_stack_ptr(task_t *t) {
    return t->stack_ptr;
}

/* Get a task handle by index */
task_t *scheduler_get_task_by_index(uint32_t index) {
    if (index >= MAX_TASKS) {
        return NULL;
    }
    return &task_list[index];
}
