#ifndef EXTI_HAL_STM32_H
#define EXTI_HAL_STM32_H

#include "device_registers.h"
#include "exti.h"

enum {
    EXTI_TRIGGER_RISING = 0,
    EXTI_TRIGGER_FALLING,
    EXTI_TRIGGER_BOTH
};

/* SYSCFG Clock Enable */
#define RCC_APB2ENR_SYSCFGEN    (1U << 0)

/**
 * @brief Configure the hardware for a specific EXTI line.
 */
static inline void exti_hal_configure(uint8_t pin, uint8_t port, exti_trigger_t trigger) {
    if (pin > 15) return;

    /* Enable SYSCFG Clock */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* Configure SYSCFG_EXTICR to map Port to Pin */
    /* EXTICR[0] for pins 0-3, [1] for 4-7, etc. */
    uint8_t reg_idx = pin / 4;
    uint8_t shift = (pin % 4) * 4;
    
    /* Clear the 4 bits for this pin */
    SYSCFG->EXTICR[reg_idx] &= ~(0xFU << shift);
    /* Set the port value (0=PA, 1=PB, etc.) */
    SYSCFG->EXTICR[reg_idx] |= (port << shift);

    /* Configure Trigger */
    /* Disable interrupt mask while configuring */
    EXTI->IMR1 &= ~(1U << pin);

    /* Configure Rising Edge */
    if (trigger == EXTI_TRIGGER_RISING || trigger == EXTI_TRIGGER_BOTH) {
        EXTI->RTSR1 |= (1U << pin);
    } else {
        EXTI->RTSR1 &= ~(1U << pin);
    }

    /* Configure Falling Edge */
    if (trigger == EXTI_TRIGGER_FALLING || trigger == EXTI_TRIGGER_BOTH) {
        EXTI->FTSR1 |= (1U << pin);
    } else {
        EXTI->FTSR1 &= ~(1U << pin);
    }
}

/**
 * @brief Enable the EXTI interrupt mask.
 */
static inline void exti_hal_enable(uint8_t pin) {
    if (pin <= 15) {
        EXTI->IMR1 |= (1U << pin);
    }
}

/**
 * @brief Disable the EXTI interrupt mask.
 */
static inline void exti_hal_disable(uint8_t pin) {
    if (pin <= 15) {
        EXTI->IMR1 &= ~(1U << pin);
    }
}

/**
 * @brief Helper to be called from the platform ISR.
 * Checks pending flag, clears it, and calls core handler.
 */
static inline void exti_hal_irq_handler(uint8_t pin) {
    /* Check if the interrupt is pending */
    if (EXTI->PR1 & (1U << pin)) {
        /* Clear the pending bit by writing 1 to it */
        EXTI->PR1 = (1U << pin);
        
        /* Call the generic driver handler */
        exti_core_irq_handler(pin);
    }
}

#endif /* EXTI_HAL_STM32_H */
