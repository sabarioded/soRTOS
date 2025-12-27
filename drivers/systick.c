#include "systick.h"
#include "utils.h"

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

/***************** SYST_CALIB ******************/
/* [23:0] Reload value for 10ms timing */
#define SYST_CALIB_TENMS_POS    0U
#define SYST_CALIB_TENMS_MASK   (0xFFFFFFUL << SYST_CALIB_TENMS_POS)

/* 1 = TENMS value is not exactly 10ms */
#define SYST_CALIB_SKEW_POS     30U
#define SYST_CALIB_SKEW_MASK    (1UL << SYST_CALIB_SKEW_POS)

/* 1 = No external reference clock is provided */
#define SYST_CALIB_NOREF_POS    31U
#define SYST_CALIB_NOREF_MASK   (1UL << SYST_CALIB_NOREF_POS)

extern void scheduler_wake_sleeping_tasks(void);

/* Simple global tick counter incremented on each SysTick interrupt */
volatile uint32_t systick_ticks = 0;

/* SysTick interrupt handler */
void SysTick_Handler(void)
{
    systick_ticks++;

    /* Wake any tasks that have finished sleeping */
    scheduler_wake_sleeping_tasks();

    yield_cpu(); /* trigger context switch */
}


/* initialize the SysTick driver with desired tick frequency (Hz) */
int systick_init(uint32_t ticks_hz) {
    /* get system clock */
    uint32_t sysclk_hz = get_system_clock_hz();
    if(sysclk_hz == 0 || ticks_hz == 0) {
        return -1; /* error */
    }

    /* calculate the reload value. 
    * SysTick counts from reload to 0 (so we need -1)
    * for example, 1000hz(1ms) ticks with 16MHz sysclk:
    * reload = (16,000,000 / 1000) - 1 = 15,999
    */
    uint32_t reload = (sysclk_hz / ticks_hz) - 1;
    if (reload > SYST_RVR_RELOAD_MASK) {
        return -1; /* error: reload value too large */
    }

    /* disable SysTick for configuration */
    SYSTICK->CSR = 0;

    /* set reload value */
    SYSTICK->RVR = (reload & SYST_RVR_RELOAD_MASK);

    /* reset current value */
    SYSTICK->CVR = 0;

    /* enable and configure SysTick */    
    SYSTICK->CSR |= SYST_CSR_ENABLE_MASK    /* enable SysTick */
                | SYST_CSR_TICKINT_MASK     /* enable SysTick interrupt */
                | SYST_CSR_CLKSOURCE_MASK;  /* use processor clock */
    return 0; /* success */
}


/* Return number of SysTick interrupts since init */
uint32_t systick_get_ticks(void)
{
    return systick_ticks;
}


/* Busy-wait for 'ticks' SysTick interrupts */
void systick_delay_ticks(uint32_t ticks)
{
    uint32_t start = systick_ticks;
    while ((systick_ticks - start) < ticks) {
        /* busy wait */
    }
}

