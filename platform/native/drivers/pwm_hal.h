#ifndef PWM_HAL_NATIVE_H
#define PWM_HAL_NATIVE_H

#include <stdint.h>

int pwm_hal_init(void *hal_handle, uint8_t channel, uint32_t freq_hz);
void pwm_hal_set_duty(void *hal_handle, uint8_t channel, uint8_t duty);
void pwm_hal_start(void *hal_handle, uint8_t channel);
void pwm_hal_stop(void *hal_handle, uint8_t channel);

#endif /* PWM_HAL_NATIVE_H */
