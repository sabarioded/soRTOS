# SysTick Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Usage Examples](#usage-examples)
- [Configuration](#configuration)

---

## Overview

The SysTick driver provides system timing functionality using the system tick timer. It generates periodic interrupts for task scheduling and provides timing utilities for delays and uptime tracking.

### Key Features

- Configurable tick frequency
- System uptime tracking
- Blocking delay functions
- Integration with RTOS scheduler

---

## Architecture

The SysTick driver configures the system tick timer to generate periodic interrupts. Each interrupt increments a tick counter and triggers the scheduler, providing the heartbeat for the real-time operating system.

**System Heartbeat:**

1.  **SysTick Driver:** Configures the hardware timer reload value based on CPU clock.
2.  **System Tick Timer:** Counts down from reload value to zero.
3.  **Interrupt Handler:** Fires when counter hits zero.
    *   Increments the global **Tick Counter** (Uptime).
    *   Triggers the **Scheduler** (Context Switch).

---



## Usage Examples

### Basic Timing
```c
#include "systick.h"

// Initialize SysTick at 1000 Hz (1ms ticks)
if (systick_init(1000) == 0) {
    // Get current uptime
    uint32_t uptime = systick_get_ticks();
    
    // Delay for 100 ticks (100ms)
    systick_delay_ticks(100);
}
```

### Periodic Tasks
```c
#include "systick.h"

uint32_t last_tick = 0;

void periodic_task(void) {
    uint32_t current_tick = systick_get_ticks();
    
    if (current_tick - last_tick >= 1000) { // Every 1000 ticks (1 second at 1000Hz)
        // Do periodic work
        last_tick = current_tick;
    }
}
```

---

## Configuration

SysTick configuration requires:
- Tick frequency selection
- Interrupt priority setting
- Clock source selection

The tick frequency affects timing resolution and system performance.
