#ifndef EXTI_HAL_NATIVE_H
#define EXTI_HAL_NATIVE_H

#include "exti.h"
#include <stdint.h>

#define EXTI_HAL_MAX_LINES 16U

enum {
    EXTI_TRIGGER_RISING = 0,
    EXTI_TRIGGER_FALLING,
    EXTI_TRIGGER_BOTH
};

void exti_hal_configure(uint8_t pin, uint8_t port, exti_trigger_t trigger);
void exti_hal_enable(uint8_t pin);
void exti_hal_disable(uint8_t pin);

#endif /* EXTI_HAL_NATIVE_H */
