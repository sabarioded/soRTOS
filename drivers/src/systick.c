#include "systick.h"
#include "systick_hal.h"
#include "scheduler.h"
#include "arch_ops.h"
#include "platform.h"
#include <stddef.h>

/* Global tick counter */
static volatile uint32_t g_systick_ticks = 0;

/* Initialize the system tick driver */
int systick_init(uint32_t ticks_hz) {
    uint32_t sysclk_hz = platform_get_cpu_freq();
    if (sysclk_hz == 0 || ticks_hz == 0) {
        return -1;
    }

    /* Calculate reload value. 
     * SysTick counts from reload to 0 (so we need -1)
     */
    uint32_t reload = (sysclk_hz / ticks_hz) - 1;

    return systick_hal_init(reload);
}

/* Get the current tick count */
uint32_t systick_get_ticks(void) {
    return g_systick_ticks;
}

/* Busy wait for a specific number of ticks */
void systick_delay_ticks(uint32_t ticks) {
    uint32_t start = g_systick_ticks;
    while ((g_systick_ticks - start) < ticks) {
        arch_nop();
    }
}

/* Core interrupt handler for system tick */
void systick_core_tick(void) {
    g_systick_ticks++;
    if (scheduler_tick()) {
        arch_yield();
    }
}
