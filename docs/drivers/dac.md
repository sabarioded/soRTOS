# DAC Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Usage Examples](#usage-examples)

---

## Overview

The DAC (Digital-to-Analog Converter) driver provides an interface for generating analog output voltages from digital values. It supports dual-channel operation with 12-bit resolution.

### Key Features

- Dual-channel DAC output
- 12-bit resolution
- Simple initialization and write operations

---

## Architecture

The DAC driver directly interfaces with the microcontroller's DAC peripheral, providing a straightforward API for analog output generation.

**Signal Flow:**

1.  **Application:** Writes a digital values (e.g., 2048).
2.  **DAC Driver:** Selects the target channel.
3.  **DAC Hardware:** Converts the digital value to a corresponding voltage level on the output pin.

---



## Usage Examples

### Basic DAC Output
```c
#include "dac.h"
#include "dac_hal.h"

// Initialize DAC channel 1
if (dac_init(DAC_CHANNEL_1) == 0) {
    // Set output to mid-scale (1.65V for 3.3V reference)
    dac_write(DAC_CHANNEL_1, 2048);
}
```

---
