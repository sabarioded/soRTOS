#ifndef SYSTICK_HAL_NATIVE_H
#define SYSTICK_HAL_NATIVE_H

#include <stdint.h>

int systick_hal_init(uint32_t reload_val);
void systick_hal_irq_handler(void);

#endif /* SYSTICK_HAL_NATIVE_H */
