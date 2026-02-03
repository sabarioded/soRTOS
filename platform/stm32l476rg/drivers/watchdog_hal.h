#ifndef WATCHDOG_HAL_STM32_H
#define WATCHDOG_HAL_STM32_H

#include "device_registers.h"
#include <stdint.h>
#include "arch_ops.h"

/* IWDG Key Values */
#define IWDG_KEY_RELOAD     0xAAAA /* value to write to reload/refresh the counter */
#define IWDG_KEY_ENABLE     0xCCCC /* value to write to start the watchdog */
#define IWDG_KEY_ACCESS     0x5555 /* value to write to unlock access to prescaler/registers */

/* IWDG Status Register Bits */
#define IWDG_SR_PVU         (1U << 0) /* prescaler value update status */
#define IWDG_SR_RVU         (1U << 1) /* reload value update status */

/* LSI Frequency (approximate, usually 32kHz on STM32L4) */
#define LSI_FREQ_HZ         32000U

/* Max RLR value is 12-bit (0xFFF = 4095) */
#define IWDG_RLR_MAX        0xFFFU

/**
 * @brief Initialize the IWDG.
 * 
 * Calculates the best prescaler and reload value for the given timeout.
 * Note: Once enabled, the IWDG cannot be disabled until reset.
 */
static inline int watchdog_hal_init(uint32_t timeout_ms) {
    /* 
     * Timeout = (RLR * Prescaler) / LSI_FREQ
     * We need to find Prescaler (PR) and Reload (RLR).
     * PR can be 4, 8, 16, 32, 64, 128, 256.
     * PR register values: 0=4, 1=8, ... 6=256.
     */
    
    uint32_t prescaler_val;
    uint8_t prescaler_reg = 0;
    uint32_t reload = 0;
    
    /* Find minimum prescaler that allows the timeout to fit in 12-bit RLR */
    const uint16_t prescalers[] = {4, 8, 16, 32, 64, 128, 256};
    
    int found = 0;
    for (int i = 0; i < 7; i++) {
        prescaler_val = prescalers[i];
        prescaler_reg = (uint8_t)i;
        
        /* Calculate reload needed: RLR = (timeout_ms * LSI) / (1000 * PR) */
        reload = (timeout_ms * 32) / prescaler_val;
        
        if (reload <= IWDG_RLR_MAX) {
            found = 1;
            break;
        }
    }
    
    if (!found || reload == 0) {
        return -1;
    }

    /* Enable IWDG (starts the counter) */
    IWDG->KR = IWDG_KEY_ENABLE;
    
    /* Enable register access */
    IWDG->KR = IWDG_KEY_ACCESS;
    
    /* Wait for register access if busy (PVU/RVU) with timeout */
    uint32_t wait = 100000U;
    while (IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) {
        if (wait-- == 0U) {
            return -1;
        }
        arch_nop();
    }
    
    /* Set Prescaler */
    IWDG->PR = prescaler_reg;
    
    /* Set Reload */
    IWDG->RLR = reload;
    
    /* Wait for registers to update with timeout */
    wait = 100000U;
    while (IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) {
        if (wait-- == 0U) {
            return -1;
        }
        arch_nop();
    }

    /* Reload the counter with the new value */
    IWDG->KR = IWDG_KEY_RELOAD;
    
    return 0;
}

/**
 * @brief Kick the watchdog.
 */
static inline void watchdog_hal_kick(void) {
    IWDG->KR = IWDG_KEY_RELOAD;
}

#endif /* WATCHDOG_HAL_STM32_H */
