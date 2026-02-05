#ifndef DAC_HAL_STM32_H
#define DAC_HAL_STM32_H

#include "device_registers.h"
#include "gpio.h"
#include "gpio_hal.h"
#include "dac.h"
#include <stdint.h>

enum {
    DAC_CHANNEL_1 = 1,
    DAC_CHANNEL_2 = 2
};

/* RCC Definitions */
#define RCC_APB1ENR1_DAC1EN     (1U << 29)

/* DAC Register Bits */
#define DAC_CR_EN1              (1U << 0)
#define DAC_CR_EN2              (1U << 16)

/**
 * @brief Initialize the DAC hardware.
 */
static inline int dac_hal_init(dac_channel_t channel) {
    /* Enable DAC1 Clock */
    RCC->APB1ENR1 |= RCC_APB1ENR1_DAC1EN;

    if (channel == DAC_CHANNEL_1) {
        /* PA4 is DAC1_OUT1. Configure as Analog mode (default) */
        gpio_init(GPIO_PORT_A, 4, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
        
        /* Enable Channel 1 */
        DAC->CR |= DAC_CR_EN1;
    } else if (channel == DAC_CHANNEL_2) {
        /* PA5 is DAC1_OUT2. Configure as Analog mode */
        /* Note: PA5 is also the User LED. This might conflict if LED is used. */
        gpio_init(GPIO_PORT_A, 5, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
        
        /* Enable Channel 2 */
        DAC->CR |= DAC_CR_EN2;
    } else {
        return -1;
    }
    return 0;
}

/**
 * @brief Write 12-bit value to DAC.
 */
static inline void dac_hal_write(dac_channel_t channel, uint16_t value) {
    /* Clamp to 12-bit */
    if (value > 4095) value = 4095;

    if (channel == DAC_CHANNEL_1) {
        /* Write to DHR12R1 (12-bit right-aligned data) */
        DAC->DHR12R1 = value;
        /* Software trigger not needed if TEN=0 (default), output updates automatically */
    } else if (channel == DAC_CHANNEL_2) {
        DAC->DHR12R2 = value;
    }
}

#endif /* DAC_HAL_STM32_H */
