# Scheduler Architecture

## Table of Contents

- [Overview](#overview)
  - [Key Features](#key-features)
- [Architecture](#architecture)
- [Mathematical Foundation](#mathematical-foundation)
  - [Virtual Runtime (vruntime)](#virtual-runtime-vruntime)
  - [Basic Formula](#basic-formula)
  - [Weight-Based Time Allocation](#weight-based-time-allocation)
  - [Time Slice Calculation (as implemented)](#time-slice-calculation-as-implemented)
  - [Virtual Runtime Update](#virtual-runtime-update)
  - [Handling Overflow](#handling-overflow)
- [Scheduling Algorithm](#scheduling-algorithm)
  - [Scheduling Decision Flow](#scheduling-decision-flow)
- [Data Structures](#data-structures)
  - [The Ready Queue (Min-Heap)](#the-ready-queue-min-heap)
  - [Sleep List](#sleep-list)
  - [Zero-Malloc Blocking (The Wait Node)](#zero-malloc-blocking-the-wait-node)
- [Task Lifecycle](#task-lifecycle)
  - [State Descriptions](#state-descriptions)
  - [State Transitions](#state-transitions)
- [SMP Synchronization](#smp-synchronization)
  - [Locking Hierarchy](#locking-hierarchy)
  - [Lock Ordering Rules](#lock-ordering-rules)
  - [Critical Sections](#critical-sections)
- [Advanced Features](#advanced-features)
  - [Priority Inheritance](#priority-inheritance)
  - [Vruntime Synchronization](#vruntime-synchronization)
  - [Load Balancing (Future Enhancement)](#load-balancing-future-enhancement)
- [Performance Analysis](#performance-analysis)
  - [Time Complexity Summary](#time-complexity-summary)
  - [Space Complexity](#space-complexity)
  - [Fairness Guarantee](#fairness-guarantee)
- [Configuration Parameters](#configuration-parameters)
  - [Tuning Guidelines](#tuning-guidelines)
- [Example Scenarios](#example-scenarios)
  - [Scenario 1: Three Tasks with Different Weights](#scenario-1-three-tasks-with-different-weights)
  - [Scenario 2: Task Blocking and Wake-up with Vruntime Synchronization](#scenario-2-task-blocking-and-wake-up-with-vruntime-synchronization)
  - [Scenario 3: Priority Inversion Prevention](#scenario-3-priority-inversion-prevention)
- [Debugging Features](#debugging-features)
- [Appendix: Code Snippets](#appendix-code-snippets)

---

## Overview
The soRTOS scheduler is a **preemptive, weighted fair scheduler** designed for **Symmetric Multi-Processing (SMP)**. It implements a variant of **Completely Fair Scheduling (CFS)**, similar to the Linux kernel, ensuring that all ready tasks receive a CPU share proportional to their weight.

### Key features:
*   **Time Complexity:** $O(\log N)$ for task insertion/selection (Min-Heap).
*   **Space Complexity:** $O(1)$ dynamic overhead (uses static/embedded nodes).
*   **Fairness:** Uses virtual runtime (`vruntime`) to prevent starvation while respecting weights.
*   **SMP Support:** Per-CPU runqueues with affinity support.

---

## Architecture

```mermaid
graph TD
    subgraph Global_Context [Global Scheduler Scope]
        TaskPool["<b>Static Task Pool</b><br/>Fixed Array task_t"]
        GlobalLock["<b>Global Lock</b><br/>Protects Creation/Deletion"]
        ZombieList["<b>Zombie List</b><br/>Exited Tasks"]
        IDbitmap["<b>ID Bitmap</b><br/>Array of 64-bit bitmaps"]
    end

    subgraph CPU_0 [Per-CPU Context Core 0]
        Heap0["<b>Ready Queue</b><br/>Min-Heap (vruntime)"]
        Sleep0["<b>Sleep List</b><br/>Sorted Linked List"]
        Curr0["<b>Current Task</b>"]
        Idle0["<b>Idle Task</b>"]
        CPULock0["<b>Per-CPU Lock</b>"]
    end

    subgraph CPU_1 [Per-CPU Context Core N]
        Heap1["<b>Ready Queue</b><br/>Min-Heap (vruntime)"]
        Sleep1["<b>Sleep List</b><br/>Sorted Linked List"]
        Curr1["<b>Current Task</b>"]
        Idle1["<b>Idle Task</b>"]
        CPULock1["<b>Per-CPU Lock</b>"]
    end

    %% Relationships
    TaskPool -.->|Assigned To| CPU_0
    TaskPool -.->|Assigned To| CPU_1
    GlobalLock -.->|Syncs| TaskPool
    GlobalLock -.->|Syncs| ZombieList
    GlobalLock -.->|Syncs| IDbitmap
```

---

## Mathematical Foundation

### Virtual Runtime (vruntime)

The core of the fair scheduler is the **virtual runtime** concept. Each task's `vruntime` represents the amount of CPU time it has consumed, normalized by its weight.

#### Basic Formula

$$
\Delta vruntime = \frac{\Delta t_{physical} \times W_{default}}{W_{task}}
$$

Where:
- $\Delta vruntime$ = Increase in virtual runtime
- $\Delta t_{physical}$ = Physical CPU time consumed (in ticks)
- $W_{default}$ = Default weight (typically 1024 in Linux, configurable in soRTOS)
- $W_{task}$ = Task's assigned weight

#### Weight-Based Time Allocation

For a system with $N$ tasks where task $i$ has weight $w_i$, the share of CPU time for task $i$ is:

$$
\text{CPU Share}_i = \frac{w_i}{\sum_{j=1}^{N} w_j}
$$

**Example:**
- Task A: weight = 2
- Task B: weight = 1
- Task C: weight = 1

Total weight = 4

- Task A receives: $\frac{2}{4} = 50\%$ CPU time
- Task B receives: $\frac{1}{4} = 25\%$ CPU time
- Task C receives: $\frac{1}{4} = 25\%$ CPU time

#### Time Slice Calculation (as implemented)

In this scheduler, **time slice is linear in weight**:

$$
\text{Time Slice}_i = \text{BASE\_SLICE\_TICKS} \times w_i
$$

This design ensures CPU share is proportional to weight, while maintaining fairness in the virtual timeline.

**Why it works:**
1.  **Throughput:** A task's physical run time ($\Delta t$) is scaled by its weight ($w_i$).
    $$ \Delta t = \text{BASE\_SLICE\_TICKS} \times w_i $$
2.  **Fairness:** The cost charged to `vruntime` is inversely proportional to weight.
    $$ \Delta vruntime = \frac{\Delta t \times \text{VRUNTIME\_SCALER}}{w_i} $$

**The Cancellation Effect:**
When a task consumes its full time slice, the weight factors cancel out:

$$
\Delta vruntime = \frac{(\text{BASE\_SLICE\_TICKS} \times w_i) \times \text{VRUNTIME\_SCALER}}{w_i} = \text{BASE\_SLICE\_TICKS} \times \text{VRUNTIME\_SCALER}
$$

**Result:** Every task, regardless of weight, advances its virtual clock by the same amount per "turn". This keeps `vruntime` values synchronized (fairness), even though high-weight tasks physically run longer (throughput).

**In the code (simplified from `task_create()` and `schedule_next_task()`):**
```c
/* slice is weight-scaled */
task->time_slice = task->weight * BASE_SLICE_TICKS;

/* when switching away, charge what actually ran */
uint32_t max_slice = task->weight * BASE_SLICE_TICKS;
uint32_t ticks_ran = max_slice - task->time_slice;
if (ticks_ran == 0) ticks_ran = 1; /* prevent free yields */

task->vruntime += (uint64_t)(ticks_ran * VRUNTIME_SCALER) / task->weight;
task->time_slice = max_slice; /* replenish */
```

> Note: This differs from Linux CFS’s typical “scheduler period / total_weight” time-slice formula. Here, the effective “round length” grows with the number of runnable tasks:  
> `round_ticks ≈ BASE_SLICE_TICKS × Σ(weights of runnable tasks)`.

#### Virtual Runtime Update

When a task runs for $\Delta t$ ticks:

$$
vruntime_{new} = vruntime_{old} + \Delta vruntime
$$

This ensures tasks with higher weights advance their `vruntime` more slowly, allowing them to run more frequently.

#### Handling Overflow

Virtual runtime uses 64-bit unsigned integers, which can overflow. The scheduler handles this using **modular arithmetic**:

$$
\mathrm{VRUNTIME\_LT}(a, b) \equiv (\mathrm{int64\_t})(a - b) < 0
$$

In C implementation:
```c
#define VRUNTIME_LT(a, b)   ((int64_t)((a) - (b)) < 0)
```

This comparison works correctly even when values wrap around the 64-bit boundary by treating the unsigned subtraction result as a signed integer, which preserves the ordering relationship.

---

## Scheduling Algorithm

### Scheduling Decision Flow

The scheduling logic executes on every system tick (`scheduler_tick`) and during context switches (`schedule_next_task`).

1.  **Process Sleeping Tasks**
    *   The scheduler checks the **Sleep List** (sorted by wake time).
    *   Tasks with expired timeouts are woken up.
    *   **Vruntime Synchronization:** To prevent "sleep monopoly," woken tasks have their `vruntime` clamped to the current system minimum:
        ```c
        if (VRUNTIME_LT(task->vruntime, min_vruntime)) task->vruntime = min_vruntime;
        ```
    *   Tasks are inserted into the **Ready Heap** ($O(\log N)$).

2.  **Update Current Task**
    *   The running task's `time_slice` is decremented.
    *   **Time Slice Calculation:** Slices are proportional to weight:
        ```c
        max_slice = task->weight * BASE_SLICE_TICKS;
        ```

3.  **Check for Preemption**
    *   **Slice Expiration:** If `time_slice` reaches 0, the task is flagged for preemption.
    *   **Fairness Preemption:** If a waiting task in the Ready Heap has a lower `vruntime` than the current task, it indicates the current task has "overstayed" its fair share relative to others.
        ```c
        if (VRUNTIME_LT(min_ready_vruntime, curr->vruntime)) reschedule = 1;
        ```
    *   **Idle Preemption:** If the Idle task is running and *any* task becomes ready, preemption is immediate.

4.  **Context Switch & Vruntime Charging**
    *   If a reschedule is required, `schedule_next_task()` is called.
    *   **Charge Vruntime:** The outgoing task is charged for the ticks it actually consumed, scaled inversely by weight:
        ```c
        ticks_ran = max_slice - time_slice;
        if (ticks_ran == 0) ticks_ran = 1; /* prevent free yields */
        
        vruntime += (ticks_ran * VRUNTIME_SCALER) / weight;
        ```
    *   **Replenish Slice:** The task's time slice is reset to `max_slice`.
    *   **Re-insertion:** The outgoing task is re-inserted into the Ready Heap.

5.  **Select Next Task**
    *   The task with the **lowest `vruntime`** is popped from the Ready Heap ($O(\log N)$).
    *   If the heap is empty, the **Idle Task** is selected.

---

## Data Structures

### The Ready Queue (Min-Heap)

The ready queue is implemented as a **binary min-heap** stored in an array for cache efficiency.

#### Heap Properties

For a heap stored in array `H[]`:
- **Parent of node $i$:** $\lfloor \frac{i-1}{2} \rfloor$
- **Left child of node $i$:** $2i + 1$
- **Right child of node $i$:** $2i + 2$

#### Invariant

$$
\forall i > 0: H[\lfloor \frac{i-1}{2} \rfloor].vruntime \leq H[i].vruntime
$$

#### Operations

**Insert (Heap-Up):**
```mermaid
graph TD
    A[Insert at end] --> B[Compare with parent]
    B --> C{vruntime < parent?}
    C -->|Yes| D[Swap with parent]
    D --> B
    C -->|No| E[Done]
```

Time complexity: $O(\log N)$

**Extract-Min (Heap-Down):**
```mermaid
graph TD
    A[Remove root] --> B[Move last to root]
    B --> C[Compare with children]
    C --> D{vruntime > min_child?}
    D -->|Yes| E[Swap with min_child]
    E --> C
    D -->|No| F[Done]
```

Time complexity: $O(\log N)$

**Remove from middle:**
```mermaid
graph TD
    A[Mark index] --> B[Move last to position]
    B --> C[Try Heap-Up]
    C --> D[Try Heap-Down]
    D --> E[Done]
```

Time complexity: $O(\log N)$

#### Cache Optimization

Array-based heap provides better cache locality than pointer-based trees:
- Sequential memory access
- Predictable memory layout
- Better prefetching by CPU

### Sleep List

The sleep list is a **sorted singly-linked list** ordered by wake-up time.

```mermaid
graph LR
    Head["Head"] --> T1["Task 1<br/>wake: 100"]
    T1 --> T2["Task 2<br/>wake: 150"]
    T2 --> T3["Task 3<br/>wake: 200"]
    T3 --> NULL["NULL"]
```

#### Insertion Algorithm

```c
if (list_empty || new_task->wake_time < head->wake_time) {
    insert_at_head();
} else {
    // Find insertion point
    while (curr->next && curr->next->wake_time < new_task->wake_time) {
        curr = curr->next;
    }
    insert_after(curr);
}
```

Time complexity: $O(N)$ worst case, but typically $O(1)$ if tasks sleep for similar durations.

#### Wake-up Processing

```c
void process_sleep_list(uint32_t current_tick) {
    while (head != NULL && head->wake_time <= current_tick) {
        task = remove_head();
        wake_task(task);
    }
}
```

Time complexity: $O(K)$ where $K$ is the number of tasks to wake.

### Zero-Malloc Blocking (The Wait Node)

To ensure deterministic behavior and avoid memory allocation in critical paths, every task contains an embedded `wait_node_t`.

```c
typedef struct wait_node {
    void *task;           // Backpointer to owning task
    struct wait_node *next;  // Link for wait queues
} wait_node_t;

typedef struct task_struct {
    // ... other fields ...
    wait_node_t wait_node;  // Embedded, no malloc needed
} task_t;
```

#### Benefits

1.  **Deterministic Memory:** No allocation failures during blocking operations
2.  **Cache Locality:** Wait node is part of task structure
3.  **No Fragmentation:** No dynamic allocation/deallocation
4.  **Real-Time Guarantees:** Blocking is $O(1)$ time and space

#### Usage Pattern

```c
// When blocking on a mutex:
task_t *task = get_current_task();
wait_node_t *node = &task->wait_node;  // No malloc!
node->task = task;
node->next = mutex->wait_list;
mutex->wait_list = node;
```

---

## Task Lifecycle

```mermaid
stateDiagram-v2
    [*] --> UNUSED
    
    UNUSED --> READY: task_create()<br/>Allocate from pool<br/>Initialize stack<br/>Set weight & vruntime
    
    READY --> RUNNING: Scheduler selects<br/>(lowest vruntime)
    
    RUNNING --> READY: Time slice expired<br/>OR preempted by<br/>lower vruntime task
    
    RUNNING --> BLOCKED: Lock mutex<br/>Wait for queue<br/>Block on semaphore
    
    BLOCKED --> READY: Resource available<br/>OR timeout
    
    RUNNING --> SLEEPING: task_sleep()<br/>Wait with timeout<br/>Wait for notification
    
    SLEEPING --> READY: Timer expired<br/>Notification received<br/>Timeout
    
    RUNNING --> ZOMBIE: task_exit()<br/>Resources held
    
    ZOMBIE --> UNUSED: Garbage collection<br/>by idle task
    
    note right of UNUSED
        Available slot in task pool
        No resources allocated
    end note
    
    note right of READY
        In Ready Heap
        Ordered by vruntime
        Waiting for CPU
    end note
    
    note right of RUNNING
        Executing on CPU
        time_slice counting down
        Not in any queue
    end note
    
    note right of BLOCKED
        Removed from heap
        In resource wait list
        May be in sleep list (timeout)
    end note
    
    note right of SLEEPING
        Removed from heap
        In sleep list (sorted by wake time)
        Can be woken early by notify
    end note
    
    note right of ZOMBIE
        Marked for deletion
        In zombie list
        Stack/resources not freed yet
    end note
```

### State Descriptions

| State | Description | In Heap? | In Sleep List? |
|:------|:------------|:---------|:---------------|
| `UNUSED` | Task slot available in pool | No | No |
| `READY` | Waiting to be scheduled | **Yes** | No |
| `RUNNING` | Currently executing on CPU | No | No |
| `BLOCKED` | Waiting for resource (mutex, queue) | No | Maybe (if timeout) |
| `SLEEPING` | Waiting for time or notification | No | **Yes** |
| `ZOMBIE` | Exited, awaiting cleanup | No | No |

### State Transitions

```mermaid
graph LR
    subgraph Creation
        A[UNUSED] -->|task_create| B[READY]
    end
    
    subgraph Scheduling
        B -->|schedule_next| C[RUNNING]
        C -->|time_slice=0| B
        C -->|preempted| B
    end
    
    subgraph Blocking
        C -->|mutex_lock| D[BLOCKED]
        D -->|mutex_unlock| B
        C -->|task_sleep| E[SLEEPING]
        E -->|timeout| B
        E -->|notify| B
    end
    
    subgraph Termination
        C -->|task_exit| F[ZOMBIE]
        F -->|GC| A
    end
```

---

## SMP Synchronization

### Locking Hierarchy

```mermaid
graph TD
    Global["<b>Global Lock</b><br/>g_sched.lock"] -->|Protects| Pool["Task Pool"]
    Global -->|Protects| Zombie["Zombie List"]
    Global -->|Protects| Bitmap["ID Bitmap"]
    
    CPU0["<b>CPU 0 Lock</b><br/>cpu_sched[0].lock"] -->|Protects| Heap0["Ready Heap"]
    CPU0 -->|Protects| Sleep0["Sleep List"]
    CPU0 -->|Protects| Curr0["Current Task"]
    
    CPU1["<b>CPU 1 Lock</b><br/>cpu_sched[1].lock"] -->|Protects| Heap1["Ready Heap"]
    CPU1 -->|Protects| Sleep1["Sleep List"]
    CPU1 -->|Protects| Curr1["Current Task"]
```

### Lock Ordering Rules

1.  **Never hold multiple CPU locks simultaneously** (prevents deadlock)
2.  **CPU lock can be acquired while holding Global lock**
3.  **Task-specific operations use the CPU lock of the task's affinity**

### Critical Sections

#### Task Creation
```c
// Acquire global lock for pool allocation
spin_lock(&g_sched.lock);
task = allocate_from_pool();
assign_cpu_affinity(task);
spin_unlock(&g_sched.lock);

// Acquire CPU lock for scheduling
cpu = task->cpu_id;
spin_lock(&cpu_sched[cpu].lock);
heap_insert(&cpu_sched[cpu], task);
spin_unlock(&cpu_sched[cpu].lock);
```

#### Task Notification (Cross-CPU)
```c
// Find task by ID (global)
target = find_task(task_id);

// Lock target's CPU, not caller's CPU
cpu = target->cpu_id;
spin_lock(&cpu_sched[cpu].lock);
target->notify_val |= value;
unblock_task(target);
spin_unlock(&cpu_sched[cpu].lock);
```

---

## Advanced Features

### Priority Inheritance

**Priority Inversion** occurs when a high-priority task is blocked waiting for a resource held by a low-priority task, but the low-priority task is preempted by a medium-priority task. This effectively causes the high-priority task to wait for the medium-priority task, violating the intended priority policy.

**Priority Inheritance** solves this by temporarily boosting the weight (priority) of the task holding the resource to match the highest weight of any task waiting for that resource. This ensures the lock holder runs quickly to release the resource.

In soRTOS, when a low-weight task holds a resource needed by a high-weight task, temporary weight boosting prevents priority inversion.

```mermaid
sequenceDiagram
    participant LowTask as Low Weight Task (1)
    participant Mutex as Mutex
    participant HighTask as High Weight Task (10)
    participant Sched as Scheduler

    LowTask->>Mutex: Lock (acquired)
    HighTask->>Mutex: Lock (blocked)
    Mutex->>LowTask: Boost weight to 10
    Sched->>LowTask: Run with higher priority
    LowTask->>Mutex: Unlock
    Mutex->>LowTask: Restore weight to 1
    Mutex->>HighTask: Unblock
    HighTask->>Mutex: Lock (acquired)
```

Implementation:
```c
void task_boost_weight(task_t *t, uint8_t weight) {
    if (weight > t->weight) {
        t->weight = weight;  // Temporarily increase
    }
}

void task_restore_base_weight(task_t *t) {
    t->weight = t->base_weight;  // Restore original
}
```

### Vruntime Synchronization

When a task wakes from sleep, its `vruntime` may be far behind the current minimum. To prevent it from monopolizing the CPU, we synchronize:

$$
vruntime_{task} = \max(vruntime_{task}, vruntime_{min})
$$

```c
uint64_t min_v = get_min_vruntime();
if (task->vruntime < min_v) {
    task->vruntime = min_v;
}
```

### Load Balancing (Future Enhancement)

For true SMP efficiency, periodic load balancing can migrate tasks between CPUs:

$$
\mathrm{Load}_{\mathrm{CPU}_i} = \sum_{j \in \mathrm{Ready}_{\mathrm{CPU}_i}} w_j
$$

Migration threshold:
$$
|\mathrm{Load}_{\mathrm{CPU}_i} - \mathrm{Load}_{\mathrm{CPU}_k}| > \mathrm{Threshold}
$$

---

## Performance Analysis

### Time Complexity Summary

| Operation | Complexity | Notes |
|:----------|:-----------|:------|
| Get next task | $O(1)$ | Root of heap |
| Insert task into ready queue | $O(\log N)$ | Heap insert |
| Remove task from ready queue | $O(\log N)$ | Heap remove |
| Update task vruntime | $O(1)$ | Simple arithmetic |
| Process sleep list | $O(K)$ | K = tasks to wake |
| Task creation | $O(\log N)$ | Pool alloc + heap insert |
| Task deletion | $O(\log N)$ | Heap remove + cleanup |
| Context switch | $O(\log N)$ | Remove old + insert new |

### Space Complexity

| Structure | Space | Notes |
|:----------|:------|:------|
| Task pool | $O(M)$ | M = MAX_TASKS (static) |
| Ready heap | $O(N)$ | N = ready tasks (pointers only) |
| Sleep list | $O(K)$ | K = sleeping tasks (pointers only) |
| Per-task overhead | $O(1)$ | Fixed size structure |
| Total dynamic overhead | $O(1)$ | No malloc during runtime |

### Fairness Guarantee

Over a scheduling period $P$, each task $i$ receives:

$$
\text{CPU Time}_i = P \times \frac{w_i}{\sum_{j=1}^{N} w_j} \pm \epsilon
$$

Where $\epsilon$ is bounded by the maximum time slice duration.

---

## Configuration Parameters

Tunable parameters in `project_config.h` (names match the implementation):

| Macro | Example | Description |
|:------|:--------|:------------|
| `MAX_CPUS` | 4 | Number of processor cores |
| `MAX_TASKS` | 32 | Maximum concurrent tasks (static pool size) |
| `BASE_SLICE_TICKS` | 10 | Base slice unit; task slice = `weight * BASE_SLICE_TICKS` |
| `VRUNTIME_SCALER` | 1024 | Scaling constant used in vruntime charging |
| `GARBAGE_COLLECTION_TICKS` | 1000 | How often the idle task triggers zombie cleanup |
| `STACK_CANARY` | `0xDEADBEEF` | Marker for stack overflow detection |

### Tuning Guidelines

**If you want lower latency / faster interactivity:**
- Decrease `BASE_SLICE_TICKS` (shorter slices → more frequent switches)

**If you want higher throughput / fewer context switches:**
- Increase `BASE_SLICE_TICKS` (longer slices → fewer switches)

**Notes:**
- `VRUNTIME_SCALER` does **not** change fairness; it only scales the numeric `vruntime` values.
- The effective “round length” grows with runnable load: `round_ticks ≈ BASE_SLICE_TICKS × Σ(weights)`.

---

## Example Scenarios

### Scenario 1: Three Tasks with Different Weights

**Setup:**
- Task A: weight = 4
- Task B: weight = 2
- Task C: weight = 1

In the implementation, each task’s slice is:

$$
\text{slice} = \text{weight} \times \text{BASE\_SLICE\_TICKS}
$$

So (assuming `BASE_SLICE_TICKS = 10` just for an easy-to-read example):

| Task | Weight | Slice (ticks) | CPU Share (ideal) |
|:-----|:-------|:--------------|:------------------|
| A | 4 | 40 | 57.1% |
| B | 2 | 20 | 28.6% |
| C | 1 | 10 | 14.3% |

**One “round” length:** `40 + 20 + 10 = 70 ticks`

#### Execution Timeline

Round 1:
- `t=0..40`   → A runs
- `t=40..60`  → B runs
- `t=60..70`  → C runs

Round 2:
- `t=70..110` → A runs
- `t=110..130`→ B runs
- `t=130..140`→ C runs

Visually:
```
0         40        60   70        110       130  140
|---- A ----|-- B --|C|---- A ----|-- B --|C| ...
```

#### Why the order is stable (with full slices)

Charging uses:
$$
\Delta vruntime = \frac{\Delta t \times \text{VRUNTIME\_SCALER}}{\text{weight}}
$$

If each task consumes a full slice:
- A: $\Delta t=40$ → $\Delta v = (40 \cdot s)/4 = 10s$
- B: $\Delta t=20$ → $\Delta v = (20 \cdot s)/2 = 10s$
- C: $\Delta t=10$ → $\Delta v = (10 \cdot s)/1 = 10s$

So every task advances by the same vruntime step per round (ties are broken by heap ordering/insertion order), while CPU time share stays proportional to weight.

**Key observations:**
1. CPU share follows the weight ratio (4:2:1).
2. Fairness comes from keeping vruntime values close.
3. If a task yields early, it’s charged less (`ticks_ran` smaller), so it can become runnable again sooner (expected behavior).

### Scenario 2: Task Blocking and Wake-up with Vruntime Synchronization

**Problem:** When a task sleeps or blocks for a long time, its vruntime falls far behind. If not synchronized, it would monopolize the CPU upon waking.

**Setup:**
- Task A: Currently running, vruntime = 1000
- Task B: Blocked on mutex for a long time, old vruntime = 500
- System minimum vruntime: 1000 (from heap root)

Wake-up steps:

1. Task A unlocks the mutex.
2. Scheduler unblocks Task B.
3. Scheduler reads `min_vruntime` from the ready-heap root (current fairness baseline).
4. If `taskB.vruntime < min_vruntime`, clamp it up:
   $$
   vruntime_{wake} = \max(vruntime_{task}, vruntime_{min})
   $$
5. Insert Task B back into the ready heap. It now competes fairly instead of “catching up” by monopolizing the CPU.

**Why Synchronization Matters:**

Without synchronization:
```
Task B vruntime: 500
Task A vruntime: 1000
→ Task B would run for (1000-500) = 500 ticks before Task A gets CPU!
→ Unfair monopoly by sleeping task
```

With synchronization:
```
Task B vruntime: 500 → synchronized to 1000
Task A vruntime: 1000
→ Both tasks compete fairly based on current weights
→ No monopoly, fair scheduling resumes
```

**Vruntime Synchronization Formula:**

$$
vruntime_{wake} = \max(vruntime_{task}, vruntime_{min})
$$

Where:
- $vruntime_{task}$ = Task's old vruntime (before blocking)
- $vruntime_{min}$ = Current minimum vruntime in system (heap root)

**Implementation:**
```c
void task_unblock(task_t *task) {
    // Get current minimum vruntime from heap
    uint64_t min_vruntime = get_min_vruntime();
    
    // Synchronize if task fell too far behind
    if (VRUNTIME_LT(task->vruntime, min_vruntime)) {
        task->vruntime = min_vruntime;
    }
    
    // Insert into ready heap
    heap_insert(task);
}
```

### Scenario 3: Priority Inversion Prevention

Priority inversion occurs when a high-weight task is blocked by a low-weight task holding a resource, while a medium-weight task preempts the low-weight task.

**Setup:**
- Low Task: weight = 1, holds mutex
- Medium Task: weight = 5
- High Task: weight = 10, needs mutex

**Without Priority Inheritance:**

Timeline (ticks):
- `t=0..50`   → Low runs and **holds mutex**
- `t=50..250` → Medium runs and **preempts Low**
- `t=100..300`→ High becomes ready but stays **BLOCKED** (mutex held by Low)
- `t=250..300`→ Low finally runs again and **releases mutex**
- `t=300..400`→ High runs

**Analysis:** High-priority task blocked for 200 ticks waiting for low-priority task, which was preempted by medium-priority task. This is priority inversion!

**Total high-priority wait time: 200 ticks** (blocked by medium-priority task!)

**With Priority Inheritance:**

Timeline (ticks):
- `t=0..50`   → Low runs and **holds mutex**
- `t=50`      → High needs mutex → Low **inherits/boosts** to weight 10
- `t=50..150` → Low runs boosted and **releases mutex**
- `t=150..250`→ High runs
- `t=250..350`→ Medium runs (after the critical path is unblocked)

**Analysis:** Low-priority task inherited high-priority weight (10), completed quickly, released lock. High-priority task only waited 100 ticks instead of 200.

**Total high-priority wait time: 100 ticks** (only blocked by boosted low-priority task)

**Improvement: 50% reduction in blocking time!**

**Comparison Table:**

| Metric | Without Inheritance | With Inheritance | Improvement |
|:-------|:-------------------|:-----------------|:------------|
| High task wait time | 200 ticks | 100 ticks | **50% faster** |
| Low task completion | 300 ticks | 150 ticks | **50% faster** |
| Medium task start | 50 ticks | 150 ticks | Delayed (correct!) |
| Total system time | 400 ticks | 350 ticks | **12.5% faster** |

#### Implementation Details

```c
// When High Task blocks on mutex held by Low Task:
void mutex_block_with_inheritance(mutex_t *mutex, task_t *high_task) {
    task_t *lock_holder = mutex->owner;
    
    // Boost lock holder to blocker's weight
    if (high_task->weight > lock_holder->weight) {
        task_boost_weight(lock_holder, high_task->weight);
    }
    
    // Block the high priority task
    task_block(high_task);
}

// When Low Task releases mutex:
void mutex_unlock(mutex_t *mutex) {
    task_t *lock_holder = mutex->owner;
    
    // Restore original weight
    task_restore_base_weight(lock_holder);
    
    // Wake next waiter
    task_t *next = mutex->wait_list;
    if (next) {
        task_unblock(next);
    }
}
```

---

## Debugging Features

The scheduler includes built-in mechanisms to aid in debugging and system analysis:

1.  **Stack Overflow Detection:** A "canary" value (`0xDEADBEEF`) is placed at the bottom of every task stack. The scheduler checks this value periodically; if corrupted, the system panics.
2.  **Runtime Statistics:** Each task tracks `total_cpu_ticks`, `vruntime`, and `last_switch_tick` for precise CPU usage analysis.
3.  **Heap Integrity:** In debug builds, the scheduler can validate the min-heap invariant ($\text{parent} \le \text{child}$) to detect corruption in the ready queue.

---

## Appendix: Code Snippets (matching the implementation)

### Initializing Time Slice (weight-scaled)
```c
/* Called on task creation */
t->weight     = (weight == 0) ? 1 : weight;
t->base_weight = t->weight;
t->time_slice = t->weight * BASE_SLICE_TICKS;
```

### Charging Vruntime on Switch
```c
/* Called when switching away from a running task */
uint32_t max_slice = t->weight * BASE_SLICE_TICKS;
uint32_t ticks_ran = max_slice - t->time_slice;
if (ticks_ran == 0) ticks_ran = 1; /* prevent free yields */

t->vruntime += (uint64_t)(ticks_ran * VRUNTIME_SCALER) / t->weight;
t->time_slice = max_slice; /* replenish */
```

### Preemption Check (tick-side)
```c
uint32_t scheduler_tick(void) {
    /* ... */
    if (curr && !curr->is_idle) {
        if (curr->time_slice > 0) curr->time_slice--;
        if (curr->time_slice == 0) need_resched = 1;
    }

    if (curr && curr->is_idle && heap_size > 0) {
        need_resched = 1; /* always preempt idle if work exists */
    } else if (curr && VRUNTIME_LT(min_ready_vruntime, curr->vruntime)) {
        need_resched = 1; /* fair preemption */
    }
    /* ... */
}
```