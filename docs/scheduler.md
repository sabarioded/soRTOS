# Scheduler Architecture

## Overview
The soRTOS scheduler is a **preemptive, weighted fair scheduler** designed for **Symmetric Multi-Processing (SMP)**. It implements a variant of **Completely Fair Scheduling (CFS)**, similar to the Linux kernel, ensuring that all ready tasks receive a CPU share proportional to their weight.

### Key features:
*   **Time Complexity:** $O(\log N)$ for task insertion/selection (Min-Heap).
*   **Space Complexity:** $O(1)$ dynamic overhead (uses static/embedded nodes).
*   **Fairness:** Uses virtual runtime (`vruntime`) to prevent starvation.

## Architecture

```mermaid
graph TD
    subgraph Global_Context [Global Scheduler Scope]
        TaskPool["<b>Static Task Pool</b><br/>Fixed Array task_t"]
        GlobalLock["<b>Global Lock</b><br/>Protects Creation/Deletion"]
        ZombieList["<b>Zombie List</b><br/>Exited Tasks"]
    end

    subgraph CPU_0 [Per-CPU Context Core 0]
        Heap0["<b>Ready Queue</b><br/>Min-Heap (vruntime)"]
        Sleep0["<b>Sleep List</b><br/>Sorted Linked List"]
        Curr0["<b>Current Task</b>"]
        Idle0["<b>Idle Task</b>"]
    end

    subgraph CPU_1 [Per-CPU Context Core N]
        Heap1["<b>Ready Queue</b><br/>Min-Heap (vruntime)"]
        Sleep1["<b>Sleep List</b><br/>Sorted Linked List"]
        Curr1["<b>Current Task</b>"]
        Idle1["<b>Idle Task</b>"]
    end

    %% Relationships
    TaskPool -.->|Assigned To| CPU_0
    TaskPool -.->|Assigned To| CPU_1
    GlobalLock -.->|Syncs| TaskPool
```

## Scheduling Algorithm
The scheduler uses **Weighted Fair Queueing** to determine execution order:

1.  **Virtual Runtime (`vruntime`):** Each task maintains a counter called `vruntime` that represents how much CPU time it has consumed, scaled by its weight.
2.  **Selection Rule:** The scheduler always selects the task with the **lowest `vruntime`** from the local Min-Heap.
3.  **Time Slicing:** A hardware tick decrements the running task's slice.
4.  **Slice Expiry:** If the slice reaches 0, `vruntime` is updated, and the task is re-inserted into the Heap.
5.  **Preemption:** If a waking task has a lower `vruntime` than the current task, a context switch occurs immediately.

## Data Structures

### The Ready Queue (Min-Heap)
Tasks are stored in a binary **Min-Heap** structure.
*   **Lookup:** O(1) - The best task is always at the root.
*   **Insert/Remove:** O(log N).
*   **Optimization:** Uses an array-flattened tree for cache locality.

### Zero-Malloc Blocking (The Wait Node)
To ensure deterministic behavior, the kernel **never calls malloc** when a task blocks.
*   **Mechanism:** Every `task_t` contains a pre-allocated `wait_node_t` structure.
*   **Usage:** When a task blocks on a Mutex or Queue, its embedded node is linked directly into the resource's wait list.
*   **Benefit:** Blocking is always $O(1)$ memory overhead and cannot fail due to OOM (Out of Memory).

## Task Lifecycle

```mermaid
stateDiagram-v2
    [*] --> UNUSED
    UNUSED --> READY : task_create()
    
    state "Per-CPU Scheduler" as CPU {
        READY --> RUNNING : Scheduler Picks (Min vruntime)
        RUNNING --> READY : Time Slice Expired / Preempted
        
        RUNNING --> BLOCKED : Queue/Mutex Wait
        BLOCKED --> READY : Resource Available / Timeout
        
        RUNNING --> SLEEPING : task_sleep()
        SLEEPING --> READY : Timer Expire
    }
    
    RUNNING --> ZOMBIE : task_exit()
    ZOMBIE --> UNUSED : Garbage Collection
```

## Configuration
Tunable parameters in `project_config.h`:

| Macro | Description |
| :--- | :--- |
| `MAX_CPUS` | Number of processor cores to support. |
| `MAX_TASKS` | Maximum number of concurrent tasks (size of static pool). |
| `GARBAGE_COLLECTION_TICKS` | How often the Idle task cleans up Zombies. |
| `STACK_CANARY` | `0xDEADBEEF` marker for overflow detection. |