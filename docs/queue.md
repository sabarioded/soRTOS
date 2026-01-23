# Queue Architecture

## Overview
The soRTOS queue implementation provides a **thread-safe, blocking, First-In-First-Out (FIFO)** mechanism for inter-task communication. It allows tasks to send and receive fixed-size data items safely, even between Interrupt Service Routines (ISRs) and tasks.

## Internal Structure

The queue is defined by the `queue_t` structure, which manages a circular buffer for data and two wait lists for task synchronization.

### 1. Circular Buffer
Data is stored in a dynamically allocated memory block treated as a ring buffer.
*   **`head`**: Index where the next item will be read (Read Pointer).
*   **`tail`**: Index where the next item will be written (Write Pointer).
*   **`count`**: Current number of items in the queue.
*   **`capacity`**: Maximum number of items the queue can hold.

### 2. Wait Queues
To handle blocking when the queue is full (for senders) or empty (for receivers), the queue maintains two separate **Linked Lists** of waiting tasks. These act as "waiting rooms".
*   **`rx_wait_head` / `rx_wait_tail`**: List of tasks waiting to **receive** data (Queue Empty).
*   **`tx_wait_head` / `tx_wait_tail`**: List of tasks waiting to **send** data (Queue Full).

These are managed as FIFO lists to ensure fairness. Each node in the list is allocated on the **stack** of the blocking task. This avoids dynamic memory allocation overhead and fragmentation. The node is automatically valid as long as the task is blocked.

## How it Works

### Sending Data (`queue_send`)
1.  **Lock:** Interrupts are disabled to ensure atomicity (`arch_irq_lock`).
2.  **Check Space:**
    *   **If Space Available:**
        1.  Copy data to `buffer[tail]`.
        2.  Increment `tail` (wrapping around).
        3.  Increment `count`.
        4.  **Wake Receiver:** If the `rx_wait_list` is not empty, pop the head task and mark it `TASK_READY`.
        5.  **Callback:** If a push callback is registered, execute it.
        6.  Unlock and return success.
    *   **If Queue Full:**
        1.  Create a wait node on the stack and add it to the `tx_wait_list`.
        2.  Set current task state to `TASK_BLOCKED`.
        3.  Unlock and **Yield**.
        4.  When woken (by a receiver removing an item), the loop restarts to retry the operation.

### Receiving Data (`queue_receive`)
1.  **Lock:** Interrupts are disabled.
2.  **Check Data:**
    *   **If Data Available:**
        1.  Copy data from `buffer[head]`.
        2.  Increment `head` (wrapping around).
        3.  Decrement `count`.
        4.  **Wake Sender:** If the `tx_wait_list` is not empty, pop the head task and mark it `TASK_READY`.
        5.  Unlock and return success.
    *   **If Queue Empty:**
        1.  Create a wait node on the stack and add it to the `rx_wait_list`.
        2.  Set current task state to `TASK_BLOCKED`.
        3.  Unlock and **Yield**.
        4.  When woken (by a sender adding an item), the loop restarts to retry the operation.

### Interrupt Safety (`_from_isr`)
ISR versions (`queue_send_from_isr`, `queue_receive_from_isr`) are **non-blocking**.
*   They lock interrupts (safe to do inside an ISR).
*   If the operation cannot be performed immediately (Full/Empty), they return an error code (`-1`) instead of blocking.
*   They still wake up blocked tasks if the operation succeeds.

## API Reference

| Function | Description | Blocking? |
| :--- | :--- | :--- |
| `queue_create` | Allocates memory for queue and buffer. | No |
| `queue_delete` | Frees memory. | No |
| `queue_send` | Pushes item to back. | **Yes** (if full) |
| `queue_receive` | Pops item from front. | **Yes** (if empty) |
| `queue_peek` | Reads front without removing. | No |
| `queue_send_from_isr` | Pushes item (for ISRs). | No |
| `queue_receive_from_isr` | Pops item (for ISRs). | No |
| `queue_reset` | Clears data and wakes all writers. | No |
| `queue_set_push_callback` | Registers a callback for when data is pushed. | No |

## Usage Example

```c
/* Create a queue for 10 integers */
queue_t *q = queue_create(sizeof(int), 10);

/* Task A: Sender */
int data = 42;
if (queue_send(q, &data) == 0) {
    // Success
}

/* Task B: Receiver */
int rx_val;
queue_receive(q, &rx_val); // Blocks until data arrives
```