#ifndef GPIO_HAL_NATIVE_H
#define GPIO_HAL_NATIVE_H

#include "gpio.h"
#include <stdint.h>

/* Platform specific GPIO definitions */
enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B,
    GPIO_PORT_C,
    GPIO_PORT_D,
    GPIO_PORT_E,
    GPIO_PORT_H,
    GPIO_PORT_MAX
};

enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_AF,
    GPIO_MODE_ANALOG
};

enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN
};

void gpio_hal_init(gpio_port_t port, uint8_t pin, gpio_mode_t mode, gpio_pull_t pull, uint8_t af);
void gpio_hal_write(gpio_port_t port, uint8_t pin, uint8_t value);
void gpio_hal_toggle(gpio_port_t port, uint8_t pin);
uint8_t gpio_hal_read(gpio_port_t port, uint8_t pin);

#endif /* GPIO_HAL_NATIVE_H */
