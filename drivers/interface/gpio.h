#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO Port identifiers.
 */
typedef uint8_t gpio_port_t;

/**
 * @brief GPIO Pin Modes.
 */
typedef uint8_t gpio_mode_t;

/**
 * @brief GPIO Pull-up/Pull-down configurations.
 */
typedef uint8_t gpio_pull_t;

/**
 * @brief Initialize a GPIO pin.
 * 
 * @param port The GPIO port.
 * @param pin The pin number.
 * @param mode The mode (Input, Output, AF, Analog).
 * @param pull The pull-up/down configuration.
 * @param af The alternate function number (0-15), used only if mode is GPIO_MODE_AF.
 */
void gpio_init(gpio_port_t port, uint8_t pin, gpio_mode_t mode, gpio_pull_t pull, uint8_t af);

/**
 * @brief Write a value to a GPIO pin.
 * 
 * @param port The GPIO port.
 * @param pin The pin number.
 * @param value 0 for low, non-zero for high.
 */
void gpio_write(gpio_port_t port, uint8_t pin, uint8_t value);

/**
 * @brief Toggle the state of a GPIO pin.
 * 
 * @param port The GPIO port.
 * @param pin The pin number.
 */
void gpio_toggle(gpio_port_t port, uint8_t pin);

/**
 * @brief Read the state of a GPIO pin.
 * 
 * @param port The GPIO port.
 * @param pin The pin number.
 * @return uint8_t 1 if high, 0 if low.
 */
uint8_t gpio_read(gpio_port_t port, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_H */
