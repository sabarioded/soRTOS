#ifndef ADC_HAL_NATIVE_H
#define ADC_HAL_NATIVE_H

#include <stdint.h>

typedef struct {
    uint32_t Resolution;
} ADC_Config_t;

int adc_hal_init(void *hal_handle, void *config_ptr);
int adc_hal_read(void *hal_handle, uint32_t channel, uint16_t *value);

#endif /* ADC_HAL_NATIVE_H */
