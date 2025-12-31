#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include "device_registers.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize user button driver
 */
void button_init(void);

/**
 * @brief read the state of the button
 * @return 1 if the button is pressed, 0 otherwise.
 */
uint32_t button_read(void);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */