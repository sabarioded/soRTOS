#ifndef DAC_HAL_NATIVE_H
#define DAC_HAL_NATIVE_H

#include "dac.h"
#include <stdint.h>

int dac_hal_init(dac_channel_t channel);
void dac_hal_write(dac_channel_t channel, uint16_t value);

#endif /* DAC_HAL_NATIVE_H */
