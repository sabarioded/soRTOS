# Mutex Architecture

## Table of Contents

- [Overview](#overview)
  - [Key Features](#key-features)
- [Architecture](#architecture)
- [Data Structures](#data-structures)
  - [Mutex Structure](#mutex-structure)
  - [Wait Node Integration](#wait-node-integration)
- [Algorithms](#algorithms)
  - [Lock Acquisition](#lock-acquisition)
  - [Lock Release](#lock-release)
  - [Priority Inheritance](#priority-inheritance)
- [Concurrency & Thread Safety](#concurrency--thread-safety)
- [Performance Analysis](#performance-analysis)
  - [Time Complexity](#time-complexity)
  - [Space Complexity](#space-complexity)
- [Example Scenarios](#example-scenarios)
  - [Scenario 1: Basic Mutex Usage](#scenario-1-basic-mutex-usage)
  - [Scenario 2: Priority Inversion](#scenario-2-priority-inversion)
  - [Scenario 3: Recursive Locking](#scenario-3-recursive-locking)

- [Appendix: Code Snippets](#appendix-code-snippets)

---

## Overview

The soRTOS mutex provides a **mutual exclusion lock** with **priority inheritance** to prevent priority inversion. It ensures that only one task can hold the lock at a time, and automatically boosts the priority of the lock holder when a higher-priority task is waiting.

Mutexes are particularly useful for:
*   **Critical Sections:** Protecting shared resources
*   **Data Structures:** Ensuring atomic access to data
*   **Device Drivers:** Serializing hardware access
*   **Real-Time Systems:** Preventing priority inversion

### Key Features

*   **Mutual Exclusion:** Only one task can hold the lock
*   **Priority Inheritance:** Automatically prevents priority inversion
*   **Recursive Locking:** Same task can lock multiple times
*   **Zero-Malloc Blocking:** Uses embedded wait nodes
*   **Direct Handoff:** Lock ownership transferred directly to next waiter

---

## Architecture

```mermaid
graph TD
    subgraph Mutex[Mutex]
        Owner["<b>Owner</b><br/>Task holding lock"]
        WaitList["<b>Wait List</b><br/>FIFO queue of<br/>waiting tasks"]
        Lock["<b>Spinlock</b><br/>Thread safety"]
    end

    subgraph Tasks[Tasks]
        TaskA["<b>Task A</b><br/>Weight: 10<br/>Owner"]
        TaskB["<b>Task B</b><br/>Weight: 20<br/>Waiting"]
        TaskC["<b>Task C</b><br/>Weight: 5<br/>Waiting"]
    end

    Owner --> TaskA
    WaitList --> TaskB
    WaitList --> TaskC
    
    TaskB -.->|Priority Inheritance| TaskA
    Note1["Task A weight boosted<br/>to 20 (Task B's weight)"]
    TaskA -.-> Note1
```

---

## Data Structures

### Mutex Structure

```c
typedef struct {
    spinlock_t lock;      /* Protection lock */
    void *owner;          /* Task holding the lock (NULL if unlocked) */
    wait_node_t *wait_head;  /* Head of waiting tasks list */
    wait_node_t *wait_tail;  /* Tail of waiting tasks list */
} so_mutex_t;
```

**Key Fields:**

*   **`owner`**: Pointer to the task currently holding the mutex (NULL if unlocked)
*   **`wait_head` / `wait_tail`**: FIFO queue of tasks waiting for the lock
*   **`lock`**: Spinlock protecting mutex operations

### Wait Node Integration

Tasks use their embedded `wait_node_t` to block on mutexes:

```c
typedef struct wait_node {
    void *task;              /* Backpointer to owning task */
    struct wait_node *next;  /* Link for wait queues */
} wait_node_t;
```

**Zero-Malloc Design:** Each task has a pre-allocated wait node, eliminating dynamic allocation during blocking.

---

## Algorithms

### Lock Acquisition

**Logic Flow:**
1.  **Lock:** Acquire mutex spinlock.
2.  **Check Ownership:**
    *   **Recursive:** If `owner == current_task`, return immediately.
    *   **Unlocked:** If `owner == NULL`, set `owner = current_task` and return.
3.  **Contention:** If locked by another task:
    *   **Priority Inheritance:** If `current_weight > owner_weight`, boost owner's weight.
    *   **Block:** Add current task to `wait_list`, set state to `BLOCKED`.
    *   **Unlock & Yield:** Release spinlock and yield CPU.
    *   **Retry:** On wake-up, retry acquisition.

### Lock Release

**Logic Flow:**
1.  **Lock:** Acquire mutex spinlock.
2.  **Validate:** Ensure `owner == current_task`.
3.  **Restore Priority:** Restore owner's base weight (undo inheritance).
4.  **Handoff:**
    *   **Waiters Exist:**
        *   Pop next waiter from list.
        *   **Direct Handoff:** Set `owner = next_waiter` immediately.
        *   **Cascade Boost:** If remaining waiters have higher priority than new owner, boost new owner.
        *   **Wake:** Unblock the new owner.
    *   **No Waiters:** Set `owner = NULL`.
5.  **Unlock:** Release spinlock.

### Priority Inheritance

Priority inheritance prevents **priority inversion**, where a high-priority task is blocked by a low-priority task holding a resource.

**Algorithm:**

**Logic Flow:**

1.  **Inherit:** When High Task blocks on Low Task's mutex, boost Low Task's weight to match High Task.
2.  **Restore:** When Low Task releases data, restore its original weight.
3.  **Cascade:** If other high-priority tasks are still waiting, boost the *new* owner immediately.

**Visual Example:**

```
Initial State:
- Task L (Low, weight=1): Holds mutex
- Task M (Medium, weight=5): Running
- Task H (High, weight=10): Ready

Timeline:
t=0: Task H tries to lock → blocked
    → Task L weight boosted to 10
    → Task L now runs (preempts Task M)

t=1: Task L releases lock
    → Task L weight restored to 1
    → Task H acquires lock and runs
```

---

## Concurrency & Thread Safety

The mutex is protected by a **spinlock**:

```c
typedef struct {
    spinlock_t lock;  /* Protects all operations */
    /* ... */
} so_mutex_t;
```

**Critical Sections:**

*   **Lock acquisition:** Locked to atomically check owner and add to wait list
*   **Lock release:** Locked to atomically transfer ownership
*   **Priority inheritance:** Weight changes are protected

**Safety Guarantees:**

*   **Thread Safe:** Multiple tasks can compete for the lock
*   **ISR Safe:** Spinlocks can be used from interrupt context (though mutex operations from ISRs are not recommended)
*   **Deadlock Prevention:** Recursive locking prevents self-deadlock

---

## Performance Analysis

### Time Complexity

| Operation | Complexity | Notes |
|:----------|:-----------|:------|
| `so_mutex_init` | $O(1)$ | Simple initialization |
| `so_mutex_lock` (uncontended) | $O(1)$ | Direct acquisition |
| `so_mutex_lock` (contended) | $O(1)$ | Add to wait list, block |
| `so_mutex_unlock` (no waiters) | $O(1)$ | Clear owner |
| `so_mutex_unlock` (with waiters) | $O(N)$ | N = number of waiters (to find max weight) |

**Note:** The $O(N)$ complexity in unlock is acceptable because:
*   Typically, few tasks wait on a single mutex
*   The operation is fast (just pointer chasing)
*   Priority inheritance requires checking all waiters

### Space Complexity

| Structure | Space | Notes |
|:----------|:------|:------|
| Mutex | $O(1)$ | Fixed size structure |
| Per waiting task | $O(1)$ | Uses embedded wait node |
| Total | $O(1)$ | No dynamic allocation |

---

## Example Scenarios

### Scenario 1: Basic Mutex Usage

**Setup:**
- Shared resource protected by mutex
- Multiple tasks accessing the resource

**Code:**

```c
so_mutex_t resource_mutex;

void task_a(void *arg) {
    so_mutex_lock(&resource_mutex);
    /* Critical section */
    access_shared_resource();
    so_mutex_unlock(&resource_mutex);
}

void task_b(void *arg) {
    so_mutex_lock(&resource_mutex);
    /* Critical section */
    access_shared_resource();
    so_mutex_unlock(&resource_mutex);
}
```

**Timeline:**

```
t=0: Task A locks mutex
    → owner = Task A

t=1: Task B tries to lock
    → Blocked, added to wait list

t=2: Task A unlocks
    → owner = Task B (direct handoff)
    → Task B wakes and runs
```

### Scenario 2: Priority Inversion

**Setup:**
- Task L (weight=1): Holds mutex
- Task M (weight=5): Medium priority
- Task H (weight=10): High priority, needs mutex

**Without Priority Inheritance:**

```
t=0: Task L holds mutex
t=1: Task H tries to lock → blocked
t=2: Task M preempts Task L
t=3: Task M runs for a long time
t=4: Task L finally runs and releases mutex
t=5: Task H acquires mutex

Problem: Task H waited for Task M (priority inversion!)
```

**With Priority Inheritance:**

```
t=0: Task L holds mutex
t=1: Task H tries to lock → blocked
    → Task L weight boosted to 10
t=2: Task L runs (preempts Task M)
t=3: Task L releases mutex quickly
t=4: Task H acquires mutex

Solution: Task H only waited for Task L (correct!)
```

### Scenario 3: Recursive Locking

**Setup:**
- Task locks mutex, then calls a function that also locks the same mutex

**Code:**

```c
void helper_function(void) {
    so_mutex_lock(&mutex);  /* Recursive lock */
    /* Do work... */
    so_mutex_unlock(&mutex);
}

void main_task(void *arg) {
    so_mutex_lock(&mutex);
    helper_function();  /* Safe: same task */
    so_mutex_unlock(&mutex);
}
```

**Behavior:**
- First lock: Task acquires mutex
- Recursive lock: Returns immediately (already owner)
- First unlock: Does nothing (still locked)
- Second unlock: Actually releases mutex

---



## Appendix: Code Snippets

### Basic Usage

```c
/* Initialize mutex */
so_mutex_t my_mutex;
so_mutex_init(&my_mutex);

/* Task 1 */
void task1(void *arg) {
    so_mutex_lock(&my_mutex);
    /* Critical section */
    shared_data++;
    so_mutex_unlock(&my_mutex);
}

/* Task 2 */
void task2(void *arg) {
    so_mutex_lock(&my_mutex);
    /* Critical section */
    shared_data--;
    so_mutex_unlock(&my_mutex);
}
```

### Protecting a Shared Resource

```c
static int counter = 0;
static so_mutex_t counter_mutex;

void increment_counter(void) {
    so_mutex_lock(&counter_mutex);
    counter++;
    so_mutex_unlock(&counter_mutex);
}

void decrement_counter(void) {
    so_mutex_lock(&counter_mutex);
    counter--;
    so_mutex_unlock(&counter_mutex);
}
```

### Error Handling

```c
/* Mutex operations don't return errors, but you can check owner */
void try_lock_with_timeout(so_mutex_t *m, uint32_t timeout_ticks) {
    uint32_t start = platform_get_ticks();
    
    while (platform_get_ticks() - start < timeout_ticks) {
        /* Try to lock (non-blocking check would need different API) */
        so_mutex_lock(m);
        /* If we get here, we have the lock */
        return;
    }
    
    /* Timeout - handle error */
}
```

### Priority Inheritance in Action

```c
/* Low priority task */
void low_priority_task(void *arg) {
    so_mutex_lock(&shared_mutex);
    /* Long operation... */
    task_sleep_ticks(100);
    so_mutex_unlock(&shared_mutex);
}

/* High priority task */
void high_priority_task(void *arg) {
    so_mutex_lock(&shared_mutex);  /* Will boost low task's priority */
    /* Quick operation... */
    so_mutex_unlock(&shared_mutex);
}
```
