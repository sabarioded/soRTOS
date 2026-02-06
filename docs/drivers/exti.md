# EXTI Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Usage Examples](#usage-examples)
- [Configuration](#configuration)

---

## Overview

The EXTI (External Interrupt) driver provides an interface for handling external interrupts from GPIO pins. It supports configurable trigger conditions and callback-based interrupt handling.

---

## Architecture

The EXTI driver configures external interrupt lines and manages interrupt callbacks. It integrates with the platform's interrupt service routines for handling external events.

**Signal Path:**

1.  **GPIO Pin:** Detects a signal change (Rising/Falling edge).
2.  **EXTI Line:** Maps the specific pin to an interrupt line.
3.  **Interrupt Controller:** Triggers the ISR.
4.  **EXTI Driver:** Executes the registered **Callback Function** associated with that pin.

---



## Usage Examples

### Button Interrupt
```c
#include "exti.h"
#include "exti_hal.h"

// Callback function
void button_callback(void *arg) {
    // Handle button press
}

// Configure interrupt on pin 0, port A, rising edge
if (exti_configure(0, 0, EXTI_TRIGGER_RISING, button_callback, NULL) == 0) {
    exti_enable(0);
}
```

---

## Configuration

EXTI configuration involves:
- Selecting the GPIO pin and port
- Choosing the trigger type
- Providing a callback function
- Enabling the interrupt after configuration
- Enable/disable interrupt control
