# soRTOS

Welcome to **soRTOS**! This is a lightweight, preemptive Real-Time Operating System (RTOS) that I wrote from scratch in C. It started as a project for the STM32L476RG (Cortex-M4), but I've designed it to be modular so the kernel logic is separate from the hardware details. This makes it pretty easy to port to other chips if you want to.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-STM32L476-green.svg)
![Architecture](https://img.shields.io/badge/arch-ARM%20Cortex--M4-orange.svg)

## Key Features

### 1. Preemptive Kernel
I implemented a proper Round-Robin scheduler that handles context switching (using `PendSV` on Cortex-M). You can create and delete tasks dynamically, and put them to sleep with `task_sleep_ticks`. It saves the full register context (including FPU) so your tasks are safe. There's also an Idle task that cleans up memory (garbage collection) and puts the CPU to sleep (`WFI`) to save power when nothing else is running.

### 2. Custom Memory Management
I wrote a custom heap allocator (`malloc`/`free`) using a "Best-Fit" strategy that merges free blocks to keep fragmentation low. It's thread-safe (using critical sections) so interrupts won't corrupt your memory. I also added some CLI commands so you can see exactly what's happening with the heap.

### 3. Interactive CLI
The system comes with a built-in shell. It uses an interrupt-driven UART driver with ring buffers, so it doesn't block the CPU. The command parser handles arguments, and adding your own commands is super simple—just use `cli_register_command`.

### 4. Portable Architecture
I tried to keep the kernel clean by hiding hardware details behind a Platform Abstraction Layer (PAL). The drivers are "Zero-HAL"—I wrote custom register definitions for the STM32L476RG to avoid the bloat of vendor libraries.

## Building and Running

The project uses a portable `Makefile` to handle everything.

### Prerequisites
* `arm-none-eabi-gcc` (for STM32)
* `gcc` (for host tests)
* `openocd` (for flashing)
* `make`

### Build for STM32L476RG (Default)
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
