#ifndef LED_H
#define LED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize led driver
 */
void led_init(void);

/**
 * @brief turn led on
 */
void led_on(void);

/**
 * @brief turn led off
 */
void led_off(void);

/**
 * @brief toggle the led
 */
void led_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */
