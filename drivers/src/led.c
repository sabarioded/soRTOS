#include "led.h"
#include "led_hal.h"

/* Initialize the LED pin as output */
void led_init(void) {
    led_hal_init();
}

/* Turn the LED on */
void led_on(void) {
    led_hal_on();
}

/* Turn the LED off */
void led_off(void) {
    led_hal_off();
}

/* Toggle the LED state */
void led_toggle(void) {
    led_hal_toggle();
}
