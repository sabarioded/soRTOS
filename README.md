# soRTOS

**A lightweight, preemptive Real-Time Operating System written from scratch.**

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-STM32L476-green.svg)
![Architecture](https://img.shields.io/badge/arch-ARM%20Cortex--M4-orange.svg)

## What is this?

Hi! Welcome to **soRTOS**. This is a hobby project where I built a fully functional RTOS kernel in C. It started as a way to deeply understand how operating systems work on bare metal—specifically for the STM32L476RG (Cortex-M4)—but it has grown into a modular system that separates the kernel logic from the hardware.

If you're interested in embedded systems, context switching, or just want to see how `malloc` works under the hood, you've come to the right place.

## What's Inside?

I've implemented the core primitives you'd expect in a modern RTOS, keeping the code clean and readable.

### The Kernel
*   **Preemptive Scheduler**: A **Stride Scheduler** (Weighted Fair Queuing) backed by a Min-Heap. It ensures deterministic fairness based on task weights (no starvation!) and scales efficiently (O(log N)). Context switching uses `PendSV` and saves the full CPU context (including FPU).
*   **Task Management**: Create and delete tasks dynamically. Assign weights to tasks to control CPU allocation. There's even a garbage collector running in the Idle task to clean up stack memory from deleted tasks.
*   **Software Timers**: Need to blink an LED or timeout an operation? You can schedule one-shot or periodic callbacks without burning a whole task for it.

### Synchronization & IPC
*   **Mutexes**: With "Direct Handoff" optimization to prevent priority inversion and race conditions upon wake-up.
*   **Semaphores**: Counting semaphores for resource tracking, complete with `broadcast` capabilities.
*   **Message Queues**: Thread-safe, blocking queues for passing data between tasks (or from ISRs to tasks). Supports peeking and resetting.
*   **Task Notifications**: Lightweight signals to wake up specific tasks directly.

### Memory Management
*   **Custom Allocator**: I wrote a "Best-Fit" heap allocator with coalescing. It handles `malloc`, `free`, and `realloc` safely (thread-safe!).
*   **Diagnostics**: You can inspect heap fragmentation and usage in real-time via the CLI.

### The Shell (CLI)
*   **Interactive**: Connect via UART and type commands.
*   **Non-Blocking**: The UART driver uses interrupt-driven ring buffers, so the CPU stays free while you type.
*   **Extensible**: Adding a custom command is as easy as calling `cli_register_command`.

## Building and Running

I use a standard `Makefile` to keep things portable. You can run this on an STM32 board or run the unit tests right on your PC.

### Prerequisites
*   `arm-none-eabi-gcc` (for STM32 builds)
*   `gcc` (for running tests on your host)
*   `openocd` (for flashing the board)
*   `make`

### 1. Build for STM32
Compiles the kernel, drivers, and app for the STM32L476RG.
```bash
make
```

### Flash to Board
```bash
make load
```

### Run Unit Tests (Host)
```bash
make test
```
