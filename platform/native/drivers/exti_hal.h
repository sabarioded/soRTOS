#ifndef EXTI_HAL_NATIVE_H
#define EXTI_HAL_NATIVE_H

#include <stdint.h>

void exti_hal_configure(uint8_t pin, uint8_t port, int trigger);
void exti_hal_enable(uint8_t pin);
void exti_hal_disable(uint8_t pin);

#endif /* EXTI_HAL_NATIVE_H */
