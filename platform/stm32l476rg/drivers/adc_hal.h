#ifndef ADC_HAL_STM32_H
#define ADC_HAL_STM32_H

#include "device_registers.h"
#include "arch_ops.h"

/* RCC Definitions */
#define RCC_AHB2ENR_ADCEN       (1U << 13)

/* ADC Register Bits */
/* CR */
#define ADC_CR_ADEN             (1U << 0)
#define ADC_CR_ADDIS            (1U << 1)
#define ADC_CR_ADSTART          (1U << 2)
#define ADC_CR_ADVREGEN         (1U << 28)
#define ADC_CR_DEEPPWD          (1U << 29)
#define ADC_CR_ADCAL            (1U << 31)

/* ISR */
#define ADC_ISR_ADRDY           (1U << 0)
#define ADC_ISR_EOC             (1U << 2)

/* SQR1 */
#define ADC_SQR1_L_Pos          (0U)
#define ADC_SQR1_SQ1_Pos        (6U)

/* ADC Common Register Bits (CCR) */
#define ADC_CCR_CKMODE_Pos      (16U)
#define ADC_CCR_CKMODE_HCLK_1   (1U << ADC_CCR_CKMODE_Pos)

/* Address of ADC Common block (relative to ADC1) */
/* ADC1 Base: 0x50040000, ADC Common: 0x50040300 */
#define ADC_COMMON_OFFSET       (0x300U)

typedef struct {
    uint32_t Resolution; /* Not used in this simple driver, defaults to 12-bit */
} ADC_Config_t;

/**
 * @brief Initialize the ADC hardware.
 * Performs the startup sequence: Deep Power Down exit -> Regulator Enable -> Calibration -> Enable.
 */
static inline int adc_hal_init(void *hal_handle, void *config_ptr) {
    ADC_TypeDef *ADCx = (ADC_TypeDef *)hal_handle;
    (void)config_ptr; /* Unused for now */

    if (!ADCx) return -1;

    /* 1. Enable ADC Clock */
    RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;

    /* 2. Configure ADC Clock Source (Synchronous HCLK/1) */
    /* Access ADC Common Register */
    volatile uint32_t *adc_ccr = (volatile uint32_t *)((uint32_t)ADCx + ADC_COMMON_OFFSET);
    *adc_ccr |= ADC_CCR_CKMODE_HCLK_1;

    /* 3. Exit Deep Power Down Mode */
    ADCx->CR &= ~ADC_CR_DEEPPWD;

    /* 4. Enable Internal Voltage Regulator */
    ADCx->CR |= ADC_CR_ADVREGEN;

    /* 5. Wait for Regulator Startup Time (T_ADCVREG_STUP = 20us) */
    /* Simple busy wait loop - assuming 80MHz, 20us is ~1600 cycles */
    for (volatile int i = 0; i < 2000; i++) {
        arch_nop();
    }

    /* 6. Calibration */
    /* Ensure ADC is disabled */
    if (ADCx->CR & ADC_CR_ADEN) {
        ADCx->CR |= ADC_CR_ADDIS;
        while (ADCx->CR & ADC_CR_ADEN);
    }
    
    /* Start Calibration */
    ADCx->CR |= ADC_CR_ADCAL;

    /* Wait for Calibration to complete */
    while (ADCx->CR & ADC_CR_ADCAL);

    /* 7. Enable ADC */
    ADCx->CR |= ADC_CR_ADEN;

    /* Wait for ADC Ready */
    while (!(ADCx->ISR & ADC_ISR_ADRDY));

    return 0;
}

/**
 * @brief Perform a single blocking conversion.
 */
static inline int adc_hal_read(void *hal_handle, uint32_t channel, uint16_t *value) {
    ADC_TypeDef *ADCx = (ADC_TypeDef *)hal_handle;

    if (!ADCx) return -1;

    /* 1. Configure Sequence */
    /* Set Sequence Length to 1 (L=0) */
    /* Set SQ1 to 'channel' */
    /* Note: This overwrites previous sequence */
    ADCx->SQR1 = (channel << ADC_SQR1_SQ1_Pos);

    /* 2. Start Conversion */
    ADCx->CR |= ADC_CR_ADSTART;

    /* 3. Wait for End of Conversion */
    /* Add a simple timeout mechanism to prevent infinite hang */
    uint32_t timeout = 100000;
    while (!(ADCx->ISR & ADC_ISR_EOC)) {
        if (--timeout == 0) return -1;
    }

    /* 4. Read Result */
    *value = (uint16_t)ADCx->DR;

    /* Clear EOC flag (optional, reading DR usually clears it) */
    ADCx->ISR |= ADC_ISR_EOC;

    return 0;
}

#endif /* ADC_HAL_STM32_H */