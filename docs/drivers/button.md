# Button Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Usage Examples](#usage-examples)

---

## Overview

The Button driver provides debounced button input functionality for user interface buttons. It handles contact bounce and provides stable press detection with event-based notifications.

---

## Architecture

The button driver implements a simple state machine for debouncing and event detection. It requires periodic polling to update the debounced state and detect press events.

**Processing Pipeline:**

1.  **Hardware Button:** Mechanical switch (noisy signal).
2.  **button_read():** Reads raw GPIO state.
3.  **Debounce Logic:**
    *   Filters out rapid transitions (bounce).
    *   Confirms a stable state over time.
4.  **Events:**
    *   **Stable State:** Current debounced level (Pressed/Released).
    *   **Press Events:** Rising/Falling edge detection.

---



## Usage Examples

### Basic Button Handling
```c
#include "button.h"

// Initialize button
button_init();

// In main loop or timer callback
void button_task(void) {
    // Poll button state (call every 10ms)
    button_poll();
    
    // Check for press events
    if (button_was_pressed()) {
        // Handle button press
    }
    
    // Check current state
    if (button_is_held()) {
        // Button is being held
    }
}
```

---
