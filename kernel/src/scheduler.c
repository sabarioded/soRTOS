#include "scheduler.h"
#include <stdint.h>
#include "utils.h"
#include "allocator.h"
#include "platform.h"
#include "arch_ops.h"
#include "spinlock.h"

/* Time Slice Configuration */
#define BASE_SLICE_TICKS    2       /* Base ticks per weight unit */
#define VRUNTIME_SCALER     1000    /* Scaling factor for vruntime calc */

/* Bitmap helper macros */
#define BITMAP_IDX(id)  ((size_t)((id) - 1) / 64)
#define BITMAP_BIT(id)  ((size_t)((id) - 1) % 64)
#define BITMAP_SIZE     ((MAX_TASKS + 63) / 64)
#define IS_ID_USED(id)   (task_id_bitmap[BITMAP_IDX(id)] & (1ULL << BITMAP_BIT(id)))
#define MARK_ID_USED(id) (task_id_bitmap[BITMAP_IDX(id)] |= (1ULL << BITMAP_BIT(id)))
#define MARK_ID_FREE(id) (task_id_bitmap[BITMAP_IDX(id)] &= ~(1ULL << BITMAP_BIT(id)))

typedef struct task_struct {
    void            *psp;               /* Platform-agnostic Stack Pointer (Current Top) */
    uint32_t        sleep_until_tick;   /* System Tick count when task should wake */
    
    void            *stack_ptr;         /* Pointer to start of allocated memory */
    size_t          stack_size;         /* Size of allocated stack in bytes */
    
    uint8_t         state;              /* Current task state */
    uint8_t         is_idle;            /* Flag for idle task identification */
    uint16_t        task_id;            /* Unique Task ID */
    
    uint8_t         weight;             /* User assigned weight */
    uint32_t        time_slice;         /* Remaining ticks in current slice */
    uint64_t        vruntime;           /* Virtual runtime (fairness metric) */
    int32_t         heap_index;         /* Index in ready_heap (-1 if not in heap) */   
    
    uint32_t        notify_val;         /* Task notification value */
    uint8_t         notify_state;       /* 0: None, 1: Pending */
    struct task_struct *next;           /* Link for Sleep/Free/Zombie lists */
} task_t;

task_t      task_list[MAX_TASKS];
task_t      *task_current = NULL;

/* Min-Heap for Ready Tasks (ordered by 'vruntime') */
static task_t       *ready_heap[MAX_TASKS];
static uint32_t     heap_size = 0;

/* List for Sleeping/Free/Zombie Tasks */
static task_t       *sleep_list_head = NULL;
static task_t       *free_list = NULL;
static task_t       *zombie_list = NULL;

static uint32_t     task_count = 0;
static uint64_t     task_id_bitmap[BITMAP_SIZE];
static task_t       *idle_task = NULL;
static spinlock_t   scheduler_lock;

/**
 * The heap is implemented by a tree flattened into an array.
 * Parent: (index-1)/2
 * Left child: (2 * index) + 1
 * Right child (2 * index) + 2
 */

/* Swap two tasks in the heap and update their index tracking */
static void _swap_tasks(uint32_t i, uint32_t j) {
    task_t *temp = ready_heap[i];
    ready_heap[i] = ready_heap[j];
    ready_heap[j] = temp;
    
    /* Update indices in task structs */
    ready_heap[i]->heap_index = i;
    ready_heap[j]->heap_index = j;
}

/* Bubble up an element to maintain min-heap property */
static void _heap_up(uint32_t index) {
    while (index > 0) {
        uint32_t parent = (index - 1) / 2;
        if (ready_heap[index]->vruntime < ready_heap[parent]->vruntime) {
            _swap_tasks(index, parent);
            index = parent;
        } else {
            break;
        }
    }
}

/* Bubble down an element to maintain min-heap property */
static void _heap_down(uint32_t index) {
    while (1) {
        uint32_t left = 2 * index + 1; /* left child */
        uint32_t right = 2 * index + 2; /* right child */
        uint32_t smallest = index;

        if (left < heap_size && ready_heap[left]->vruntime < ready_heap[smallest]->vruntime) {
            smallest = left;
        }
        if (right < heap_size && ready_heap[right]->vruntime < ready_heap[smallest]->vruntime) {
            smallest = right;
        }

        if (smallest != index) {
            _swap_tasks(index, smallest);
            index = smallest;
        } else {
            break;
        }
    }
}

/* Insert a task into the priority queue */
static void _heap_insert(task_t *t) {
    if (heap_size >= MAX_TASKS) {
        return;
    }

    t->heap_index = heap_size;
    ready_heap[heap_size] = t;
    heap_size++;
    _heap_up(heap_size - 1);
}

/* Extract the task with the lowest vruntime value (highest priority) */
static task_t* _heap_pop_min(void) {
    if (heap_size == 0) {
        return NULL;
    }
    
    task_t *min = ready_heap[0];
    min->heap_index = -1; /* Mark as not in heap */
    
    heap_size--;
    if (heap_size > 0) {
        ready_heap[0] = ready_heap[heap_size];
        ready_heap[0]->heap_index = 0;
        _heap_down(0);
    }
    return min;
}

/* Remove a specific task from the middle of the heap */
static void _heap_remove(task_t *t) {
    if (t->heap_index == -1 || t->heap_index >= (int32_t)heap_size) {
        return;
    }
    
    uint32_t index = (uint32_t)t->heap_index;
    t->heap_index = -1;
    
    heap_size--;
    if (index < heap_size) {
        /* Move last element to this spot */
        ready_heap[index] = ready_heap[heap_size];
        ready_heap[index]->heap_index = index;
        
        /* Rebalance (could go up or down) */
        _heap_up(index);
        _heap_down(index);
    }
}

/* Get the minimum vruntime value currently in the system */
static uint64_t _get_min_vruntime(void) {
    if (heap_size > 0) {
        return ready_heap[0]->vruntime;
    }
    return (task_current) ? task_current->vruntime : 0;
}

/* Remove task from sleep list */
static void _remove_from_sleep_list(task_t *task) {
    if (task == NULL) {
        return;
    }

    /* Removing head */
    if (sleep_list_head == task) {
        sleep_list_head = task->next;
        task->next = NULL;
        return;
    }

    /* Removing from middle */
    task_t *curr = sleep_list_head;
    while (curr != NULL && curr->next != task) {
        curr = curr->next;
    }

    /* If task was found */
    if (curr != NULL) {
        curr->next = task->next;
        task->next = NULL;
    }
}

/* Insert task into sleep list (sorted by wake-up time) */
static void _insert_into_sleep_list(task_t *task) {
    if (task == NULL) {
        return;
    }

    /* We assume that the absolute tick value was calculated already
    ** for example if we need to sleep for 50 ticks and the time is 50,000 than
    ** the task->sleep_until_tick will be 50,050 
    */

    /* If empty list or new head */
    if (sleep_list_head == NULL || task->sleep_until_tick < sleep_list_head->sleep_until_tick) {
        task->next = sleep_list_head;
        sleep_list_head = task;
    } else {
        /* Otherwise insert in the middle */
        task_t *curr = sleep_list_head;
        while (curr->next != NULL && curr->next->sleep_until_tick < task->sleep_until_tick) {
            curr = curr->next;
        }
        task->next = curr->next;
        curr->next = task;
    }
}

static void inline _wake_sleeping_task(task_t *task) {
     task->state = TASK_READY;
     /* Insert into heap */
     uint64_t min_v = _get_min_vruntime();
     if (task->vruntime < min_v) {
         task->vruntime = min_v;
     }
     _heap_insert(task);
}

static void _process_sleep_list(uint32_t current_ticks) {
    task_t *curr = sleep_list_head;

    /* Process tasks at head of the list whose time has come */
    while (curr != NULL && (int32_t)(current_ticks - curr->sleep_until_tick) >= 0) {
        _remove_from_sleep_list(curr);
        _wake_sleeping_task(curr);
        curr = sleep_list_head;  /* Peek at new head */
    }
}

/* Unblock a task (Assumes Lock Held) */
static void _unblock_task_locked(task_t *task) {
    if (task->state == TASK_BLOCKED) {
        /* Remove from sleep list if it was waiting with timeout */
        if (task->sleep_until_tick > 0) {
            _remove_from_sleep_list(task);
            task->sleep_until_tick = 0;
        }

        task->state = TASK_READY;
        
        /* Insert to heap */
        uint64_t min_v = _get_min_vruntime();
        if (task->vruntime < min_v) {
            task->vruntime = min_v;
        }
        _heap_insert(task);
    }
}

/* Idle task function */
static void _task_idle_function(void *arg) {
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
static void _task_create_idle(void) {
    if (idle_task != NULL) {
        return; 
    }

    /* Create idle task with minimal stack */
    int32_t task_id = task_create(_task_idle_function, NULL, STACK_SIZE_512B, TASK_WEIGHT_IDLE);

    if (task_id < 0) {
        /* Failed to create idle task - this is a critical system failure */
        platform_panic();
        return;
    }

    /* Mark the newly created task as the IDLE task */
    for (uint32_t i = 0; i < MAX_TASKS; ++i) {
        if(task_list[i].task_id == task_id) {
            idle_task = &task_list[i];
            idle_task->is_idle = 1;
            /* Idle tasks should not be in the ready heap */
            _heap_remove(idle_task);
            return;
        }
    }
}

/* Initialize the scheduler */
void scheduler_init(void) {
    memset(task_list, 0, sizeof(task_list));
    task_current = NULL;
    task_count = 0;
    memset(task_id_bitmap, 0, sizeof(task_id_bitmap));
    idle_task = NULL;
    heap_size = 0;

    /* Initialize Free List */
    for (uint32_t i = 0; i < MAX_TASKS - 1; ++i) {
        task_list[i].next = &task_list[i+1];
    }
    task_list[MAX_TASKS - 1].next = NULL;
    free_list = &task_list[0];
    spinlock_init(&scheduler_lock);
}

/* Start the scheduler */
void scheduler_start(void) {
    if (task_count == 0 || idle_task == NULL) {
        _task_create_idle(); 
    }

    /* Pick the task to start with */
    task_t *min_vrt_task = _heap_pop_min();
    
    if (min_vrt_task == NULL) {
        /* If heap is empty, try to run idle task directly */
        if (idle_task != NULL) {
            task_current = idle_task;
            task_current->state = TASK_RUNNING;
            platform_start_scheduler((size_t)task_current->psp);
            return; /* Should not be reached on real hardware, but needed for tests */
        }
        platform_panic();
        return;
    }

    task_current = min_vrt_task;
    
    /* Mark first task as running */
    task_current->state = TASK_RUNNING;
    
    /* Hand over control to the platform scheduler start */
    platform_start_scheduler((size_t)task_current->psp);
}

/* Create a new task */
int32_t task_create(void (*task_func)(void *), void *arg, size_t stack_size_bytes, uint8_t weight)
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

    uint32_t stat = spin_lock(&scheduler_lock);

    if (free_list == NULL) {
        /* Resource exhaustion: Try to reclaim zombie tasks immediately */
        task_garbage_collection();
    }

    if (free_list == NULL) {
        spin_unlock(&scheduler_lock, stat);
        return -1;
    }
    task_t *new_task = free_list;
    free_list = free_list->next;
    new_task->next = NULL; /* Detach from free list */

    /* Allocate stack from heap */
    uint32_t *stack_base = (uint32_t*)allocator_malloc(stack_size_bytes);
    if (stack_base == NULL) {
        /* Allocation failed: Return task slot, run GC, and retry once */
        new_task->next = free_list;
        free_list = new_task;
        
        task_garbage_collection();
                
        /* Re-acquire task slot */
        if (free_list == NULL) {
            spin_unlock(&scheduler_lock, stat);
            return -1;
        }
        new_task = free_list;
        free_list = free_list->next;
        new_task->next = NULL;

        /* Retry allocation */
        stack_base = (uint32_t*)allocator_malloc(stack_size_bytes);
        if (stack_base == NULL) {
            /* Failed again: Rollback and exit */
            new_task->next = free_list;
            free_list = new_task;
            spin_unlock(&scheduler_lock, stat);
            return -1;
        }
    }

    /* Calculate Top of Stack (highest address). Remmber stack grows down */
    uint32_t *stack_end = (uint32_t*)((uint8_t*)stack_base + stack_size_bytes);

    /* Initialize task structure */
    new_task->stack_ptr     = stack_base;     /* Save base for free */
    new_task->stack_size    = stack_size_bytes;
    
    /* Initialize Task Context (Platform Specific) */
    new_task->psp = platform_initialize_stack(stack_end, task_func, arg, task_exit);
    
    new_task->state = TASK_READY;
    new_task->is_idle = 0;

    new_task->task_id = 0;
    /* Find first available ID using bitmap */
    for (uint16_t id = 1; id <= MAX_TASKS; ++id) {
        if (!IS_ID_USED(id)) {
            new_task->task_id = id;
            MARK_ID_USED(id);
            break;
        }
    }

    if (new_task->task_id == 0) {
        /* Rollback: Free stack and return task to free list */
        allocator_free(stack_base);
        new_task->next = free_list;
        free_list = new_task;
        spin_unlock(&scheduler_lock, stat);
        return -1;
    }

    new_task->sleep_until_tick = 0;
    new_task->notify_val = 0;
    new_task->notify_state = 0;
    
    /* Time Slice & VRuntime Init */
    if (weight == 0) {
        weight = 1;
    }
    new_task->weight = weight;
    new_task->time_slice = weight * BASE_SLICE_TICKS;
    new_task->vruntime = _get_min_vruntime(); /* Start fair */
    new_task->heap_index = -1;

    /* Add to ready heap immediately */
    _heap_insert(new_task);

    /* Set stack canary at the bottom for overflow detection */
    stack_base[0] = STACK_CANARY;

    /* Increment active task count */
    task_count++;

    spin_unlock(&scheduler_lock, stat);

    return new_task->task_id;
}

/* Delete a task */
int32_t task_delete(uint16_t task_id) {
    if (task_id == 0) {
        return -1;
    }

    /* Check if trying to delete self */
    if (task_current && task_current->task_id == task_id) {
        task_exit();
        return 0; /* Unreachable */
    }

    uint32_t stat = spin_lock(&scheduler_lock);

    task_t *task_to_delete = NULL;
    task_t *t = task_list;
    for (uint32_t i = 0; i < MAX_TASKS; ++i, ++t) {
        if (t->task_id == task_id) {
            task_to_delete = t;
            break;
        }
    }

    if (task_to_delete == NULL || task_to_delete->is_idle) {
        spin_unlock(&scheduler_lock, stat);
        return -1;
    }

    /* Remove from heap if it's there */
    _heap_remove(task_to_delete);
    
    /* Remove from sleep list if it's there */
    if (task_to_delete->sleep_until_tick > 0) {
        _remove_from_sleep_list(task_to_delete);
    }

    if (task_to_delete->task_id > 0 && task_to_delete->task_id <= MAX_TASKS) {
        MARK_ID_FREE(task_to_delete->task_id);
    }

    /* Mark task as zombie. Its stack will be freed in garbage collection. */
    task_to_delete->state = TASK_ZOMBIE;
    task_to_delete->task_id = 0;
    
    /* Add to zombie list for GC */
    task_to_delete->next = zombie_list;
    zombie_list = task_to_delete;

    spin_unlock(&scheduler_lock, stat);

    return 0;
}

/* Task voluntarily exits */
void task_exit(void) {
    uint32_t stat = spin_lock(&scheduler_lock);
    
    if (task_current != NULL) {
        if (task_current->task_id > 0 && task_current->task_id <= MAX_TASKS) {
            MARK_ID_FREE(task_current->task_id);
        }

        task_current->state = TASK_ZOMBIE;
        task_current->task_id = 0;
        
        /* Add to zombie list for GC */
        task_current->next = zombie_list;
        zombie_list = task_current;
    }
    
    spin_unlock(&scheduler_lock, stat);

    /* Yield immediately to allow scheduler to switch away */
    while(1) {
        platform_yield();
    }
}

/* Called by Platform Context Switcher to pick next task */ 
void schedule_next_task(void) {
    uint32_t stat = spin_lock(&scheduler_lock);

    if (task_count == 0 && idle_task == NULL) {
        spin_unlock(&scheduler_lock, stat);
        return;
    }

    /* Handle Current Task */
    if (task_current && task_current->state == TASK_RUNNING) {
        /* It was running, so it yielded or was preempted. */
        task_current->state = TASK_READY;

        if (!task_current->is_idle) {
            /* Calculate actual ticks consumed */
            uint32_t max_slice = task_current->weight * BASE_SLICE_TICKS;
            uint32_t ticks_ran = max_slice - task_current->time_slice;
            if (ticks_ran == 0) {
                ticks_ran = 1; /* Minimum charge to prevent free yields */
            }

            /* 
             * Update vruntime:
             * vruntime += (ticks_ran * SCALER) / weight
             */
            task_current->vruntime += (uint64_t)(ticks_ran * VRUNTIME_SCALER) / task_current->weight;
            
            /* Replenish Time Slice */
            task_current->time_slice = max_slice;

            /* Put back into heap */
            _heap_insert(task_current);
        }
    }

    /* Pick Next Task from Heap */
    task_t *best = _heap_pop_min();
    
    if (best != NULL) {
        /* Found a user task */
        task_current = best;
        task_current->state = TASK_RUNNING;
        spin_unlock(&scheduler_lock, stat);
        return;
    }

    /* If no READY user task found, schedule IDLE task */
    if (idle_task != NULL && idle_task->state == TASK_READY) {
        task_current = idle_task;
        task_current->state = TASK_RUNNING;

        spin_unlock(&scheduler_lock, stat);
        return;
    }

    /* Fallback: if absolutely nothing is ready (shouldn't happen if idle exists), just stay */
    if (task_current->state == TASK_READY || task_current->state == TASK_RUNNING) {
        /* task_current remains the same */
        task_current->state = TASK_RUNNING;
    } else {
        /* If current is blocked and no idle task, we have a critical failure */
        platform_panic();
    }

    spin_unlock(&scheduler_lock, stat);
}

/* Rearrange active tasks and free zombie stacks */
void task_garbage_collection(void) {
    uint32_t stat = spin_lock(&scheduler_lock);
    
    while (zombie_list != NULL) {
        task_t *t = zombie_list;
        zombie_list = t->next;
        
        /* Free the stack memory */
        if (t->stack_ptr != NULL) {
            allocator_free(t->stack_ptr);
            t->stack_ptr = NULL;
        }
        
        t->state = TASK_UNUSED;
        t->next = free_list;
        free_list = t;
        
        if (task_count > 0) {
            task_count--;
        }
    }

    spin_unlock(&scheduler_lock, stat);
}

/* Check all tasks for stack overflow */
void task_check_stack_overflow(void) {
    uint32_t current_task_overflow = 0;
    uint32_t stat = spin_lock(&scheduler_lock);

    task_t *t = task_list;
    for (uint32_t i = 0; i < MAX_TASKS; ++i, ++t) {
        if (t->state != TASK_UNUSED) {
            /* Use stack_ptr (void*) and cast to check canary */
            uint32_t *stack_base = (uint32_t*)t->stack_ptr;

            if (stack_base != NULL && stack_base[0] != STACK_CANARY) {
                if (t == task_current) {
                    current_task_overflow = 1;
                } else {
                    /* Kill other tasks immediately */
                    uint16_t id_to_delete = t->task_id;
                    /* Only delete active tasks (ZOMBIEs are already dead/ID=0) */
                    if (id_to_delete != 0) {
                        task_delete(id_to_delete);
                    }
                }
            }
        }
    }

    spin_unlock(&scheduler_lock, stat);

    if (current_task_overflow) {
        platform_panic();
    }
}

/* Block a task */
void task_block(task_t *task) {
    if (task == NULL) {
        return;
    }

    uint32_t stat = spin_lock(&scheduler_lock);
    if (task->state != TASK_UNUSED && !task->is_idle) {
        /* If it was READY, remove from heap */
        if (task->state == TASK_READY) {
            _heap_remove(task);
        }
        task->state = TASK_BLOCKED;
    }
    spin_unlock(&scheduler_lock, stat);
}

/* Unblock a task */
void task_unblock(task_t *task) {
    if (task == NULL) {
        return;
    }

    uint32_t stat = spin_lock(&scheduler_lock);
    _unblock_task_locked(task);
    spin_unlock(&scheduler_lock, stat);
}

/* Block current task */
void task_block_current(void) {
    uint32_t stat = spin_lock(&scheduler_lock);
    if (task_current && task_current->state != TASK_UNUSED && !task_current->is_idle) {
        /* Current task is RUNNING, so it's not in the heap. Just set state. */
        task_current->state = TASK_BLOCKED;
    }
    spin_unlock(&scheduler_lock, stat);
    
    platform_yield();
}

/* Update wake-up time and add to sleep list */
int task_sleep_ticks(uint32_t ticks)
{
    if (ticks == 0 || task_current == NULL) {
        return -1;
    }

    uint32_t stat = spin_lock(&scheduler_lock);

    /* Remove from sleep list if already there */
    _remove_from_sleep_list(task_current);

    task_current->sleep_until_tick = (uint32_t)platform_get_ticks() + ticks;

    /* Block task */
    if (task_current->state != TASK_UNUSED && !task_current->is_idle) {
        task_current->state = TASK_BLOCKED;
        /* Not in heap, so no remove needed */
    }

    _insert_into_sleep_list(task_current);

    spin_unlock(&scheduler_lock, stat);

    /* Yield immediately */
    platform_yield();

    return 0;
}

uint32_t task_notify_wait(uint8_t clear_on_exit, uint32_t wait_ticks) {
    uint32_t stat = spin_lock(&scheduler_lock);
    
    /* If no notification pending, block */
    if (task_current->notify_state == 0) {
        if (wait_ticks > 0) {
            /* Set wake-up time if not infinite wait */
            if (wait_ticks != UINT32_MAX) {
                task_current->sleep_until_tick = (uint32_t)platform_get_ticks() + wait_ticks;
                _insert_into_sleep_list(task_current);
            }
            task_current->state = TASK_BLOCKED;
            /* Not in heap */
        } else {
            spin_unlock(&scheduler_lock, stat);
            return 0;
        }
    }
    
    spin_unlock(&scheduler_lock, stat);
    
    if (task_current->state == TASK_BLOCKED) {
        platform_yield();
    }
    
    /* Woke up */
    stat = spin_lock(&scheduler_lock);
    uint32_t val = task_current->notify_val;
    if (clear_on_exit) {
        task_current->notify_state = 0;
        task_current->notify_val = 0;
    }
    /* Clear sleep tick in case we woke up early due to notification */
    if (wait_ticks > 0 && wait_ticks != UINT32_MAX) {
        task_current->sleep_until_tick = 0;
    }
    spin_unlock(&scheduler_lock, stat);
    
    return val;
}

void task_notify(uint16_t task_id, uint32_t value) {
    uint32_t stat = spin_lock(&scheduler_lock);
    
    task_t *t = task_list;
    for (uint32_t i = 0; i < MAX_TASKS; ++i, ++t) {
        if (t->state != TASK_UNUSED && t->task_id == task_id) {
            t->notify_val |= value;
            t->notify_state = 1;
            _unblock_task_locked(t);
            break;
        }
    }
    spin_unlock(&scheduler_lock, stat);
}

/* Process System Tick (Called by ISR) */
uint32_t scheduler_tick(void)
{
    uint32_t stat = spin_lock(&scheduler_lock);
    uint32_t need_reschedule = 0;
    
    uint32_t current_ticks = (uint32_t)platform_get_ticks();

    _process_sleep_list(current_ticks);

    /* 2. Update Time Slice for Current Task */
    if (task_current && task_current->state == TASK_RUNNING && !task_current->is_idle) {
        if (task_current->time_slice > 0) {
            task_current->time_slice--;
        }

        /* If slice expired, trigger switch */
        if (task_current->time_slice == 0) {
            need_reschedule = 1;
        }
    }

    /* Check if we need to preempt current task for a higher priority one (lower vruntime) */
    uint64_t min_v = _get_min_vruntime();
    if (task_current && task_current->vruntime > min_v) {
        need_reschedule = 1;
    }

    spin_unlock(&scheduler_lock, stat);
    return need_reschedule;
}

/* Get the handle of the currently running task */
void *task_get_current(void) {
    return (void *)task_current;
}

/* Change the task state. */
void task_set_state(task_t *t, task_state_t state) {
    uint32_t stat = spin_lock(&scheduler_lock);

    /* Handle Heap Management */
    if (t->state == TASK_READY && state != TASK_READY) {
        _heap_remove(t);
    }
    
    /* Remove if it was sleeping (regardless of target state) */
    if (t->sleep_until_tick > 0) {
        _remove_from_sleep_list(t);
        t->sleep_until_tick = 0;
    }

    task_state_t old_state = (task_state_t)t->state;
    t->state = state;
    
    if (state == TASK_READY) {
        uint64_t min_v = _get_min_vruntime();
        if (t->vruntime < min_v) {
            t->vruntime = min_v;
        }
        _heap_insert(t);
    } else if (state == TASK_ZOMBIE && old_state != TASK_ZOMBIE) {
        /* Add to zombie list for GC */
        if (t->task_id > 0 && t->task_id <= MAX_TASKS) {
            MARK_ID_FREE(t->task_id);
        }
        t->task_id = 0;
        
        t->next = zombie_list;
        zombie_list = t;
    }

    spin_unlock(&scheduler_lock, stat);
}

/* Atomically get the task state */
task_state_t task_get_state_atomic(task_t *t) {
    return (task_state_t)t->state;
}

/* Get the unique ID of a task */
uint16_t task_get_id(task_t *t) {
    return t->task_id;
}

/* Get the weight of a task */
uint8_t task_get_weight(task_t *t) {
    return t->weight;
}

/* Get the remaining time slice of a task */
uint32_t task_get_time_slice(task_t *t) {
    return t->time_slice;
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