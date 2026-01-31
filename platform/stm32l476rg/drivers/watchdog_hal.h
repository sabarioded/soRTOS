#ifndef WATCHDOG_HAL_STM32_H
#define WATCHDOG_HAL_STM32_H

#include "device_registers.h"
#include <stdint.h>

/* IWDG Key Values */
#define IWDG_KEY_RELOAD     0xAAAA
#define IWDG_KEY_ENABLE     0xCCCC
#define IWDG_KEY_ACCESS     0x5555

/* IWDG Status Register Bits */
#define IWDG_SR_PVU         (1U << 0) /* Prescaler Value Update */
#define IWDG_SR_RVU         (1U << 1) /* Reload Value Update */

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
    
    uint32_t prescaler_val = 4;
    uint8_t pr_reg = 0;
    uint32_t reload = 0;
    
    /* Find minimum prescaler that allows the timeout to fit in 12-bit RLR */
    /* We iterate through available prescalers: 4, 8, 16, 32, 64, 128, 256 */
    const uint16_t prescalers[] = {4, 8, 16, 32, 64, 128, 256};
    
    int found = 0;
    for (int i = 0; i < 7; i++) {
        prescaler_val = prescalers[i];
        pr_reg = (uint8_t)i;
        
        /* Calculate reload needed: RLR = (timeout_ms * LSI) / (1000 * PR) */
        /* To avoid float, RLR = (timeout_ms * 32) / PR */
        reload = (timeout_ms * 32) / prescaler_val;
        
        if (reload <= IWDG_RLR_MAX) {
            found = 1;
            break;
        }
    }
    
    if (!found) {
        return -1; /* Timeout too long */
    }
    
    if (reload == 0) reload = 1; /* Minimum safety */

    /* Enable IWDG (starts the counter) */
    IWDG->KR = IWDG_KEY_ENABLE;
    
    /* Enable register access */
    IWDG->KR = IWDG_KEY_ACCESS;
    
    /* Wait for register access if busy (PVU/RVU) */
    while (IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU));
    
    /* Set Prescaler */
    IWDG->PR = pr_reg;
    
    /* Set Reload */
    IWDG->RLR = reload;
    
    /* Wait for registers to update (optional but safe) */
    while (IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU));

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