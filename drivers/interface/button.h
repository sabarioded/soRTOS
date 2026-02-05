#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize user button driver
 */
void button_init(void);

/**
 * @brief Read the raw state of the button.
 * @return 1 if the button is physically pressed, 0 otherwise.
 */
uint32_t button_read(void);

/**
 * @brief Process button state logic.
 * Call this function periodically from a task or timer.
 */
void button_poll(void);

/**
 * @brief Check if the button is currently held down.
 * @return 1 if stable state is pressed, 0 otherwise.
 */
uint32_t button_is_held(void);

/**
 * @brief Check if the button was newly pressed since the last call.
 * This clears the event flag after reading.
 * @return 1 if a press event occurred, 0 otherwise.
 */
uint32_t button_was_pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
