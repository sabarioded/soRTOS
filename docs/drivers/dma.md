# DMA Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Protocol](#protocol)
- [Usage Examples](#usage-examples)

---

## Overview

The DMA (Direct Memory Access) driver provides an interface for high-speed data transfers between memory and peripherals without CPU intervention. It supports configurable transfer parameters and channel management.

---

## Architecture

The DMA driver manages DMA channels and provides a high-level interface for initiating and controlling data transfers. It delegates hardware-specific operations to the HAL layer.

**Data Flow:**

1.  **Application:** Configures a DMA transfer (Src, Dst, Size).
2.  **DMA Driver:** Sets up the DMA channel and enables the stream.
3.  **DMA Controller:** Takes over the bus matrix.
4.  **Hardware:** Transfers data directly between Memory and Peripherals (or Memory-to-Memory) independent of the CPU.

---



---

## Protocol

DMA transfers follow a specific setup and execution lifecycle:

```text
1. Setup Phase
   [Application] -> dma_start() -> [DMA Driver] -> [DMA Controller]
                                         (Configures & Enables Stream)

2. Transfer Phase (Parallel)
   .----------------------------.       .-----------------------------.
   |      CPU Execution         |       |      DMA Transfer           |
   |----------------------------|       |-----------------------------|
   | CPU continues to execute   |       | DMA Controller manages bus: |
   | application code without   |       | [Memory] <===> [Peripheral] |
   | blocking.                  |       | (Data moves independently)  |
   '----------------------------'       '-----------------------------'

3. Completion Phase
   [DMA Controller] -> Interrupt (HAL-specific) -> [Application]
```

1.  **Configuration:** Driver sets source/destination addresses and transfer size.
2.  **Arbitration:** DMA Controller requests bus access.
3.  **Transfer:** Data moves independent of CPU.
4.  **Completion:** The platform HAL signals end of transfer (interrupt or polling).

---

## Usage Examples

### Memory-to-Memory Transfer
```c
#include "dma.h"

// Create DMA channel
dma_channel_t dma = dma_create(&dma_channel, &dma_config);
if (dma == NULL) {
    // Handle error
}

// Start transfer
uint8_t src[100];
uint8_t dst[100];
if (dma_start(dma, src, dst, 100) == 0) {
    // Transfer started successfully
    // Wait for completion or use interrupts
}

// Clean up
dma_destroy(dma);
```

---
