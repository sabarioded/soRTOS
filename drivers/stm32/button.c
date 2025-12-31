#include "button.h"
#include "gpio.h"

#define BUTTON_PORT GPIO_PORT_C
#define BUTTON_PIN  13

/* Initialize the user button pin as input */
void button_init(void) {
    gpio_init(BUTTON_PORT, BUTTON_PIN, GPIO_MODE_INPUT, GPIO_PULL_NONE, 0);
} 

/* Read the button state (returns 1 if pressed, 0 if released) */
uint32_t button_read(void) {
    /* Button is active low */
    return !gpio_read(BUTTON_PORT, BUTTON_PIN);
}
