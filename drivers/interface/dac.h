#ifndef DAC_H
#define DAC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DAC_CHANNEL_1 = 1,
    DAC_CHANNEL_2 = 2
} dac_channel_t;

/**
 * @brief Initialize the DAC channel.
 * @param channel DAC_CHANNEL_1 or DAC_CHANNEL_2.
 * @return 0 on success, -1 on error.
 */
int dac_init(dac_channel_t channel);

/**
 * @brief Write a value to the DAC output.
 * @param channel DAC_CHANNEL_1 or DAC_CHANNEL_2.
 * @param value 12-bit value (0-4095).
 */
void dac_write(dac_channel_t channel, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* DAC_H */