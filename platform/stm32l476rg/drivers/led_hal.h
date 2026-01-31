#ifndef LED_HAL_H
#define LED_HAL_H

#include "gpio.h"
#include "gpio_hal.h"

/* User LED (LD2) is connected to PA5 on Nucleo-L476RG */
#define LED_HAL_PORT    GPIO_PORT_A
#define LED_HAL_PIN     5

/**
 * @brief Initialize the LED hardware.
 * Configures the GPIO pin as output.
 */
static inline void led_hal_init(void) {
    gpio_init(LED_HAL_PORT, LED_HAL_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
}

/**
 * @brief Turn the LED on.
 */
static inline void led_hal_on(void) {
    gpio_write(LED_HAL_PORT, LED_HAL_PIN, 1);
}

/**
 * @brief Turn the LED off.
 */
static inline void led_hal_off(void) {
    gpio_write(LED_HAL_PORT, LED_HAL_PIN, 0);
}

/**
 * @brief Toggle the LED state.
 */
static inline void led_hal_toggle(void) {
    gpio_toggle(LED_HAL_PORT, LED_HAL_PIN);
}

#endif /* LED_HAL_H */