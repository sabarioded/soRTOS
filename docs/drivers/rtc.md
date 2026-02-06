# RTC Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Usage Examples](#usage-examples)
- [Configuration](#configuration)

---

## Overview

The RTC (Real-Time Clock) driver provides time and date keeping functionality using the microcontroller's RTC peripheral. It supports getting and setting time and date with automatic calendar calculations.

---

## Architecture

The RTC driver interfaces with the hardware RTC peripheral, which maintains time independently of the main system clock and can operate during low-power modes.

---



## Usage Examples

### Setting and Reading Time
```c
#include "rtc.h"
#include "rtc_hal.h"

// Initialize RTC
if (rtc_init() == 0) {
    // Set initial time
    rtc_time_t time = {12, 30, 45}; // 12:30:45
    rtc_set_time(&time);
    
    rtc_date_t date = {15, 6, 23, 3}; // June 15, 2023, Wednesday
    rtc_set_date(&date);
    
    // Read current time
    rtc_time_t current_time;
    rtc_get_time(&current_time);
    
    // Use time values
    uint8_t hours = current_time.hours;
    uint8_t minutes = current_time.minutes;
}
```

---

## Configuration

The RTC driver uses a low-speed clock source for operation. Configuration includes:
- Clock source selection
- Initial time/date setting
- Alarm and wakeup configurations (if supported)
