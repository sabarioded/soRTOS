# Flash Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Protocol](#protocol)
- [Usage Examples](#usage-examples)

---

## Overview

The Flash driver provides an interface for programming and erasing microcontroller flash memory. Reads are performed directly from memory-mapped flash. It handles the programming sequence including unlock/lock operations and page-based erase.

**Note:** `flash_read` takes a `uintptr_t` address so it works on 32-bit and 64-bit hosts.

---

## Architecture

The flash driver provides direct access to the microcontroller's internal flash memory, implementing the necessary sequence for safe programming operations.

**Operation:**

1.  **Application:** Initiates a Program or Erase command.
2.  **Flash Driver:** Unlocks the Flash Control Register.
3.  **Flash Controller:** Executes the high-voltage operations to modify the Non-Volatile Memory (NVM) cells.
4.  **Flash Memory:** Stores the new data permanently.

---



---

## Protocol

Safe Flash programming requires a strict state machine flow:

```mermaid
graph TD
    Idle[Locked State] -->|flash_unlock| Unlocked[Unlocked State]
    Unlocked -->|flash_erase_page| Erasing[BSY: Erasing Page]
    Erasing --> Unlocked
    Unlocked -->|flash_program| Prog[BSY: Programming Word]
    Prog --> Unlocked
    Unlocked -->|flash_lock| Idle
```

1.  **Unlock:** Write keys to Flash Key Register to allow write access.
2.  **Erase:** Set Page Erase bit, select page, and start. Wait for Busy flag.
3.  **Program:** Write data to address. Hardware handles the high-voltage timer. Wait for Busy flag. Alignment and program granularity are platform-specific.
4.  **Lock:** Set Lock bit to protect memory.

---

## Usage Examples

### Writing Data to Flash
```c
#include "flash.h"

// Unlock flash
flash_unlock();

// Erase the page first
if (flash_erase_page(FLASH_USER_START_ADDR) == 0) {
// Program data (STM32L4 requires 64-bit aligned address and length)
uint64_t data = 0x1122334455667788ULL;
if (flash_program(FLASH_USER_START_ADDR, &data, sizeof(data)) == 0) {
    // Success
}
}

// Lock flash
flash_lock();
```

### Reading Data from Flash
```c
#include "flash.h"

uint64_t data = 0;
uintptr_t addr = (uintptr_t)FLASH_USER_START_ADDR;
if (flash_read(addr, &data, sizeof(data)) == 0) {
    // Use data
}
```

---
