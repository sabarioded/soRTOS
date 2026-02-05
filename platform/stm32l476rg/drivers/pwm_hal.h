#ifndef PWM_HAL_STM32_H
#define PWM_HAL_STM32_H

#include "device_registers.h"
#include "gpio.h"
#include "gpio_hal.h"
#include "platform.h"

/* RCC */
#define RCC_APB1ENR1_TIM2EN     (1U << 0)

/* TIM2 is used for this implementation */
/* TIMx_CR1 */
#define TIM_CR1_CEN             (1U << 0)

/* TIMx_CCMR1 */
#define TIM_CCMR1_OC1M_Pos      4
#define TIM_CCMR1_OC1M_PWM1     (6U << TIM_CCMR1_OC1M_Pos)
#define TIM_CCMR1_OC1PE         (1U << 3)

/* TIMx_CCER */
#define TIM_CCER_CC1E           (1U << 0)

/**
 * @brief Initialize PWM on TIM2.
 * Assumes 80MHz PCLK1.
 * Configures for 0-100 resolution (ARR=99).
 */
static inline int pwm_hal_init(void *hal_handle, uint8_t channel, uint32_t freq_hz) {
    TIM_TypeDef *TIMx = (TIM_TypeDef *)hal_handle;

    if (TIMx == TIM2) {
        RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
        
        /* Configure GPIOs for TIM2 */
        /* PA0=CH1, PA1=CH2, PA2=CH3, PA3=CH4 (AF1) */
        if (channel == 1) { 
            gpio_init(GPIO_PORT_A, 0, GPIO_MODE_AF, GPIO_PULL_NONE, 1);
        }
        else if (channel == 2) {
            gpio_init(GPIO_PORT_A, 1, GPIO_MODE_AF, GPIO_PULL_NONE, 1);
        }
        else if (channel == 3) {
            gpio_init(GPIO_PORT_A, 2, GPIO_MODE_AF, GPIO_PULL_NONE, 1);
        }
        else if (channel == 4) {
            gpio_init(GPIO_PORT_A, 3, GPIO_MODE_AF, GPIO_PULL_NONE, 1);
        }
        else {
            return -1;
        }
    } else {
        return -1; /* Only TIM2 supported in this HAL for now */
    }

    /* Calculate Prescaler for desired frequency with 100 steps resolution */
    /* Freq = Clock / ((PSC+1) * (ARR+1)) */
    /* We fix ARR = 99 (so period is 100 ticks) */
    /* PSC = (Clock / (Freq * 100)) - 1 */
    
    uint32_t pclk = platform_get_cpu_freq();
    uint32_t arr = 99;
    uint32_t psc = (pclk / (freq_hz * 100)) - 1;

    TIMx->PSC = psc;
    TIMx->ARR = arr;

    /* Configure Channel for PWM Mode 1 */
    /* CCMR1 controls CH1 and CH2, CCMR2 controls CH3 and CH4 */
    if (channel == 1) {
        TIMx->CCMR1 &= ~(0xFFU); /* Clear CH1 bits */
        TIMx->CCMR1 |= (TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC1PE);
    } else if (channel == 2) {
        TIMx->CCMR1 &= ~(0xFF00U); /* Clear CH2 bits */
        TIMx->CCMR1 |= ((TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC1PE) << 8);
    }
    /* Add CH3/CH4 logic for CCMR2 if needed */

    /* Enable Auto-Reload Preload */
    TIMx->CR1 |= (1U << 7); /* ARPE */

    return 0;
}

static inline void pwm_hal_set_duty(void *hal_handle, uint8_t channel, uint8_t duty) {
    TIM_TypeDef *TIMx = (TIM_TypeDef *)hal_handle;
    if (duty > 100) duty = 100;

    /* CCR value corresponds directly to duty % because ARR=99 */
    if (channel == 1) TIMx->CCR1 = duty;
    else if (channel == 2) TIMx->CCR2 = duty;
    else if (channel == 3) TIMx->CCR3 = duty;
    else if (channel == 4) TIMx->CCR4 = duty;
}

static inline void pwm_hal_start(void *hal_handle, uint8_t channel) {
    TIM_TypeDef *TIMx = (TIM_TypeDef *)hal_handle;
    
    /* Enable Capture/Compare output */
    if (channel == 1) TIMx->CCER |= (1U << 0);
    else if (channel == 2) TIMx->CCER |= (1U << 4);
    else if (channel == 3) TIMx->CCER |= (1U << 8);
    else if (channel == 4) TIMx->CCER |= (1U << 12);

    /* Enable Counter */
    TIMx->CR1 |= TIM_CR1_CEN;
}

static inline void pwm_hal_stop(void *hal_handle, uint8_t channel) {
    TIM_TypeDef *TIMx = (TIM_TypeDef *)hal_handle;
    
    /* Disable Capture/Compare output */
    if (channel == 1) TIMx->CCER &= ~(1U << 0);
    else if (channel == 2) TIMx->CCER &= ~(1U << 4);
    /* ... */
}

#endif /* PWM_HAL_STM32_H */