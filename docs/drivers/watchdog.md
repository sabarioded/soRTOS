# Watchdog Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Usage Examples](#usage-examples)

---

## Overview

The Watchdog driver provides a platform-agnostic API for system monitoring. It resets the system if the application fails to periodically refresh the watchdog, helping detect and recover from software hangs.

### Key Features

- Configurable timeout period
- Independent operation (runs on separate clock)
- System reset on timeout
- Periodic refresh mechanism

---

## Architecture

The watchdog driver exposes a generic interface and delegates all hardware-specific configuration to the platform HAL. The watchdog must be periodically "kicked" to prevent automatic system reset, ensuring the application is running correctly.

**Logic Flow:**

1.  **Init:** Application sets a timeout (e.g., 5000ms).
2.  **Run:** The platform watchdog peripheral counts down.
3.  **Kick:** Application calls `watchdog_kick()` resets the counter to the reload value.
4.  **Timeout:** If the counter reaches 0 (Application hung):
    *   **Action:** The Watchdog hardware forces a System Reset.

---



## Usage Examples

### Basic Watchdog Usage
```c
#include "watchdog.h"

// Initialize watchdog with 5 second timeout
if (watchdog_init(5000) == 0) {
    while (1) {
        // Main application loop
        
        // Periodic tasks
        do_work();
        
        // Kick watchdog to show we're alive
        watchdog_kick();
        
        // Small delay
    }
}
```

### Task-Based Watchdog
```c
#include "watchdog.h"

void main_task(void) {
    // Initialize watchdog
    watchdog_init(2000); // 2 second timeout
    
    while (1) {
        // Do main work
        process_data();
        
        // Check subsystems
        if (check_subsystem_health()) {
            watchdog_kick();
        }
        
        // Task delay
        task_delay(500); // 500ms
    }
}
```

---
