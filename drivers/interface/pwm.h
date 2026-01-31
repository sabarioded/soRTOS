#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque Handle for a PWM port.
 */
typedef struct pwm_context* pwm_port_t;

/**
 * @brief Create and initialize a PWM channel.
 * 
 * @param hal_handle Pointer to the hardware timer (e.g., TIM2).
 * @param channel Channel number (1-4).
 * @param freq_hz Desired frequency in Hz.
 * @param duty_percent Initial duty cycle (0-100).
 * @return pwm_port_t Handle to the PWM port, or NULL on failure.
 */
pwm_port_t pwm_create(void *hal_handle, uint8_t channel, uint32_t freq_hz, uint8_t duty_percent);

/**
 * @brief Destroy the PWM port.
 */
void pwm_destroy(pwm_port_t port);

/**
 * @brief Set the duty cycle.
 * @param port Handle to the PWM port.
 * @param duty_percent Duty cycle (0-100).
 */
void pwm_set_duty(pwm_port_t port, uint8_t duty_percent);

/**
 * @brief Start PWM output.
 */
void pwm_start(pwm_port_t port);

/**
 * @brief Stop PWM output.
 */
void pwm_stop(pwm_port_t port);

/**
 * @brief Get context size.
 */
size_t pwm_get_context_size(void);

#ifdef __cplusplus
}
#endif

#endif /* PWM_H */