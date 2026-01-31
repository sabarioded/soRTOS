#ifndef GPIO_HAL_STM32_H
#define GPIO_HAL_STM32_H

#include "device_registers.h"
#include "gpio.h"
#include <stddef.h>

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

/* GPIO Mode Register bits and mask */
#define GPIO_MODER_BITS         2U
#define GPIO_MODER_MASK         3U

/* GPIO Pull-up/Pull-down Register bits and mask */
#define GPIO_PUPDR_BITS         2U
#define GPIO_PUPDR_MASK         3U

/* GPIO Alternate Function Register bits and mask */
#define GPIO_AFR_BITS           4U
#define GPIO_AFR_MASK           0xFU
#define GPIO_AFR_PINS_PER_REG   8U

/* GPIO Bit Set/Reset Register offset for reset bits */
#define GPIO_BSRR_RESET_OFFSET  16U

/* Helper to get the base address of a GPIO port */
static inline GPIO_t* gpio_hal_get_port(gpio_port_t port) {
    switch(port) {
        case GPIO_PORT_A: return GPIOA;
        case GPIO_PORT_B: return GPIOB;
        case GPIO_PORT_C: return GPIOC;
        case GPIO_PORT_D: return GPIOD;
        case GPIO_PORT_E: return GPIOE;
        case GPIO_PORT_H: return GPIOH;
        default: return NULL;
    }
}

/* Helper to get the RCC clock enable bit for a GPIO port */
static inline uint32_t gpio_hal_get_clock_mask(gpio_port_t port) {
    switch(port) {
        case GPIO_PORT_A: return (1U << 0);
        case GPIO_PORT_B: return (1U << 1);
        case GPIO_PORT_C: return (1U << 2);
        case GPIO_PORT_D: return (1U << 3);
        case GPIO_PORT_E: return (1U << 4);
        case GPIO_PORT_H: return (1U << 7);
        default: return 0;
    }
}

/* Initialize a GPIO pin with the specified configuration */
static inline void gpio_hal_init(gpio_port_t port, uint8_t pin, gpio_mode_t mode, gpio_pull_t pull, uint8_t af) {
    GPIO_t *gpio = gpio_hal_get_port(port);
    if (!gpio) {
        return;
    }

    /* Enable GPIO Clock */
    RCC->AHB2ENR |= gpio_hal_get_clock_mask(port);

    /* Configure Mode (2 bits per pin) */
    gpio->MODER &= ~(GPIO_MODER_MASK << (pin * GPIO_MODER_BITS));
    gpio->MODER |= ((uint32_t)mode << (pin * GPIO_MODER_BITS));

    /* Configure Pull-up/Pull-down (2 bits per pin) */
    gpio->PUPDR &= ~(GPIO_PUPDR_MASK << (pin * GPIO_PUPDR_BITS));
    gpio->PUPDR |= ((uint32_t)pull << (pin * GPIO_PUPDR_BITS));

    /* Configure Alternate Function (4 bits per pin) */
    if (mode == GPIO_MODE_AF) {
        int afr_idx = pin / GPIO_AFR_PINS_PER_REG; /* 0 for pins 0-7, 1 for pins 8-15 */
        int afr_shift = (pin % GPIO_AFR_PINS_PER_REG) * GPIO_AFR_BITS; 
        
        gpio->AFR[afr_idx] &= ~(GPIO_AFR_MASK << afr_shift);
        gpio->AFR[afr_idx] |= ((uint32_t)af << afr_shift);
    }
}

/* Write a value to a GPIO pin */
static inline void gpio_hal_write(gpio_port_t port, uint8_t pin, uint8_t value) {
    GPIO_t *gpio = gpio_hal_get_port(port);
    if (!gpio) {
        return;
    }

    if (value) {
        gpio->BSRR = (1U << pin);
    } else {
        gpio->BSRR = (1U << (pin + GPIO_BSRR_RESET_OFFSET));
    }
}

/* Toggle the state of a GPIO pin */
static inline void gpio_hal_toggle(gpio_port_t port, uint8_t pin) {
    GPIO_t *gpio = gpio_hal_get_port(port);
    if (!gpio) {
        return;
    }

    /* Atomic toggle using ODR check and BSRR write */
    if (gpio->ODR & (1U << pin)) {
        gpio->BSRR = (1U << (pin + GPIO_BSRR_RESET_OFFSET)); /* Reset */
    } else {
        gpio->BSRR = (1U << pin); /* Set */
    }
}

/* Read the state of a GPIO pin */
static inline uint8_t gpio_hal_read(gpio_port_t port, uint8_t pin) {
    GPIO_t *gpio = gpio_hal_get_port(port);
    if (!gpio) {
        return 0;
    }

    return (gpio->IDR & (1U << pin)) != 0;
}

#endif /* GPIO_HAL_STM32_H */