#include "exti.h"
#include "exti_hal.h"
#include <stddef.h>

typedef struct {
    exti_callback_t callback;
    void *arg;
} exti_handler_t;

/* Storage for callbacks for EXTI lines supported by the HAL */
static exti_handler_t g_exti_handlers[EXTI_HAL_MAX_LINES];

int exti_configure(uint8_t pin, uint8_t port, exti_trigger_t trigger, exti_callback_t callback, void *arg) {
    if (pin >= EXTI_HAL_MAX_LINES) {
        return -1;
    }

    g_exti_handlers[pin].callback = callback;
    g_exti_handlers[pin].arg = arg;

    exti_hal_configure(pin, port, trigger);
    return 0;
}

void exti_enable(uint8_t pin) {
    if (pin < EXTI_HAL_MAX_LINES) {
        exti_hal_enable(pin);
    }
}

void exti_disable(uint8_t pin) {
    if (pin < EXTI_HAL_MAX_LINES) {
        exti_hal_disable(pin);
    }
}

void exti_core_irq_handler(uint8_t pin) {
    if (pin < EXTI_HAL_MAX_LINES && g_exti_handlers[pin].callback) {
        g_exti_handlers[pin].callback(g_exti_handlers[pin].arg);
    }
}
