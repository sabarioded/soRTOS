# Scheduler Architecture

## Overview
The soRTOS scheduler is a **preemptive, weighted fair scheduler** with support for **Symmetric Multi-Processing (SMP)**. It ensures that all ready tasks get a share of the CPU proportional to their assigned weight, distributed across available processor cores.

It implements a simplified version of **Stride Scheduling** or **Completely Fair Scheduling (CFS)** concepts found in larger kernels like Linux.

## SMP Architecture

The scheduler is designed to scale across multiple CPUs (defined by `MAX_CPUS`). It splits resources into **Global** and **Per-CPU** contexts to minimize lock contention.

### 1. Global Context
*   **Task Pool:** A static array of `task_t` structures. This avoids dynamic allocation overhead for the kernel's internal metadata.
*   **Free List:** A linked list of unused slots in the task pool.
*   **Zombie List:** Tasks that have exited but haven't had their stacks freed yet.
*   **Global Lock:** Protects the creation and deletion of tasks.

### 2. Per-CPU Context
Each CPU has its own independent scheduler instance containing:
*   **Ready Queue (Min-Heap):** Contains tasks ready to run on this specific CPU.
*   **Sleep List:** Tasks waiting for a timeout on this CPU.
*   **Current Task:** The task currently executing on this CPU.
*   **Idle Task:** A specific idle task pinned to this CPU.
*   **CPU Lock:** Protects the local queues during context switches and ISRs.

## Scheduling Algorithm

The scheduler uses a **Weighted Fair Queueing** algorithm to determine which task runs next on a specific CPU.

1.  **Virtual Runtime (`vruntime`):** Each task maintains a counter called `vruntime` that represents how much CPU time it has consumed, scaled by its weight.
2.  **Selection:** The scheduler always selects the task with the **lowest `vruntime`** from its local ready queue.
3.  **Execution & Updates:** As a task executes, its `vruntime` increases.
    *   **Higher Weight:** `vruntime` increases slowly. The task stays "cheap" longer and gets more CPU time.
    *   **Lower Weight:** `vruntime` increases quickly. The task becomes "expensive" faster and yields the CPU sooner.
4.  **Preemption:** A hardware timer (Tick) decrements the running task's time slice. When the slice expires, the scheduler updates the task's `vruntime`, re-inserts it into the sorted queue, and picks the new lowest-`vruntime` task.

## Data Structures

### The Ready Queue (Min-Heap)
To efficiently find the task with the lowest `vruntime`, ready tasks are stored in a **Min-Heap** per CPU.
*   **Lookup:** O(1) - The best task is always at the root.
*   **Insert/Remove:** O(log N).

### The Sleep List
Tasks waiting for a timeout (e.g., `task_sleep_ticks`) are stored in a **Singly Linked List** per CPU, sorted by wake-up time.

### The Zombie List
When a task is deleted or exits, it enters the **ZOMBIE** state and is added to the **Global Zombie List**. The Idle Task on any CPU can trigger garbage collection to free the stack memory associated with these zombies.

## Task Lifecycle & Affinity

### Creation & Affinity
When a task is created (`task_create`), the kernel:
1.  Allocates a TCB from the **Global** free list.
2.  Allocates stack memory from the heap.
3.  Assigns the task to a CPU using a **Round-Robin** strategy.
4.  Inserts the task into that CPU's Ready Queue.

### Execution
Once assigned, a task remains pinned to its CPU. It competes only with other tasks on that same core.

### Idle Task
Each CPU creates its own Idle Task during startup. This task runs only when no other user tasks are ready on that core. It is responsible for putting the CPU into a low-power state and running garbage collection.

### Task States
*   **TASK_READY:** In the local Min-Heap, waiting for CPU.
*   **TASK_RUNNING:** Currently executing (removed from Heap).
*   **TASK_BLOCKED:** Waiting for an event or timer (in Sleep List or Wait Queue).
*   **TASK_ZOMBIE:** Finished, waiting for memory cleanup.
*   **TASK_UNUSED:** Slot available for new tasks.

## Memory Management
*   **Stack Allocation:** Stacks are allocated dynamically from the system heap (`allocator.c`) or provided statically by the user (`task_create_static`).
*   **Stack Protection:** A "Canary" value (`0xDEADBEEF`) is placed at the bottom of every stack. The scheduler checks this periodically to detect overflows.

## Task Communication

The kernel provides two primary mechanisms for tasks to communicate and synchronize safely: **Notifications** and **Queues**.

### 1. Task Notifications
The most lightweight mechanism. Each task has a 32-bit notification value.
*   **Use Case:** Signaling a task from an ISR or another task (e.g., "Data Ready").
*   **Behavior:** Unblocks a waiting task immediately. It is faster than queues or semaphores because it modifies the task structure directly.

### 2. Message Queues
Thread-safe FIFO queues for passing data between tasks.
*   **Use Case:** Sending structured data (like sensor readings or commands) from one task to another.
*   **Behavior:** Tasks can block while waiting for data to arrive (read) or for space to become available (write).

## Configuration

The scheduler behavior can be tuned in `project_config.h`:

| Macro | Description |
| :--- | :--- |
| `MAX_CPUS` | Number of processor cores to support. |
| `MAX_TASKS` | Maximum number of concurrent tasks (size of static pool). |
| `TASK_WEIGHT_NORMAL` | Default weight for tasks. |
| `BASE_SLICE_TICKS` | Multiplier for time slices (Slice = Weight * Base). |
| `STACK_CANARY` | Magic value for overflow detection. |

## Flow of Control

The following steps describe the lifecycle of the scheduler execution:

1. **Initialization:** `scheduler_init()` clears global lists and per-CPU structures.
2. **Start:** `scheduler_start()` is called on each CPU. It creates the local Idle task and jumps to the first ready task assigned to that core.
3. **Tick:** `scheduler_tick()` (called by hardware timer ISR) updates the local sleep list and decrements the current task's time slice.
4. **Switch:** If the slice expires or a higher priority task wakes up, `schedule_next_task()` is called.
   - Current task state saved.
   - `vruntime` updated.
   - Task inserted back into local Heap.
   - Lowest `vruntime` task popped from local Heap.
   - Context switch performed.