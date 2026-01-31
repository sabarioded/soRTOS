#include "pwm.h"
#include "pwm_hal.h"
#include "allocator.h"
#include "utils.h"

struct pwm_context {
    void *hal_handle;
    uint8_t channel;
};

size_t pwm_get_context_size(void) {
    return sizeof(struct pwm_context);
}

pwm_port_t pwm_create(void *hal_handle, uint8_t channel, uint32_t freq_hz, uint8_t duty_percent) {
    if (!hal_handle) return NULL;

    pwm_port_t port = (pwm_port_t)allocator_malloc(sizeof(struct pwm_context));
    if (!port) return NULL;

    port->hal_handle = hal_handle;
    port->channel = channel;

    if (pwm_hal_init(hal_handle, channel, freq_hz) != 0) {
        allocator_free(port);
        return NULL;
    }

    pwm_hal_set_duty(hal_handle, channel, duty_percent);
    return port;
}

void pwm_destroy(pwm_port_t port) {
    if (port) {
        pwm_hal_stop(port->hal_handle, port->channel);
        allocator_free(port);
    }
}

void pwm_set_duty(pwm_port_t port, uint8_t duty_percent) {
    if (port) {
        pwm_hal_set_duty(port->hal_handle, port->channel, duty_percent);
    }
}

void pwm_start(pwm_port_t port) {
    if (port) {
        pwm_hal_start(port->hal_handle, port->channel);
    }
}

void pwm_stop(pwm_port_t port) {
    if (port) {
        pwm_hal_stop(port->hal_handle, port->channel);
    }
}