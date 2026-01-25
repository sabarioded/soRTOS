# soRTOS

**A lightweight, preemptive Real-Time Operating System written from scratch.**

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-STM32L476-green.svg)
![Architecture](https://img.shields.io/badge/arch-ARM%20Cortex--M4-orange.svg)

## What is soRTOS?

This is a real-time operating system kernel I built from scratch in C.

I created it to deeply understand OS internals—scheduling, context switching, and memory management. While the current implementation runs on an **STM32L476RG (Cortex-M4)**, the kernel itself is **platform-agnostic**.

The architecture strictly separates the core kernel logic (scheduler, queues, mutexes) from the hardware-specific drivers and assembly code. This means porting it to a different architecture (like RISC-V or AVR) just requires implementing the context switch and a timer driver.

## Project Structure

I organized the code to keep the generic kernel logic separate from hardware specifics:

*   **`kernel/`**: The heart of the OS (Scheduler, Allocator, IPC). It knows nothing about the specific chip.
*   **`arch/`**: The low-level glue. Contains assembly for context switching and interrupt handling.
*   **`platform/`**: Board-specific setup (Clocks, Linker Scripts, Startup code).
*   **`drivers/`**: Drivers for peripherals like UART and GPIO.
*   **`app/`**: Where the user application and `main()` reside.

## Under the Hood

### The Kernel
*   **Weighted Fair Scheduling:** I implemented a **Stride Scheduler** instead of a basic Round-Robin one. This guarantees that high-priority tasks get proportionally more CPU time without starving low-priority ones.
*   **O(1) Scheduling:** Ready tasks are stored in a **Min-Heap**. Picking the next task is instant, regardless of how many tasks are running.

### Memory Management
*   **TLSF Allocator:** Standard `malloc` is unpredictable. I used the **Two-Level Segregated Fit** algorithm, which guarantees O(1) allocation and deallocation time. It handles fragmentation much better than a simple free-list.
*   **Stack Protection:** Every task stack has a "canary" value at the bottom. The scheduler checks this on every context switch to catch overflows early.

### IPC & Synchronization
*   **Mutexes with Priority Inheritance:** To prevent priority inversion (where a low-priority task holding a lock blocks a high-priority task), the mutex automatically boosts the owner's priority.
*   **Lock-Free Queues (mostly):** The queues use fine-grained spinlocks and are safe to use from Interrupt Service Routines (ISRs).

### The Shell (CLI)
*   **It's not just `printf`:** The CLI runs as its own task. It uses non-blocking UART queues, so typing a command doesn't stall the system.
*   **Features:** It supports history, arrow-key navigation (VT100), and allows you to inspect system state (tasks, heap usage) at runtime.

## How to Write an App

Using the OS is straightforward. You define a function, create a task, and start the scheduler.

```c
void my_task(void *arg) {
    while (1) {
        // Do work
        cli_printf("Hello World!\n");
        
        // Sleep for 1000 ticks (1 second)
        task_sleep_ticks(1000);
    }
}

int main(void) {
    system_init(); // Setup clocks and drivers
    
    // Create a task with 1KB stack and normal priority
    task_create(my_task, NULL, STACK_SIZE_1KB, TASK_WEIGHT_NORMAL);
    
    scheduler_start(); // Hand over control to the kernel
    return 0;
}
```

## Configuration

You can tune the system in `config/project_config.h`. This lets you adjust time slices, stack sizes, heap limits, and tick frequency without modifying the core kernel code.

## Building the Project

The build system is a standard `Makefile`. It supports two targets: the embedded hardware and a native host simulation for testing.

### Prerequisites
*   `arm-none-eabi-gcc` (For the STM32 build)
*   `gcc` (For running unit tests on PC)
*   `openocd` (For flashing)
*   `make`

### 1. Build for Target (STM32)
This compiles the kernel, drivers, and application code into an ELF file for the board.
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
