# I2C Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Protocol](#protocol)
- [Usage Examples](#usage-examples)
- [Configuration](#configuration)

---

## Overview

The I2C (Inter-Integrated Circuit) driver provides a master-mode interface for communicating with I2C slave devices. It supports data transmission and reception with configurable timing parameters.

### Key Features

- Master-mode I2C communication
- 7-bit slave addressing
- Transmit and receive operations
- Configurable I2C parameters (speed, timing)

---

## Architecture

The I2C driver manages I2C bus communication as a master device, handling the protocol timing and data transfer operations through the HAL layer.

The I2C driver abstraction stack:

1.  **Application:** Initiates transfers (e.g., "Write 2 bytes to device 0x68").
2.  **I2C Driver:** Formats the request into an I2C transaction structure.
3.  **I2C HAL:** Manages the low-level signal generation (Start, Stop, ACK/NACK).
4.  **Hardware:** The I2C peripheral drives the **SDA** and **SCL** lines to communicate with Slave Devices.

---

## Protocol

I2C is a synchronous, multi-master, multi-slave packet switched bus.

```text
          Start   Addr (7-bit)   R/W  ACK   Data (8-bit)   ACK  Stop
       __      _   _        _   _    _     _        _   _    __
SDA:  /  \____/ \_/ \_...._/ \_/ \__/ \___/ \_...._/ \__/ \__/  \___
      ____    _   _        _   _    _     _        _   _    ____
SCL:      \__/ \_/ \_...._/ \_/ \__/ \___/ \_...._/ \__/ \_/
```

1.  **Start Condition:** SDA transitions High -> Low while SCL is High.
2.  **Clocking:** Data changes when SCL is Low, sampled when SCL is High.
3.  **Acknowledge:** Receiver pulls SDA Low during the 9th clock pulse.
4.  **Stop Condition:** SDA transitions Low -> High while SCL is High.

---



## Usage Examples

### Communicating with an I2C Sensor
```c
#include "i2c.h"

// Create I2C instance
i2c_port_t i2c = i2c_create(&i2c_peripheral, &i2c_config);
if (i2c == NULL) {
    // Handle error
}

// Write configuration to sensor
uint8_t config_data[] = {0x00, 0x80}; // Register 0x00, value 0x80
if (i2c_master_transmit(i2c, 0x68, config_data, sizeof(config_data)) == 0) {
    // Read sensor data
    uint8_t sensor_data[2];
    if (i2c_master_receive(i2c, 0x68, sensor_data, sizeof(sensor_data)) == 0) {
        // Process sensor data
    }
}

// Clean up
i2c_destroy(i2c);
```

---

## Configuration

I2C configuration includes:
- Clock speed (standard/fast mode)
- Timing parameters
- Address mode (7-bit)
- Pull-up configurations</content>
