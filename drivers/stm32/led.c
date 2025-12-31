#include "led.h"
#include "gpio.h"

#define LED_PORT    GPIO_PORT_A
#define LED_PIN     5

/* Initialize the LED pin as output */
void led_init(void) {
    gpio_init(LED_PORT, LED_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
}

/* Turn the LED on */
void led_on(void) {
    /* set output to 1 */
    gpio_write(LED_PORT, LED_PIN, 1);
}

/* Turn the LED off */
void led_off(void) {
    /* set output to 0 */
    gpio_write(LED_PORT, LED_PIN, 0);
}

/* Toggle the LED state */
void led_toggle(void) {
    gpio_toggle(LED_PORT, LED_PIN);
}
