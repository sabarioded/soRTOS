# Event Group Architecture

## Table of Contents

- [Overview](#overview)
  - [Key Features](#key-features)
- [Architecture](#architecture)
- [Data Structures](#data-structures)
  - [Event Group Structure](#event-group-structure)
  - [Wait Node Integration](#wait-node-integration)
- [Algorithms](#algorithms)
  - [Setting Bits](#setting-bits)
  - [Waiting for Bits](#waiting-for-bits)
  - [Wake-up Condition Evaluation](#wake-up-condition-evaluation)
- [Concurrency & Thread Safety](#concurrency--thread-safety)
- [Performance Analysis](#performance-analysis)
  - [Time Complexity](#time-complexity)
  - [Space Complexity](#space-complexity)
- [Configuration Parameters](#configuration-parameters)
- [Example Scenarios](#example-scenarios)
  - [Scenario 1: Multiple Tasks Waiting for Different Events](#scenario-1-multiple-tasks-waiting-for-different-events)
  - [Scenario 2: Task Waiting for All Bits](#scenario-2-task-waiting-for-all-bits)
  - [Scenario 3: Event Group with Timeout](#scenario-3-event-group-with-timeout)

- [Appendix: Code Snippets](#appendix-code-snippets)

---

## Overview

The soRTOS event group provides a **lightweight synchronization primitive** that allows tasks to wait for one or more event bits to be set. It provides efficient multi-event synchronization without the overhead of multiple semaphores or condition variables.

Event groups are particularly useful for:
*   **Task Coordination:** Multiple tasks can wait for different combinations of events
*   **State Machines:** Tasks can wait for specific state transitions
*   **ISR-to-Task Communication:** ISRs can set bits that tasks are waiting for
*   **Multi-Event Synchronization:** Wait for "any" or "all" of a set of events

### Key Features

*   **32-bit Event Space:** Each event group supports 32 independent event bits
*   **Flexible Wait Modes:** Wait for ANY bit or ALL bits
*   **Auto-Clear Option:** Automatically clear bits when task wakes
*   **ISR Safe:** Can set bits from interrupt context
*   **Zero-Malloc Blocking:** Uses embedded wait nodes
*   **Timeout Support:** Tasks can wait with a timeout

---

## Architecture

```mermaid
graph TD
    subgraph EventGroup[Event Group]
        Bits["<b>Event Bits</b><br/>32-bit uint32_t<br/>bits: 0x00000005"]
        WaitList["<b>Wait List</b><br/>Linked list of<br/>waiting tasks"]
        Lock["<b>Spinlock</b><br/>Thread safety"]
    end

    subgraph Tasks[Waiting Tasks]
        TaskA["<b>Task A</b><br/>Waiting for: 0x01<br/>Mode: ANY"]
        TaskB["<b>Task B</b><br/>Waiting for: 0x06<br/>Mode: ALL"]
        TaskC["<b>Task C</b><br/>Waiting for: 0x04<br/>Mode: ANY"]
    end

    subgraph ISR[Interrupt Context]
        ISR_Set["<b>ISR</b><br/>Sets bits: 0x04"]
    end

    WaitList --> TaskA
    WaitList --> TaskB
    WaitList --> TaskC
    
    ISR_Set -->|event_group_set_bits_from_isr| Bits
    Bits -->|Check conditions| WaitList
    WaitList -->|Wake satisfied tasks| TaskA
    WaitList -->|Wake satisfied tasks| TaskC
```

---

## Data Structures

### Event Group Structure

```c
struct event_group {
    wait_node_t     *wait_head;     /* Head of waiting tasks list */
    wait_node_t     *wait_tail;     /* Tail of waiting tasks list */
    uint32_t        bits;           /* Current event bits (32 flags) */
    spinlock_t      lock;           /* Protection lock */
};
```

**Key Fields:**

*   **`bits`**: 32-bit value where each bit represents an event flag (bit 0 = event 0, bit 1 = event 1, etc.)
*   **`wait_head` / `wait_tail`**: Linked list of tasks waiting for events
*   **`lock`**: Spinlock protecting the event group state

### Wait Node Integration

Tasks use their embedded `wait_node_t` to block on event groups:

```c
typedef struct wait_node {
    void *task;              /* Backpointer to owning task */
    struct wait_node *next;  /* Link for wait queues */
} wait_node_t;
```

**Wait Parameters Stored in Task:**

Each waiting task stores:
*   **`bits_to_wait`**: Which bits the task is waiting for
*   **`flags`**: Wait mode (ANY/ALL) and auto-clear option

**Flag Definitions:**

```c
#define EVENT_WAIT_ANY      0x00  /* Wait for any specified bit */
#define EVENT_WAIT_ALL      0x01  /* Wait for all specified bits */
#define EVENT_CLEAR_ON_EXIT 0x02  /* Clear bits after waking */
#define EVENT_SATISFIED_FLAG 0x80 /* Internal: condition met */
```

---

## Algorithms

### Setting Bits

**Logic Flow:**
1.  **Lock:** Acquire spinlock.
2.  **Set Bits:** Perform bitwise OR: `group->bits |= bits_to_set`.
3.  **Check Waiters:** Iterate through the list of waiting tasks.
4.  **Evaluate:** For each task, check if its wait condition (ANY or ALL) is now met.
5.  **Wake:** If satisfied:
    *   Remove task from wait list.
    *   Clear bits if `EVENT_CLEAR_ON_EXIT` was requested.
    *   Unblock the task.
6.  **Unlock:** Release spinlock.

### Waiting for Bits

Tasks can wait for bits with different modes:

**Logic Flow:**
1.  **Lock:** Acquire spinlock.
2.  **Check Condition:**
    *   **Wait ANY:** Check if `(current_bits & wait_bits) != 0`.
    *   **Wait ALL:** Check if `(current_bits & wait_bits) == wait_bits`.
3.  **If Satisfied:**
    *   Clear bits if `EVENT_CLEAR_ON_EXIT` is set.
    *   Unlock and return current bits.
4.  **If Not Satisfied:**
    *   Store wait parameters (`bits_to_wait`, `flags`) in the task structure.
    *   Add task to wait list.
    *   Set state to `TASK_BLOCKED`.
    *   Unlock and Yield.
    *   *On wake-up*: Check if woken by event (success) or timeout.

### Wake-up Condition Evaluation

When bits are set, the event group iterates through the wait list and evaluates each task's condition:

**Logic Flow:**
1.  **Iterate:** Walk the linked list of waiting tasks.
2.  **Retrieve:** For each task, get `bits_to_wait` and `flags` (Any/All).
3.  **Check:** Compare current group bits (`eg->bits`) with task's `bits_to_wait`.
4.  **Resolve:**
    *   **If Satisfied:**
        *   Remove from list.
        *   Clear bits if `CLEAR_ON_EXIT` is set.
        *   Update task's return info.
        *   Call `task_unblock()`.
    *   **If Not Satisfied:**
        *   Proceed to next task.

**Key Points:**

*   **O(N) evaluation:** Checks all waiting tasks when bits are set
*   **Immediate wake-up:** Tasks are woken as soon as their condition is met
*   **Auto-clear:** Bits can be automatically cleared when a task wakes
*   **Multiple wake-ups:** Multiple tasks can be woken by a single `set_bits()` call

---

## Concurrency & Thread Safety

The event group is protected by a **spinlock**:

```c
struct event_group {
    spinlock_t lock;  /* Protects all operations */
    /* ... */
};
```

**Critical Sections:**

*   **Setting bits:** Locked to prevent race conditions with waiters
*   **Waiting:** Locked to atomically check condition and add to wait list
*   **Wake-up evaluation:** Locked to prevent lost wake-ups

**ISR Safety:**

The `event_group_set_bits_from_isr()` function is safe to call from interrupt context:

The `event_group_set_bits_from_isr()` function is safe to call from interrupt context by modifying the bits and checking waiters within a spinlock. It avoids unsafe operations like context switching or blocking.

**Lost Wake-up Prevention:**

The wait operation uses careful locking to prevent lost wake-ups. The critical issue is ensuring the task state is set to `TASK_BLOCKED` **before** unlocking the event group lock:

**Mechanism:**
1.  Task acquires lock.
2.  Task computes new state (BLOCKED).
3.  **Critical:** Task sets state to `BLOCKED` *before* releasing the lock.
4.  Task releases lock.
5.  Task yields/sleeps.
    
If an ISR fires between step 4 and 5, it sees the task is already `BLOCKED` and can successfully wake it up (making step 5 effectively a no-op or immediate return).

**Why this matters:**

If the task state is not set before unlocking, there's a race condition window:

1. **Problematic sequence (without fix):**
   ```
   t=0: Task unlocks event group lock
   t=1: ISR fires, sets bits, calls _check_and_wake_tasks()
   t=2: ISR calls task_unblock() - but task is still TASK_RUNNING!
   t=3: Task calls task_sleep_ticks() - sets state to TASK_SLEEPING
   Result: Task missed the wake-up and will sleep unnecessarily
   ```

2. **Correct sequence (with fix):**
   ```
   t=0: Task sets state to TASK_BLOCKED (while holding lock)
   t=1: Task unlocks event group lock
   t=2: ISR fires, sets bits, calls _check_and_wake_tasks()
   t=3: ISR calls task_unblock() - sees TASK_BLOCKED, wakes task correctly
   Result: Task is woken immediately when bits are set
   ```

This ensures that if an ISR sets bits immediately after unlock, the task is already in a blockable state (`TASK_BLOCKED`) and `task_unblock()` can wake it correctly. For finite timeouts, `task_sleep_ticks()` will later change the state to `TASK_SLEEPING`, but by then the task may have already been woken if the condition was satisfied.

---

## Performance Analysis

### Time Complexity

| Operation | Complexity | Notes |
|:----------|:-----------|:------|
| `event_group_set_bits` | $O(N)$ | N = number of waiting tasks (must check all) |
| `event_group_wait_bits` | $O(1)$ | Constant time to add to wait list |
| `event_group_clear_bits` | $O(1)$ | Simple bitwise operation |
| `event_group_get_bits` | $O(1)$ | Simple read operation |

**Note:** The $O(N)$ complexity of `set_bits` is acceptable because:
*   Typically, few tasks wait on a single event group
*   The operation is still fast (just pointer chasing)
*   Multiple tasks can be woken in a single pass

### Space Complexity

| Structure | Space | Notes |
|:----------|:------|:------|
| Event group | $O(1)$ | Fixed size structure |
| Per waiting task | $O(1)$ | Uses embedded wait node |
| Total | $O(1)$ | No dynamic allocation |

---

## Configuration Parameters

Event groups use the standard scheduler wait node mechanism, so no special configuration is required. The 32-bit event space is fixed by the `uint32_t` type.

---

## Example Scenarios

### Scenario 1: Multiple Tasks Waiting for Different Events

**Setup:**
- Event Group with bits: `0x00` (all clear)
- Task A: Waiting for bit 0 (ANY mode)
- Task B: Waiting for bits 1 and 2 (ALL mode)
- Task C: Waiting for bit 3 (ANY mode)

**Timeline:**

```
t=0: Initial state
    Event bits: 0x00
    Wait list: [Task A, Task B, Task C]

t=1: ISR sets bit 0
    event_group_set_bits_from_isr(eg, 0x01)
    → Event bits: 0x01
    → Check Task A: (0x01 & 0x01) != 0 → WAKE
    → Check Task B: (0x01 & 0x06) != 0x06 → Continue waiting
    → Check Task C: (0x01 & 0x08) == 0 → Continue waiting
    → Wait list: [Task B, Task C]

t=2: Task sets bits 1 and 2
    event_group_set_bits(eg, 0x06)
    → Event bits: 0x07
    → Check Task B: (0x07 & 0x06) == 0x06 → WAKE
    → Check Task C: (0x07 & 0x08) == 0 → Continue waiting
    → Wait list: [Task C]

t=3: Task sets bit 3
    event_group_set_bits(eg, 0x08)
    → Event bits: 0x0F
    → Check Task C: (0x0F & 0x08) != 0 → WAKE
    → Wait list: []
```

### Scenario 2: Task Waiting for All Bits

**Setup:**
- Event Group: `0x00`
- Task: Waiting for bits 0x07 (bits 0, 1, 2) in ALL mode

**Timeline:**

```
t=0: Task waits
    event_group_wait_bits(eg, 0x07, EVENT_WAIT_ALL, UINT32_MAX)
    → Condition: (bits & 0x07) == 0x07
    → Current: (0x00 & 0x07) != 0x07 → BLOCK

t=1: Set bit 0
    event_group_set_bits(eg, 0x01)
    → Event bits: 0x01
    → Condition: (0x01 & 0x07) != 0x07 → Still waiting

t=2: Set bit 1
    event_group_set_bits(eg, 0x02)
    → Event bits: 0x03
    → Condition: (0x03 & 0x07) != 0x07 → Still waiting

t=3: Set bit 2
    event_group_set_bits(eg, 0x04)
    → Event bits: 0x07
    → Condition: (0x07 & 0x07) == 0x07 → WAKE!
```

### Scenario 3: Event Group with Timeout

**Setup:**
- Task waits for bit 0 with 100-tick timeout
- No one sets the bit

**Timeline:**

```
t=0: Task waits
    event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY, 100)
    → Condition not met → BLOCK
    → Sleep for 100 ticks

t=100: Timeout expires
    → Task wakes up
    → Check EVENT_SATISFIED_FLAG: Not set
    → Remove from wait list
    → Return 0 (timeout)
```

---



**Wait Flags:**

```c
#define EVENT_WAIT_ANY      0x00  /* Wake on any bit */
#define EVENT_WAIT_ALL      0x01  /* Wake on all bits */
#define EVENT_CLEAR_ON_EXIT 0x02  /* Auto-clear on wake */
```

---

## Appendix: Code Snippets

### Creating and Using an Event Group

```c
/* Create event group */
event_group_t *eg = event_group_create();

/* Task 1: Wait for any of bits 0x03 (bits 0 or 1) */
void task1(void *arg) {
    uint32_t result = event_group_wait_bits(eg, 0x03, EVENT_WAIT_ANY, UINT32_MAX);
    printf("Task 1 woke with bits: 0x%08X\n", result);
}

/* Task 2: Wait for all of bits 0x06 (bits 1 and 2) */
void task2(void *arg) {
    uint32_t result = event_group_wait_bits(eg, 0x06, EVENT_WAIT_ALL, UINT32_MAX);
    printf("Task 2 woke with bits: 0x%08X\n", result);
}

/* ISR: Set bits */
void uart_isr(void) {
    event_group_set_bits_from_isr(eg, 0x01);  /* Set bit 0 */
}
```

### Wait with Auto-Clear

```c
/* Wait for bit 0, auto-clear when waking */
uint32_t result = event_group_wait_bits(eg, 0x01, 
                                        EVENT_WAIT_ANY | EVENT_CLEAR_ON_EXIT,
                                        UINT32_MAX);

/* When task wakes, bit 0 is automatically cleared */
```

### Non-Blocking Check

```c
/* Check if bits are set without blocking */
uint32_t current = event_group_wait_bits(eg, 0x01, EVENT_WAIT_ANY, 0);
if (current & 0x01) {
    printf("Bit 0 is set!\n");
} else {
    printf("Bit 0 is not set\n");
}
```
