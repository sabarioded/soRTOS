# Scheduler Architecture

## Overview
The soRTOS scheduler is a **preemptive, weighted fair scheduler**. Unlike traditional Real-Time Operating Systems (RTOS) that often use strict priority-based scheduling (where a high-priority task starves lower ones), this scheduler ensures that **all ready tasks get a share of the CPU**, proportional to their assigned weight.

It implements a simplified version of **Stride Scheduling** or **Completely Fair Scheduling (CFS)** concepts found in larger kernels like Linux.

## Simple Explanation: How it Works

The scheduler uses a **Weighted Fair Queueing** algorithm to determine which task runs next.

1.  **Virtual Runtime (`vruntime`):** Each task maintains a counter called `vruntime` that represents how much CPU time it has consumed, scaled by its weight.
2.  **Selection:** The scheduler always selects the task with the **lowest `vruntime`** from the ready queue.
3.  **Execution & Updates:** As a task executes, its `vruntime` increases.
    *   **Higher Weight:** `vruntime` increases slowly. The task stays "cheap" longer and gets more CPU time.
    *   **Lower Weight:** `vruntime` increases quickly. The task becomes "expensive" faster and yields the CPU sooner.
4.  **Preemption:** A hardware timer (Tick) decrements the running task's time slice. When the slice expires, the scheduler updates the task's `vruntime`, re-inserts it into the sorted queue, and picks the new lowest-`vruntime` task.

## In-Depth Architecture

### 1. The `vruntime` Metric
The core of the scheduling decision is the `vruntime` (Virtual Runtime).
*   **Formula:** `vruntime += (ticks_ran * SCALER) / weight`
*   **Logic:**
    *   If a task runs for 10 ticks and has Normal Weight (20), `vruntime` increases by `10 * 1000 / 20 = 500`.
    *   If a task runs for 10 ticks and has High Weight (50), `vruntime` increases by `10 * 1000 / 50 = 200`.
    *   Since the High Weight task's `vruntime` increased less, it remains "cheaper" to schedule, ensuring it gets picked more often.

### 2. Data Structures

#### The Ready Queue (Min-Heap)
To efficiently find the task with the lowest `vruntime`, ready tasks are stored in a **Min-Heap**.
*   **Lookup:** O(1) - The best task is always at the root (`ready_heap[0]`).
*   **Insert/Remove:** O(log N) - Rebalancing the tree is fast.

#### The Sleep List
Tasks waiting for a timeout (e.g., `task_sleep_ticks`) are stored in a **Singly Linked List**, sorted by wake-up time.
*   **Tick ISR:** The interrupt handler only needs to check the head of the list. If `current_time >= head->wake_time`, the task is woken up.

#### The Zombie List
When a task is deleted or exits, it cannot free its own stack immediately (because it is running on that stack!). It enters the **ZOMBIE** state and is added to a list. The **Idle Task** periodically runs garbage collection to free memory for these zombies. Additionally, if there is no free space or unused task when creating a new task, garbage collection is actively run.

### 3. Task States
*   **TASK_READY:** In the Min-Heap, waiting for CPU.
*   **TASK_RUNNING:** Currently executing (removed from Heap).
*   **TASK_BLOCKED:** Waiting for an event or timer (in Sleep List or Wait Queue).
*   **TASK_ZOMBIE:** Finished, waiting for memory cleanup.
*   **TASK_UNUSED:** Slot available for new tasks.

### 4. Memory Management
*   **Stack Allocation:** Stacks are allocated dynamically from the system heap (`allocator.c`).
*   **Stack Protection:** A "Canary" value (`0xDEADBEEF`) is placed at the bottom of every stack. The scheduler checks this periodically to detect overflows.
*   **Context Switching:** The scheduler uses `platform_initialize_stack` to set up the initial stack frame (mimicking an interrupt return) so that `scheduler_start` can "return" into a new task.

## Task Communication

The kernel provides two primary mechanisms for tasks to communicate and synchronize safely: **Notifications** and **Queues**.

### 1. Task Notifications
The most lightweight mechanism. Each task has a 32-bit notification value.
*   **Use Case:** Signaling a task from an ISR or another task (e.g., "Data Ready").
*   **Behavior:** Unblocks a waiting task immediately. It is faster than queues or semaphores because it modifies the task structure directly without intermediate objects.

### 2. Message Queues
Thread-safe FIFO queues for passing data between tasks.
*   **Use Case:** Sending structured data (like sensor readings or commands) from one task to another.
*   **Behavior:** Tasks can block while waiting for data to arrive (read) or for space to become available (write).

### 3. Communication Flow

#### Notification Flow
1.  **Wait:** Task A calls `task_notify_wait()`. If no notification is pending, it blocks.
2.  **Signal:** Task B (or ISR) calls `task_notify(TaskA, value)`.
3.  **Wake:** Task A is immediately marked `TASK_READY`.
4.  **Switch:** If Task A is higher priority, context switches to Task A.

#### Queue Flow
1.  **Block:** Consumer calls `queue_receive()` on empty queue. It enters `TASK_BLOCKED`.
2.  **Write:** Producer calls `queue_send()`. Data is copied to the ring buffer.
3.  **Wake:** Kernel checks for blocked readers. Consumer is moved to `TASK_READY`.
4.  **Consume:** Consumer resumes, reads data from buffer, and processes it.

## Configuration

The scheduler behavior can be tuned in `scheduler.h` and `scheduler.c`:

| Macro | Description |
| :--- | :--- |
| `TASK_WEIGHT_NORMAL` | Default weight for tasks. |
| `BASE_SLICE_TICKS` | Multiplier for time slices (Slice = Weight * Base). |
| `MAX_TASKS` | Maximum number of concurrent tasks. |
| `STACK_CANARY` | Magic value for overflow detection. |

## Flow of Control

The following steps describe the lifecycle of the scheduler execution:

1. **Initialization:** `scheduler_init()` clears lists and bitmaps.
2. **Start:** `scheduler_start()` creates the Idle task and jumps to the first task.
3. **Tick:** `scheduler_tick()` (called by hardware timer) updates sleep lists and decrements the current task's time slice.
4. **Switch:** If the slice expires or a higher priority task wakes up, `schedule_next_task()` is called.
   - Current task state saved.
   - `vruntime` updated.
   - Task inserted back into Heap.
   - Lowest `vruntime` task popped from Heap.
   - Context switch performed.