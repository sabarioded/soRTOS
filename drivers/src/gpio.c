#include "gpio.h"
#include "gpio_hal.h"

/* Initialize a GPIO pin */
void gpio_init(gpio_port_t port, uint8_t pin, gpio_mode_t mode, gpio_pull_t pull, uint8_t af) {
    gpio_hal_init(port, pin, mode, pull, af);
}

/* Write a value to a GPIO pin */
void gpio_write(gpio_port_t port, uint8_t pin, uint8_t value) {
    gpio_hal_write(port, pin, value);
}

/* Toggle a GPIO pin */
void gpio_toggle(gpio_port_t port, uint8_t pin) {
    gpio_hal_toggle(port, pin);
}

/* Read the value of a GPIO pin */
uint8_t gpio_read(gpio_port_t port, uint8_t pin) {
    return gpio_hal_read(port, pin);
}
