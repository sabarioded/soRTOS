#include "button.h"
#include <stdint.h>

#define PERIPH_BASE         0x40000000UL
#define AHB2_BASE           (PERIPH_BASE + 0x8000000UL)
#define GPIOC_BASE          (AHB2_BASE + 0x800UL)
#define GPIOC_MODER         (*(volatile uint32_t *)(GPIOC_BASE + 0x00))
#define GPIOC_IDR           (*(volatile uint32_t *)(GPIOC_BASE + 0x10))

#define RCC                 0x40021000UL
#define RCC_AHB2ENR         (*(volatile uint32_t *)(RCC + 0x4C))

#define GPIOC_CLOCK_EN_POS  2U
#define GPIOC_CLOCK_EN_MASK (1U << GPIOC_CLOCK_EN_POS)
#define BUTTON_PIN_POS      13U
#define BUTTON_PIN_MASK     (1U << BUTTON_PIN_POS)


void button_init(void) {
    /* GPIOC clock enable is bit 2 */
    RCC_AHB2ENR |= GPIOC_CLOCK_EN_MASK;

    /* for GPIOC set bits [27:26] to 00 for input */
    GPIOC_MODER &= ~(3U << BUTTON_PIN_POS * 2);
} 

uint32_t button_read(void) {
    /* Read input data register from pin 13 (user button) */
    uint32_t pin_state = GPIOC_IDR & BUTTON_PIN_MASK;

    /* Button is active low */
    return (pin_state == 0);
}
