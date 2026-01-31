#include "dac.h"
#include "dac_hal.h"

int dac_init(dac_channel_t channel) {
    return dac_hal_init(channel);
}

void dac_write(dac_channel_t channel, uint16_t value) {
    dac_hal_write(channel, value);
}