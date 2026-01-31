#ifndef EXTI_H
#define EXTI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EXTI_TRIGGER_RISING = 0,
    EXTI_TRIGGER_FALLING,
    EXTI_TRIGGER_BOTH
} exti_trigger_t;

/**
 * @brief Callback function type for EXTI interrupts.
 */
typedef void (*exti_callback_t)(void *arg);

/**
 * @brief Configure an external interrupt line.
 * 
 * @param pin The pin number (0-15), which corresponds to the EXTI line.
 * @param port The GPIO port index (0=A, 1=B, etc.) to map to this line.
 * @param trigger The trigger condition (Rising, Falling, Both).
 * @param callback Function to call when interrupt occurs.
 * @param arg Argument to pass to the callback.
 * @return 0 on success, -1 on error.
 */
int exti_configure(uint8_t pin, uint8_t port, exti_trigger_t trigger, exti_callback_t callback, void *arg);

/**
 * @brief Enable the interrupt for the specified line.
 */
void exti_enable(uint8_t pin);

/**
 * @brief Disable the interrupt for the specified line.
 */
void exti_disable(uint8_t pin);

/**
 * @brief Core handler called by platform ISRs.
 * @param pin The pin/line number that triggered.
 */
void exti_core_irq_handler(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* EXTI_H */