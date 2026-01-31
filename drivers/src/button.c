#include "button.h"
#include "button_hal.h"
#include "systick.h"
#include "project_config.h"

static uint32_t g_btn_stable_state = 0;
static uint32_t g_btn_last_raw_state = 0;
static uint32_t g_btn_last_debounce_time = 0;
static uint32_t g_btn_pressed_event = 0;

/* Initialize the user button pin as input */
void button_init(void) {
    button_hal_init();
}

/* Read the button state (returns 1 if pressed, 0 if released) */
uint32_t button_read(void) {
    return button_hal_read();
}

/* Periodic poll function to handle debouncing and events */
void button_poll(void) {
    uint32_t raw = button_hal_read();
    uint32_t now = systick_get_ticks();

    /* If the raw state changed, reset the debounce timer */
    if (raw != g_btn_last_raw_state) {
        g_btn_last_debounce_time = now;
    }

    /* If the state has been stable longer than the delay, update the actual state */
    if ((now - g_btn_last_debounce_time) > BUTTON_DEBOUNCE_MS) {
        if (raw != g_btn_stable_state) {
            g_btn_stable_state = raw;
            if (g_btn_stable_state) {
                g_btn_pressed_event = 1;
            }
        }
    }
    g_btn_last_raw_state = raw;
}

uint32_t button_is_held(void) {
    return g_btn_stable_state;
}

uint32_t button_was_pressed(void) {
    uint32_t result = g_btn_pressed_event;
    g_btn_pressed_event = 0;
    return result;
}
