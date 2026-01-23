#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

/* Native/Host Platform Configuration */

/* Simulated Clock Speed */
#define SYSCLK_HZ          1000000UL

/* Interrupt Priorities (Not applicable on Host, defined for compatibility) */
#define MAX_SYSCALL_PRIORITY   0
#define SYSTICK_PRIORITY       0
#define PENDSV_PRIORITY        0

#endif /* PLATFORM_CONFIG_H */