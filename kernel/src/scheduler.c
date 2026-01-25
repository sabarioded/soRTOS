#include "scheduler.h"
#include <stdint.h>
#include "utils.h"
#include "allocator.h"
#include "platform.h"
#include "arch_ops.h"
#include "spinlock.h"
#include "logger.h"

/* Modular arithmetic comparison for vruntime to handle overflow/wrap-around */
#define VRUNTIME_LT(a, b)   ((int64_t)((a) - (b)) < 0)

/* Bitmap helper macros */
#define BITMAP_IDX(id)  ((size_t)((id) - 1) / 64)
#define BITMAP_BIT(id)  ((size_t)((id) - 1) % 64)
#define BITMAP_SIZE     ((MAX_TASKS + 63) / 64)
#define IS_ID_USED(id)   (g_sched.id_bitmap[BITMAP_IDX(id)] & (1ULL << BITMAP_BIT(id)))
#define MARK_ID_USED(id) (g_sched.id_bitmap[BITMAP_IDX(id)] |= (1ULL << BITMAP_BIT(id)))
#define MARK_ID_FREE(id) (g_sched.id_bitmap[BITMAP_IDX(id)] &= ~(1ULL << BITMAP_BIT(id)))

typedef struct task_struct {
    void            *psp;               /* Platform-agnostic Stack Pointer (Current Top) */
    void            *stack_ptr;         /* Pointer to start of allocated memory */
    struct task_struct *next;           /* Link for Sleep/Free/Zombie lists */
    size_t          stack_size;         /* Size of allocated stack in bytes */
    wait_node_t     wait_node;          /* Generic wait node for blocking */
    uint64_t        vruntime;           /* Virtual runtime (fairness metric) */
    uint64_t        total_cpu_ticks;
    uint64_t        last_switch_tick;

    uint32_t        sleep_until_tick;   /* System Tick count when task should wake */
    uint32_t        time_slice;         /* Remaining ticks in current slice */
    uint32_t        notify_val;         /* Task notification value */
    uint32_t        event_mask;         /* Event Group: bits to wait for / result */

    int16_t         heap_index;         /* Index in ready_heap (-1 if not in heap) */
    uint16_t        task_id;            /* Unique Task ID */

    uint8_t         state;              /* Current task state */
    uint8_t         is_idle;            /* Flag for idle task identification */
    uint8_t         weight;             /* User assigned weight */
    uint8_t         base_weight;        /* Base weight (before priority inheritance) */
    uint8_t         notify_state;       /* 0: None, 1: Pending */
    uint8_t         event_flags;        /* Event Group: wait_all, clear_on_exit, satisfied */
    uint8_t         cpu_id;             /* CPU affinity */
} task_t;

typedef struct {
    task_t          pool[MAX_TASKS];        /* Storage for tasks */
    task_t          *free_list;
    task_t          *zombie_list;
    uint64_t        id_bitmap[BITMAP_SIZE];
    uint32_t        count;
    uint32_t        next_cpu;
    spinlock_t      lock;
} scheduler_global_t;

typedef struct {
    task_t          *ready_heap[MAX_TASKS]; /* Min-Heap for Ready Tasks */
    task_t          *sleep_list;
    task_t          *idle_task;
    task_t          *curr;
    uint32_t        heap_size;
    spinlock_t      lock;
} scheduler_cpu_t;

static scheduler_global_t g_sched;
static scheduler_cpu_t    cpu_sched[MAX_CPUS];

/**
 * The heap is implemented by a tree flattened into an array.
 * Parent: (index-1)/2
 * Left child: (2 * index) + 1
 * Right child (2 * index) + 2
 */

/* Swap two tasks in the heap and update their index tracking */
static inline void _swap_tasks(scheduler_cpu_t *ctx, uint32_t i, uint32_t j) {
    task_t *temp = ctx->ready_heap[i];
    ctx->ready_heap[i] = ctx->ready_heap[j];
    ctx->ready_heap[j] = temp;
    
    /* Update indices in task structs */
    ctx->ready_heap[i]->heap_index = i;
    ctx->ready_heap[j]->heap_index = j;
}

/* Bubble up an element to maintain min-heap property */
static void _heap_up(scheduler_cpu_t *ctx, uint32_t index) {
    while (index > 0) {
        uint32_t parent = (index - 1) / 2;
        if (VRUNTIME_LT(ctx->ready_heap[index]->vruntime, ctx->ready_heap[parent]->vruntime)) {
            _swap_tasks(ctx, index, parent);
            index = parent;
        } else {
            break;
        }
    }
}

/* Bubble down an element to maintain min-heap property */
static void _heap_down(scheduler_cpu_t *ctx, uint32_t index) {
    while (1) {
        uint32_t left = 2 * index + 1; /* left child */
        uint32_t right = 2 * index + 2; /* right child */
        uint32_t smallest = index;

        if (left < ctx->heap_size &&
            VRUNTIME_LT(ctx->ready_heap[left]->vruntime, ctx->ready_heap[smallest]->vruntime)) {
            smallest = left;
        }
        if (right < ctx->heap_size && 
            VRUNTIME_LT(ctx->ready_heap[right]->vruntime, ctx->ready_heap[smallest]->vruntime)) {
            smallest = right;
        }

        if (smallest != index) {
            _swap_tasks(ctx, index, smallest);
            index = smallest;
        } else {
            break;
        }
    }
}

/* Insert a task into the priority queue */
static void _heap_insert(scheduler_cpu_t *ctx, task_t *t) {
    if (ctx->heap_size >= MAX_TASKS) {
        return;
    }

    t->heap_index = ctx->heap_size;
    ctx->ready_heap[ctx->heap_size] = t;
    ctx->heap_size++;
    _heap_up(ctx, ctx->heap_size - 1);
}

/* Extract the task with the lowest vruntime value (highest priority) */
static task_t* _heap_pop_min(scheduler_cpu_t *ctx) {
    if (ctx->heap_size == 0) {
        return NULL;
    }
    
    task_t *min = ctx->ready_heap[0];
    min->heap_index = -1; /* Mark as not in heap */
    
    ctx->heap_size--;
    if (ctx->heap_size > 0) {
        ctx->ready_heap[0] = ctx->ready_heap[ctx->heap_size];
        ctx->ready_heap[0]->heap_index = 0;
        _heap_down(ctx, 0);
    }
    return min;
}

/* Remove a specific task from the middle of the heap */
static void _heap_remove(scheduler_cpu_t *ctx, task_t *t) {
    if (t->heap_index == -1 || t->heap_index >= (int32_t)ctx->heap_size) {
        return;
    }
    
    uint32_t index = (uint32_t)t->heap_index;
    t->heap_index = -1;
    
    ctx->heap_size--;
    if (index < ctx->heap_size) {
        /* Move last element to this spot */
        ctx->ready_heap[index] = ctx->ready_heap[ctx->heap_size];
        ctx->ready_heap[index]->heap_index = index;
        
        /* Rebalance (could go up or down) */
        _heap_up(ctx, index);
        _heap_down(ctx, index);
    }
}

/* Get the minimum vruntime value currently in the system */
static inline uint64_t _get_min_vruntime(scheduler_cpu_t *ctx) {
    if (ctx->heap_size > 0) {
        return ctx->ready_heap[0]->vruntime;
    }
    return (ctx->curr) ? ctx->curr->vruntime : 0;
}

/* Remove task from sleep list */
static void _remove_from_sleep_list(scheduler_cpu_t *ctx, task_t *task) {
    if (task == NULL) {
        return;
    }

    /* Removing head */
    if (ctx->sleep_list == task) {
        ctx->sleep_list = task->next;
        task->next = NULL;
        return;
    }

    /* Removing from middle */
    task_t *curr = ctx->sleep_list;
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
static void _insert_into_sleep_list(scheduler_cpu_t *ctx, task_t *task) {
    if (task == NULL) {
        return;
    }

    /* If empty list or new head */
    if (ctx->sleep_list == NULL || task->sleep_until_tick < ctx->sleep_list->sleep_until_tick) {
        task->next = ctx->sleep_list;
        ctx->sleep_list = task;
    } else {
        /* Otherwise insert in the middle */
        task_t *curr = ctx->sleep_list;
        while (curr->next != NULL && curr->next->sleep_until_tick < task->sleep_until_tick) {
            curr = curr->next;
        }
        task->next = curr->next;
        curr->next = task;
    }
}

static void inline _wake_sleeping_task(scheduler_cpu_t *ctx, task_t *task) {
     task->state = TASK_READY;
     /* Insert into heap */
     uint64_t min_v = _get_min_vruntime(ctx);
     if (VRUNTIME_LT(task->vruntime, min_v)) {
         task->vruntime = min_v;
     }
     _heap_insert(ctx, task);
}

static void _process_sleep_list(scheduler_cpu_t *ctx, uint32_t current_ticks) {
    task_t *curr = ctx->sleep_list;

    /* Process tasks at head of the list whose time has come */
    while (curr != NULL && (int32_t)(current_ticks - curr->sleep_until_tick) >= 0) {
        _remove_from_sleep_list(ctx, curr);
        _wake_sleeping_task(ctx, curr);
        curr = ctx->sleep_list;  /* Peek at new head */
    }
}

/* Unblock a task (Assumes Lock Held) */
static void _unblock_task_locked(scheduler_cpu_t *ctx, task_t *task) {
    if (task->state == TASK_BLOCKED || task->state == TASK_SLEEPING) {
        /* Remove from sleep list if it was waiting with timeout */
        if (task->state == TASK_SLEEPING) {
            _remove_from_sleep_list(ctx, task);
            task->sleep_until_tick = 0;
        }

        task->state = TASK_READY;
        
        /* Insert to heap */
        uint64_t min_v = _get_min_vruntime(ctx);
        if (VRUNTIME_LT(task->vruntime, min_v)) {
            task->vruntime = min_v;
        }
        _heap_insert(ctx, task);
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
static void _task_create_idle(scheduler_cpu_t *ctx) {
    if (ctx->idle_task != NULL) {
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
        if(g_sched.pool[i].task_id == task_id) {
            ctx->idle_task = &g_sched.pool[i];
            ctx->idle_task->is_idle = 1;
            /* Force affinity to this CPU */
            ctx->idle_task->cpu_id = arch_get_cpu_id();
            
            /* Idle tasks should not be in the ready heap */
            _heap_remove(ctx, ctx->idle_task);
            return;
        }
    }
}

/* Rearrange active tasks and free zombie stacks (Internal: Lock must be held) */
static void _task_garbage_collection_locked(void) {
    while (g_sched.zombie_list != NULL) {
        task_t *t = g_sched.zombie_list;
        g_sched.zombie_list = t->next;
        
        /* Free the stack memory */
        if (t->stack_ptr != NULL && allocator_is_heap_pointer(t->stack_ptr)) {
            /* Only free stacks that actually belong to the heap */
            allocator_free(t->stack_ptr);
            t->stack_ptr = NULL;
        }
        
        t->state = TASK_UNUSED;
        t->next = g_sched.free_list;
        g_sched.free_list = t;
        
        if (g_sched.count > 0) {
            g_sched.count--;
        }
    }
}

/* Delete a task (Internal: Lock must be held) */
static int32_t _task_delete_locked(uint16_t task_id) {
    task_t *task_to_delete = NULL;
    task_t *t = g_sched.pool;
    for (uint32_t i = 0; i < MAX_TASKS; ++i, ++t) {
        if (t->task_id == task_id) {
            task_to_delete = t;
            break;
        }
    }

    if (task_to_delete == NULL || task_to_delete->is_idle) {
        return -1;
    }

    /* We need to lock the CPU that owns this task to remove it from lists */
    uint32_t cpu = task_to_delete->cpu_id;
    if (cpu >= MAX_CPUS) {
        return -1;
    }
    
    uint32_t cpu_flags = spin_lock(&cpu_sched[cpu].lock);

    /* Remove from heap if it's there */
    _heap_remove(&cpu_sched[cpu], task_to_delete);
    
    /* Remove from sleep list if it's there */
    if (task_to_delete->sleep_until_tick > 0) {
        _remove_from_sleep_list(&cpu_sched[cpu], task_to_delete);
    }

    /* Mark as ZOMBIE inside lock to prevent resurrection by task_notify */
    task_to_delete->state = TASK_ZOMBIE;
    
    spin_unlock(&cpu_sched[cpu].lock, cpu_flags);

    if (task_to_delete->task_id > 0 && task_to_delete->task_id <= MAX_TASKS) {
        MARK_ID_FREE(task_to_delete->task_id);
    }

    /* Mark task as zombie. Its stack will be freed in garbage collection. */
    task_to_delete->task_id = 0;
    
    /* Add to zombie list for GC */
    task_to_delete->next = g_sched.zombie_list;
    g_sched.zombie_list = task_to_delete;
    
    return 0;
}

/* Initialize the scheduler */
void scheduler_init(void) {
    utils_memset(&g_sched, 0, sizeof(g_sched));
    utils_memset(cpu_sched, 0, sizeof(cpu_sched));

    /* Initialize Free List */
    for (uint32_t i = 0; i < MAX_TASKS - 1; ++i) {
        g_sched.pool[i].next = &g_sched.pool[i+1];
    }
    g_sched.pool[MAX_TASKS - 1].next = NULL;
    g_sched.free_list = &g_sched.pool[0];
    spinlock_init(&g_sched.lock);
    
    for (int i = 0; i < MAX_CPUS; i++) {
        spinlock_init(&cpu_sched[i].lock);
    }
    
#if LOG_ENABLE
    logger_log("Scheduler Init", 0, 0);
#endif
}

/* Start the scheduler */
void scheduler_start(void) {
    uint32_t cpu = arch_get_cpu_id();
    scheduler_cpu_t *ctx = &cpu_sched[cpu];

    if (g_sched.count == 0 || ctx->idle_task == NULL) {
        _task_create_idle(ctx); 
    }

    /* Pick the task to start with */
    task_t *min_vrt_task = _heap_pop_min(ctx);
    
    if (min_vrt_task == NULL) {
        /* If heap is empty, try to run idle task directly */
        if (ctx->idle_task != NULL) {
            ctx->curr = ctx->idle_task;
            ctx->curr->state = TASK_RUNNING;
            platform_start_scheduler((size_t)ctx->curr->psp);
            return; /* Should not be reached on real hardware, but needed for tests */
        }
        platform_panic();
        return;
    }

    ctx->curr = min_vrt_task;
    
#if LOG_ENABLE
    logger_log("Scheduler Start", 0, 0);
#endif
    /* Mark first task as running */
    ctx->curr->state = TASK_RUNNING;
    ctx->curr->last_switch_tick = (uint64_t)platform_get_ticks();
    
    /* Hand over control to the platform scheduler start */
    platform_start_scheduler((size_t)ctx->curr->psp);
}

/* Create a new task */
int32_t task_create(void (*task_func)(void *), void *arg, size_t stack_size_bytes, uint8_t weight) {
    if (task_func == NULL) {
        return -1;
    }

    if (stack_size_bytes < STACK_MIN_SIZE_BYTES || stack_size_bytes > STACK_MAX_SIZE_BYTES) {
        return -1;
    }

    /* Align stack size request to platform alignment boundary */
    size_t align_mask = (size_t)(PLATFORM_STACK_ALIGNMENT - 1);
    stack_size_bytes = (stack_size_bytes + align_mask) & ~align_mask;

    uint32_t stat = spin_lock(&g_sched.lock);

    if (g_sched.free_list == NULL) {
        /* Try to reclaim zombie tasks immediately */
        _task_garbage_collection_locked();
    }

    if (g_sched.free_list == NULL) {
        spin_unlock(&g_sched.lock, stat);
        return -1;
    }
    task_t *new_task = g_sched.free_list;
    g_sched.free_list = g_sched.free_list->next;
    new_task->next = NULL; /* Detach from free list */

    /* Allocate stack from heap */
    uint32_t *stack_base = (uint32_t*)allocator_malloc(stack_size_bytes);
    if (stack_base == NULL) {
        /* Allocation failed: Return task slot, run GC, and retry once */
        new_task->next = g_sched.free_list;
        g_sched.free_list = new_task;
        
        _task_garbage_collection_locked();
                
        /* Re-acquire task slot */
        if (g_sched.free_list == NULL) {
            spin_unlock(&g_sched.lock, stat);
            return -1;
        }
        new_task = g_sched.free_list;
        g_sched.free_list = g_sched.free_list->next;
        new_task->next = NULL;

        /* Retry allocation */
        stack_base = (uint32_t*)allocator_malloc(stack_size_bytes);
        if (stack_base == NULL) {
            /* Failed again: Rollback and exit */
            new_task->next = g_sched.free_list;
            g_sched.free_list = new_task;
            spin_unlock(&g_sched.lock, stat);
            return -1;
        }
    }

    /* Calculate Top of Stack (highest address). Remember stack grows down */
    uint32_t *stack_end = (uint32_t*)((uint8_t*)stack_base + stack_size_bytes);

    /* Initialize task structure */
    new_task->stack_ptr     = stack_base;     /* Save base for free */
    new_task->stack_size    = stack_size_bytes;
    
    /* Initialize Task Context (Architecture Specific) */
    new_task->psp = arch_initialize_stack(stack_end, task_func, arg, task_exit);
    
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
        new_task->next = g_sched.free_list;
        g_sched.free_list = new_task;
        spin_unlock(&g_sched.lock, stat);
#if LOG_ENABLE
        logger_log("Task Create Fail", 0, 0);
#endif
        return -1;
    }

    new_task->sleep_until_tick = 0;
    new_task->notify_val = 0;
    new_task->notify_state = 0;
    new_task->event_mask = 0;
    new_task->event_flags = 0;
    
    /* Assign CPU affinity (Round Robin) */
    new_task->cpu_id = g_sched.next_cpu;
    g_sched.next_cpu = (g_sched.next_cpu + 1) % MAX_CPUS;
    
    /* Time Slice & VRuntime Init */
    if (weight == 0) {
        weight = 1;
    }
    new_task->weight = weight;
    new_task->base_weight = weight;
    new_task->time_slice = weight * BASE_SLICE_TICKS;
    
    /* Lock the specific CPU scheduler to insert */
    scheduler_cpu_t *ctx = &cpu_sched[new_task->cpu_id];
    uint32_t cpu_stat = spin_lock(&ctx->lock);
    
    new_task->vruntime = _get_min_vruntime(ctx); /* Start fair */
    new_task->heap_index = -1;
    new_task->total_cpu_ticks = 0;
    new_task->last_switch_tick = 0;

    /* Add to ready heap immediately */
    _heap_insert(ctx, new_task);
    
    spin_unlock(&ctx->lock, cpu_stat);

    /* Set stack canary at the bottom for overflow detection */
    stack_base[0] = STACK_CANARY;

    /* Increment active task count */
    g_sched.count++;

    spin_unlock(&g_sched.lock, stat);

#if LOG_ENABLE
    logger_log("Task Create ID:%u", new_task->task_id, 0);
#endif
    return new_task->task_id;
}

/* Create a new task with statically allocated stack */
int32_t task_create_static(void (*task_func)(void *), 
                           void *arg, 
                           void *stack_buffer, 
                           size_t stack_size_bytes, 
                           uint8_t weight) {

    if (task_func == NULL || stack_buffer == NULL) {
        return -1;
    }

    /* Ensure the provided stack is NOT part of the managed heap */
    if (allocator_is_heap_pointer(stack_buffer)) {
        return -1;
    }

    if (stack_size_bytes < STACK_MIN_SIZE_BYTES) {
        return -1;
    }

    /* Align stack buffer to platform alignment boundary */
    uintptr_t addr = (uintptr_t)stack_buffer;
    uintptr_t aligned_addr = (addr + PLATFORM_STACK_ALIGNMENT - 1) & ~(PLATFORM_STACK_ALIGNMENT - 1);
    size_t offset = aligned_addr - addr;
    
    /* Ensure alignment didn't shrink size below minimum */
    if (offset >= stack_size_bytes || (stack_size_bytes - offset) < STACK_MIN_SIZE_BYTES) {
        return -1;
    }
    stack_size_bytes -= offset;
    
    uint32_t *stack_base = (uint32_t*)aligned_addr;

    uint32_t stat = spin_lock(&g_sched.lock);

    if (g_sched.free_list == NULL) {
        _task_garbage_collection_locked();
    }

    if (g_sched.free_list == NULL) {
        spin_unlock(&g_sched.lock, stat);
        return -1;
    }
    
    task_t *new_task = g_sched.free_list;
    g_sched.free_list = g_sched.free_list->next;
    new_task->next = NULL;

    /* Calculate Top of Stack */
    uint32_t *stack_end = (uint32_t*)((uint8_t*)stack_base + stack_size_bytes);

    /* Initialize task structure */
    new_task->stack_ptr     = stack_base;
    new_task->stack_size    = stack_size_bytes;
    new_task->psp           = arch_initialize_stack(stack_end, task_func, arg, task_exit);
    new_task->state         = TASK_READY;
    new_task->is_idle       = 0;

    new_task->task_id = 0;
    for (uint16_t id = 1; id <= MAX_TASKS; ++id) {
        if (!IS_ID_USED(id)) {
            new_task->task_id = id;
            MARK_ID_USED(id);
            break;
        }
    }

    if (new_task->task_id == 0) {
        /* Return task to free list */
        new_task->next = g_sched.free_list;
        g_sched.free_list = new_task;
        spin_unlock(&g_sched.lock, stat);
        return -1;
    }

    new_task->sleep_until_tick = 0;
    new_task->notify_val = 0;
    new_task->notify_state = 0;
    new_task->event_mask = 0;
    new_task->event_flags = 0;
    new_task->cpu_id = g_sched.next_cpu;
    g_sched.next_cpu = (g_sched.next_cpu + 1) % MAX_CPUS;
    
    if (weight == 0) {
        weight = 1;
    }
    new_task->weight = weight;
    new_task->base_weight = weight;
    new_task->time_slice = weight * BASE_SLICE_TICKS;
    
    scheduler_cpu_t *ctx = &cpu_sched[new_task->cpu_id];
    uint32_t cpu_stat = spin_lock(&ctx->lock);
    
    new_task->vruntime = _get_min_vruntime(ctx);
    new_task->heap_index = -1;
    new_task->total_cpu_ticks = 0;
    new_task->last_switch_tick = 0;

    _heap_insert(ctx, new_task);
    
    spin_unlock(&ctx->lock, cpu_stat);
    
    /* Set stack canary */
    stack_base[0] = STACK_CANARY;

    g_sched.count++;
    spin_unlock(&g_sched.lock, stat);

    return new_task->task_id;
}

/* Delete a task */
int32_t task_delete(uint16_t task_id) {
    if (task_id == 0) {
        return -1;
    }

    task_t *curr = (task_t*)task_get_current();

    /* Check if trying to delete self */
    if (curr && curr->task_id == task_id) {
        task_exit();
        return 0; /* Unreachable */
    }

    uint32_t stat = spin_lock(&g_sched.lock);

    int32_t res = _task_delete_locked(task_id);

    spin_unlock(&g_sched.lock, stat);

#if LOG_ENABLE
    if (res == 0) logger_log("Task Delete ID:%u", task_id, 0);
#endif
    return res;
}

/* Task voluntarily exits */
void task_exit(void) {
    task_t *curr = (task_t*)task_get_current();
    if (curr == NULL) {
        return;
    }
    
    uint32_t stat = spin_lock(&g_sched.lock);
    
    if (curr->task_id > 0 && curr->task_id <= MAX_TASKS) {
        MARK_ID_FREE(curr->task_id);
    }

    curr->state = TASK_ZOMBIE;
    curr->task_id = 0;
        
    /* Add to zombie list for GC */
    curr->next = g_sched.zombie_list;
    g_sched.zombie_list = curr;
    
    spin_unlock(&g_sched.lock, stat);

    /* Yield immediately to allow scheduler to switch away */
    while(1) {
        platform_yield();
    }
}

/* Called by Platform Context Switcher to pick next task */ 
void schedule_next_task(void) {
    uint32_t cpu = arch_get_cpu_id();
    scheduler_cpu_t *ctx = &cpu_sched[cpu];
    
    uint32_t stat = spin_lock(&ctx->lock);

    uint64_t now = (uint64_t)platform_get_ticks();

    /* Account for the task that just ran */
    if (ctx->curr) {
        ctx->curr->total_cpu_ticks += (now - ctx->curr->last_switch_tick);
    }

    if (g_sched.count == 0 && ctx->idle_task == NULL) {
        spin_unlock(&ctx->lock, stat);
        return;
    }

    /* Handle Current Task */
    if (ctx->curr && ctx->curr->state == TASK_RUNNING) {
        /* It was running, so it yielded or was preempted. */
        ctx->curr->state = TASK_READY;

        if (!ctx->curr->is_idle) {
            /* Calculate actual ticks consumed */
            uint32_t max_slice = ctx->curr->weight * BASE_SLICE_TICKS;
            uint32_t ticks_ran = max_slice - ctx->curr->time_slice;
            if (ticks_ran == 0) {
                ticks_ran = 1; /* Minimum charge to prevent free yields */
            }

            /* 
             * Update vruntime:
             * vruntime += (ticks_ran * SCALER) / weight
             */
            ctx->curr->vruntime += (uint64_t)(ticks_ran * VRUNTIME_SCALER) / ctx->curr->weight;
            
            /* Replenish Time Slice */
            ctx->curr->time_slice = max_slice;

            /* Put back into heap */
            _heap_insert(ctx, ctx->curr);
        }
    }

    /* Pick Next Task from Heap */
    task_t *best = _heap_pop_min(ctx);
    
    if (best != NULL) {
        /* Found a user task */
        ctx->curr = best;
        ctx->curr->state = TASK_RUNNING;
        ctx->curr->last_switch_tick = now;
        spin_unlock(&ctx->lock, stat);
        return;
    }

    /* If no READY user task found, schedule IDLE task */
    if (ctx->idle_task != NULL && ctx->idle_task->state == TASK_READY) {
        ctx->curr = ctx->idle_task;
        ctx->curr->state = TASK_RUNNING;
        ctx->curr->last_switch_tick = now;

        spin_unlock(&ctx->lock, stat);
        return;
    }

    /* Fallback: if absolutely nothing is ready (shouldn't happen if idle exists), just stay */
    if (ctx->curr->state == TASK_READY || ctx->curr->state == TASK_RUNNING) {
        /* task_current remains the same */
        ctx->curr->state = TASK_RUNNING;
        ctx->curr->last_switch_tick = now;
    } else {
        /* If current is blocked and no idle task, we have a critical failure */
        platform_panic();
    }

    spin_unlock(&ctx->lock, stat);
}

/* Rearrange active tasks and free zombie stacks */
void task_garbage_collection(void) {
    uint32_t stat = spin_lock(&g_sched.lock);
    
    _task_garbage_collection_locked();
    
    spin_unlock(&g_sched.lock, stat);
}

/* Check all tasks for stack overflow */
void task_check_stack_overflow(void) {
    uint32_t current_task_overflow = 0;
    uint32_t stat = spin_lock(&g_sched.lock);

    task_t *t = g_sched.pool;
    task_t *curr = (task_t*)task_get_current();
    
    for (uint32_t i = 0; i < MAX_TASKS; ++i, ++t) {
        if (t->state != TASK_UNUSED) {
            /* Use stack_ptr and cast to check canary */
            uint32_t *stack_base = (uint32_t*)t->stack_ptr;

            if (stack_base != NULL && stack_base[0] != STACK_CANARY) {
                if (t == curr) {
#if LOG_ENABLE
                    logger_log("Stack Overflow! ID:%u", t->task_id, 0);
#endif
                    current_task_overflow = 1;
                } else {
                    /* Kill other tasks immediately */
                    uint16_t id_to_delete = t->task_id;
                    /* Only delete active tasks (ZOMBIEs are already dead/ID=0) */
                    if (id_to_delete != 0) {
                        _task_delete_locked(id_to_delete);
                    }
                }
            }
        }
    }

    spin_unlock(&g_sched.lock, stat);

    if (current_task_overflow) {
        platform_panic();
    }
}

/* Block a task */
void task_block(task_t *task) {
    if (task == NULL) {
        return;
    }

    uint32_t cpu = task->cpu_id;
    if (cpu >= MAX_CPUS) {
        return;
    }
    
    uint32_t stat = spin_lock(&cpu_sched[cpu].lock);
    
    if (task->state != TASK_UNUSED && !task->is_idle) {
        /* If it was READY, remove from heap */
        if (task->state == TASK_READY) {
            _heap_remove(&cpu_sched[cpu], task);
        }
        task->state = TASK_BLOCKED;
    }
    spin_unlock(&cpu_sched[cpu].lock, stat);
}

/* Unblock a task */
void task_unblock(task_t *task) {
    if (task == NULL) {
        return;
    }

    uint32_t cpu = task->cpu_id;
    if (cpu >= MAX_CPUS) {
        return;
    }

    uint32_t stat = spin_lock(&cpu_sched[cpu].lock);
    _unblock_task_locked(&cpu_sched[cpu], task);
    spin_unlock(&cpu_sched[cpu].lock, stat);
}

/* Block current task */
void task_block_current(void) {
    uint32_t cpu = arch_get_cpu_id();
    scheduler_cpu_t *ctx = &cpu_sched[cpu];
    
    uint32_t stat = spin_lock(&ctx->lock);
    if (ctx->curr && ctx->curr->state != TASK_UNUSED && !ctx->curr->is_idle) {
        /* Current task is RUNNING, so it's not in the heap. Just set state. */
        ctx->curr->state = TASK_BLOCKED;
    }
    spin_unlock(&ctx->lock, stat);
    
    platform_yield();
}

/* Update wake-up time and add to sleep list */
int task_sleep_ticks(uint32_t ticks)
{
    task_t *curr = (task_t*)task_get_current();
    if (ticks == 0 || curr == NULL) {
        return -1;
    }

    uint32_t cpu = arch_get_cpu_id();
    scheduler_cpu_t *ctx = &cpu_sched[cpu];
    
    uint32_t stat = spin_lock(&ctx->lock);

    if (curr->is_idle) {
        spin_unlock(&ctx->lock, stat);
        return -1;
    }

    /* Remove from sleep list if already there */
    _remove_from_sleep_list(ctx, curr);

    curr->sleep_until_tick = (uint32_t)platform_get_ticks() + ticks;
    curr->state = TASK_SLEEPING;

    _insert_into_sleep_list(ctx, curr);

    spin_unlock(&ctx->lock, stat);

    /* Yield immediately */
    platform_yield();

    return 0;
}

uint32_t task_notify_wait(uint8_t clear_on_exit, uint32_t wait_ticks) {
    task_t *curr = (task_t*)task_get_current();
    if (!curr) {
        return 0;
    }

    uint32_t cpu = arch_get_cpu_id();
    scheduler_cpu_t *ctx = &cpu_sched[cpu];
    
    uint32_t stat = spin_lock(&ctx->lock);
    
    /* If no notification pending, block */
    if (curr->notify_state == 0) {
        if (wait_ticks > 0) {
            /* Set wake-up time if not infinite wait */
            if (wait_ticks != UINT32_MAX) {
                curr->sleep_until_tick = (uint32_t)platform_get_ticks() + wait_ticks;
                _insert_into_sleep_list(ctx, curr);
                curr->state = TASK_SLEEPING;
            } else {
                curr->state = TASK_BLOCKED;
            }
            /* Not in heap */
        } else {
            spin_unlock(&ctx->lock, stat);
            return 0;
        }
    }
    
    spin_unlock(&ctx->lock, stat);
    
    if (curr->state == TASK_BLOCKED || curr->state == TASK_SLEEPING) {
        platform_yield();
    }
    
    /* Woke up */
    stat = spin_lock(&ctx->lock);
    uint32_t val = curr->notify_val;
    if (clear_on_exit) {
        curr->notify_state = 0;
        curr->notify_val = 0;
    }
    /* Clear sleep tick in case we woke up early due to notification */
    if (wait_ticks > 0 && wait_ticks != UINT32_MAX) {
        curr->sleep_until_tick = 0;
    }
    spin_unlock(&ctx->lock, stat);
    
    return val;
}

void task_notify(uint16_t task_id, uint32_t value) {
    if (task_id == 0) {
        return;
    }
    
    task_t *t = g_sched.pool;
    task_t *target = NULL;
    
    for (uint32_t i = 0; i < MAX_TASKS; ++i, ++t) {
        if (t->task_id == task_id) {
            target = t;
            break;
        }
    }
    
    if (target) {
        uint32_t cpu = target->cpu_id;
        if (cpu >= MAX_CPUS) {
            return;
        }
        
        uint32_t stat = spin_lock(&cpu_sched[cpu].lock);
        /* Verify ID match in case of race with deletion/creation */
        if (target->task_id == task_id && target->state != TASK_UNUSED) {
            target->notify_val |= value;
            target->notify_state = 1;
            _unblock_task_locked(&cpu_sched[cpu], target);
        }
        spin_unlock(&cpu_sched[cpu].lock, stat);
    }
}

/* Process System Tick (Called by ISR) */
uint32_t scheduler_tick(void)
{
    uint32_t cpu = arch_get_cpu_id();
    scheduler_cpu_t *ctx = &cpu_sched[cpu];
    
    uint32_t stat = spin_lock(&ctx->lock);
    uint32_t need_reschedule = 0;
    
    uint32_t current_ticks = (uint32_t)platform_get_ticks();

    _process_sleep_list(ctx, current_ticks);

    /* Update Time Slice for Current Task */
    if (ctx->curr && ctx->curr->state == TASK_RUNNING && !ctx->curr->is_idle) {
        if (ctx->curr->time_slice > 0) {
            ctx->curr->time_slice--;
        }

        /* If slice expired, trigger switch */
        if (ctx->curr->time_slice == 0) {
            need_reschedule = 1;
        }
    }

    /* If idle is running and there is any READY task, we must reschedule.
     * Otherwise idle might run forever because it doesn't consume time_slice and
     * its vruntime may not advance. */
    if (ctx->curr && ctx->curr->is_idle && ctx->heap_size > 0) {
        need_reschedule = 1;
    } else {
        /* Check if we need to preempt current task for a higher priority one (lower vruntime) */
        uint64_t min_v = _get_min_vruntime(ctx);
        if (ctx->curr && VRUNTIME_LT(min_v, ctx->curr->vruntime)) {
            need_reschedule = 1;
        }
    }

    spin_unlock(&ctx->lock, stat);
    return need_reschedule;
}

/* Get the handle of the currently running task */
void *task_get_current(void) {
    uint32_t cpu = arch_get_cpu_id();
    return (void *)cpu_sched[cpu].curr;
}

/* Set the currently running task */
void task_set_current(void *task) {
    uint32_t cpu = arch_get_cpu_id();
    scheduler_cpu_t *ctx = &cpu_sched[cpu];
    uint32_t stat = spin_lock(&ctx->lock);

    task_t *old_task = ctx->curr;
    task_t *new_task = (task_t*)task;

    if (old_task && old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
        if (!old_task->is_idle) {
            _heap_insert(ctx, old_task);
        }
    }

    ctx->curr = new_task;

    if (new_task) {
        if (new_task->state == TASK_READY) {
            _heap_remove(ctx, new_task);
        }
        new_task->state = TASK_RUNNING;
    }
    spin_unlock(&ctx->lock, stat);
}

/* Change the task state. */
void task_set_state(task_t *t, task_state_t state) {
    uint32_t cpu = t->cpu_id;
    if (cpu >= MAX_CPUS) {
        return;
    }
    
    uint32_t stat = spin_lock(&cpu_sched[cpu].lock);

    /* Handle Heap Management */
    if (t->state == TASK_READY && state != TASK_READY) {
        _heap_remove(&cpu_sched[cpu], t);
    }
    
    /* Remove if it was sleeping (regardless of target state) */
    if (t->state == TASK_SLEEPING) {
        _remove_from_sleep_list(&cpu_sched[cpu], t);
        t->sleep_until_tick = 0;
    }

    task_state_t old_state = (task_state_t)t->state;
    t->state = state;
    
    if (state == TASK_READY) {
        uint64_t min_v = _get_min_vruntime(&cpu_sched[cpu]);
        if (VRUNTIME_LT(t->vruntime, min_v)) {
            t->vruntime = min_v;
        }
        _heap_insert(&cpu_sched[cpu], t);
    } else if (state == TASK_ZOMBIE && old_state != TASK_ZOMBIE) {
        /* Zombie management is global */
        spin_unlock(&cpu_sched[cpu].lock, stat);
        
        uint32_t g_stat = spin_lock(&g_sched.lock);
        
        /* Add to zombie list for GC */
        if (t->task_id > 0 && t->task_id <= MAX_TASKS) {
            MARK_ID_FREE(t->task_id);
        }
        t->task_id = 0;
        
        t->next = g_sched.zombie_list;
        g_sched.zombie_list = t;
        
        spin_unlock(&g_sched.lock, g_stat);
        return;
    }

    spin_unlock(&cpu_sched[cpu].lock, stat);
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

/* Set the weight of a task */
void task_set_weight(task_t *t, uint8_t weight) {
    if (t) {
        if (weight == 0) {
            weight = 1;
        }
        t->weight = weight;
        t->base_weight = weight;
    }
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
    return &g_sched.pool[index];
}

/* Get the embedded wait node */
wait_node_t* task_get_wait_node(task_t *task) {
    return &task->wait_node;
}

/* Set notification value */
void task_set_notify_val(task_t *t, uint32_t val) {
    if (t) t->notify_val = val;
}

/* Get notification value */
uint32_t task_get_notify_val(task_t *t) {
    return t ? t->notify_val : 0;
}

/* Set notification state */
void task_set_notify_state(task_t *t, uint8_t state) {
    if (t) {
        t->notify_state = state;
    }
}

/* Get notification state */
uint8_t task_get_notify_state(task_t *t) {
    return t ? t->notify_state : 0;
}

/* Get total CPU ticks consumed by task */
uint64_t task_get_cpu_ticks(task_t *t) {
    return t ? t->total_cpu_ticks : 0;
}

/* Get the base weight of a task */
uint8_t task_get_base_weight(task_t *t) {
    return t ? t->base_weight : 0;
}

/* Restore task weight to its base weight */
void task_restore_base_weight(task_t *t) {
    if (t) {
        t->weight = t->base_weight;
    }
}

/* Temporarily boost task weight (for Priority Inheritance) */
void task_boost_weight(task_t *t, uint8_t weight) {
    if (t) {
        if (weight == 0) {
            weight = 1;
        }
        if (weight > t->weight) {
            t->weight = weight;
        }
    }
}

/* Set event group wait parameters */
void task_set_event_wait(task_t *t, uint32_t bits, uint8_t flags) {
    if (t) {
        t->event_mask = bits;
        t->event_flags = flags;
    }
}

/* Get event group wait bits */
uint32_t task_get_event_bits(task_t *t) {
    return t ? t->event_mask : 0;
}

/* Get/Set event group flags */
uint8_t task_get_event_flags(task_t *t) {
    return t ? t->event_flags : 0;
}
