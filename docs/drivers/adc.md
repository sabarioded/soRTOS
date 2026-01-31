# ADC Driver

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Protocol](#protocol)
- [Usage Examples](#usage-examples)
- [Configuration](#configuration)

---

## Overview

The ADC (Analog-to-Digital Converter) driver provides an interface for reading analog voltage levels from microcontroller pins. It supports single-channel conversions with configurable resolution and sampling parameters.

### Key Features

- Single-channel analog-to-digital conversion
- Configurable ADC parameters through HAL configuration
- Support for multiple ADC channels
- Memory-efficient context management

---

## Architecture

The ADC driver wraps the platform-specific HAL to provide a consistent API for analog input operations. It manages ADC context and delegates hardware-specific operations to the HAL layer.

The ADC driver operation:

1.  **Application:** Requests a conversion on a specific channel.
2.  **ADC Driver:** Selects the channel and triggers the start of conversion.
3.  **ADC HAL:** Configures the multiplexer and control registers.
4.  **Hardware:** Samples the analog voltage, performs the approximation, and stores the digital result in the data register.

---



## Protocol

The Analog-to-Digital conversion process:

```mermaid
graph LR
    Start((Start)) --> Sample[Sample Stage\n(Charge Capacitor)]
    Sample --> Hold[Hold Stage\n(Disconnect Input)]
    Hold --> Convert{SAR Conversion\n(Bit-by-Bit)}
    Convert --> Result[Store in\nData Register]
    Result --> Irq[Interrupt/\nDMA Request]
```

1.  **Sampling:** The input voltage charges an internal capacitor.
2.  **Hold:** The capacitor is disconnected from the input to stabilize the voltage.
3.  **Conversion:** Successive Approximation Register (SAR) logic determines the digital value.
4.  **Result:** The final 12-bit (or configured resolution) value is stored.

---

## Usage Examples

### Basic ADC Reading
```c
#include "adc.h"

// Create ADC instance
adc_port_t adc = adc_create(&adc_peripheral, &adc_config);
if (adc == NULL) {
    // Handle error
}

// Read from channel 0
uint16_t value;
if (adc_read_channel(adc, 0, &value) == 0) {
    // Use the ADC value
    float voltage = (value / 4095.0f) * 3.3f; // Assuming 12-bit ADC, 3.3V reference
}

// Clean up
adc_destroy(adc);
```

---

## Configuration

ADC configuration is handled through the HAL-specific configuration structure passed during initialization. This typically includes parameters such as:
- ADC resolution
- Sampling time
- Reference voltage
- Conversion mode</content>
- Conversion mode