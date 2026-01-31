#ifndef SYSTICK_HAL_H
#define SYSTICK_HAL_H

#include "device_registers.h"
#include "systick.h"

/***************** SYS_CSR ******************/
/* set to 1 to enable SysTick */
#define SYST_CSR_ENABLE_POS     0U
#define SYST_CSR_ENABLE_MASK    (1U << SYST_CSR_ENABLE_POS)

/* set to 1 -> couting down to 0 asserts SysTick exception request */
#define SYST_CSR_TICKINT_POS    1U
#define SYST_CSR_TICKINT_MASK   (1U << SYST_CSR_TICKINT_POS)

/* 0 for external source, 1 for processor clock */
#define SYST_CSR_CLKSOURCE_POS  2U
#define SYST_CSR_CLKSOURCE_MASK (1U << SYST_CSR_CLKSOURCE_POS)

/* Returns 1 if timer counted to 0 since last read */
#define SYST_CSR_COUNTFLAG_POS  16U
#define SYST_CSR_COUNTFLAG_MASK (1U << SYST_CSR_COUNTFLAG_POS)

/***************** SYST_RVR ******************/
/* [23:0] Value to load into the SYST_CVR register when the counter is enabled and when it reaches 0 */
#define SYST_RVR_RELOAD_POS     0U
#define SYST_RVR_RELOAD_MASK    (0xFFFFFFU << SYST_RVR_RELOAD_POS)

/***************** SYST_CVR ******************/
/* [23:0] Reads/return the current value of the SysTick counter */
#define SYST_CVR_RELOAD_POS     0U
#define SYST_CVR_RELOAD_MASK    (0xFFFFFFU << SYST_CVR_RELOAD_POS)

/**
 * @brief Initialize the SysTick hardware.
 * @param reload_val The value to load into the reload register.
 * @return 0 on success, -1 if reload value is invalid for this hardware.
 */
static inline int systick_hal_init(uint32_t reload_val) {
    if (reload_val > SYST_RVR_RELOAD_MASK) {
        return -1;
    }

    /* disable SysTick for configuration */
    SYSTICK->CSR = 0;

    /* set reload value */
    SYSTICK->RVR = (reload_val & SYST_RVR_RELOAD_MASK);

    /* reset current value */
    SYSTICK->CVR = 0;

    /* enable and configure SysTick */    
    SYSTICK->CSR |= SYST_CSR_ENABLE_MASK    /* enable SysTick */
                | SYST_CSR_TICKINT_MASK     /* enable SysTick interrupt */
                | SYST_CSR_CLKSOURCE_MASK;  /* use processor clock */
    return 0;
}

/**
 * @brief HAL Interrupt Handler helper.
 */
static inline void systick_hal_irq_handler(void) {
    systick_core_tick();
}

#endif /* SYSTICK_HAL_H */