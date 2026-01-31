#ifndef BUTTON_HAL_H
#define BUTTON_HAL_H

#include "gpio.h"
#include "gpio_hal.h"

/* User Button (B1) is connected to PC13 on Nucleo-L476RG */
#define BUTTON_HAL_PORT GPIO_PORT_C
#define BUTTON_HAL_PIN  13

/**
 * @brief Initialize the button hardware.
 * Configures the GPIO pin as input.
 */
static inline void button_hal_init(void) {
    gpio_init(BUTTON_HAL_PORT, BUTTON_HAL_PIN, GPIO_MODE_INPUT, GPIO_PULL_NONE, 0);
}

/**
 * @brief Read the raw button state.
 * Handles active-low logic (returns 1 if pressed).
 * 
 * @return 1 if pressed, 0 if released.
 */
static inline uint32_t button_hal_read(void) {
    /* Button is active low (connected to ground when pressed) */
    return !gpio_read(BUTTON_HAL_PORT, BUTTON_HAL_PIN);
}

#endif /* BUTTON_HAL_H */