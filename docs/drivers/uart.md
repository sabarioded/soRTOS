# UART Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Protocol](#protocol)
- [Usage Examples](#usage-examples)
- [Configuration](#configuration)

---

## Overview

The UART (Universal Asynchronous Receiver/Transmitter) driver provides serial communication capabilities with buffered transmit and receive operations. It supports interrupt-driven data transfer for efficient communication.

### Key Features

- Buffered transmit and receive
- Interrupt-driven operation
- Configurable UART parameters

---

## Architecture

The UART driver implements buffered serial communication with separate transmit and receive buffers. It uses interrupts for efficient data handling and provides both blocking and non-blocking operations.

**Data Flow:**

1.  **Application:** Uses the UART Driver API to send or receive data.
2.  **UART Driver:** Manages separate **TX** and **RX** software buffers.
3.  **UART HAL:** Interfaces with the hardware registers.
4.  **Peripheral:** The UART hardware block sends data to/from the physical pins.
    *   **TX:** Data moves from TX Buffer -> Peripheral -> Wire.
    *   **RX:** Data moves from Wire -> Peripheral -> RX Buffer (via Interrupts).

---

## Protocol

UART uses an asynchronous serial protocol with the following frame structure:

```text
      Idle   Start  D0   D1   D2   D3   D4   D5   D6   D7   Stop
Logic 1 ──────┐       ┌────┐    ┌────┬────┐    ┌────┐    ┌────┐
              │       │    │    │    │    │    │    │    │    │
Logic 0       └───────┘    └────┘    │    └────┘    └────┘    └──────
```

1.  **Idle:** Line High.
2.  **Start:** Line Low for 1 bit time.
3.  **Data:** 8 bits sent LSB first.
4.  **Stop:** Line High for 1 (or more) bit times.

---



## Usage Examples

### Basic UART Communication
```c
#include "uart.h"

// Create UART instance
uint8_t rx_buf[256];
uint8_t tx_buf[256];
uart_port_t uart = uart_create(&uart_peripheral, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf), &uart_config, 80000000);
if (uart == NULL) {
    // Handle error
}

// Send data
const char *message = "Hello, World!\n";
uart_write_buffer(uart, message, strlen(message));

// Receive data
if (uart_available(uart) > 0) {
    char buffer[64];
    int bytes_read = uart_read_buffer(uart, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        // Process received data
    }
}

// Clean up
uart_destroy(uart);
```

---

## Configuration

UART configuration includes:
- Baud rate
- Data bits
- Stop bits
- Parity
- Flow control
- Buffer sizes
- Interrupt settings</content>
- Interrupt settings