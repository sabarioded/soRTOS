#ifndef BUTTON_HAL_NATIVE_H
#define BUTTON_HAL_NATIVE_H

#include <stdint.h>

void button_hal_init(void);
uint32_t button_hal_read(void);

#endif /* BUTTON_HAL_NATIVE_H */
