#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

/* ============================================================================
   System Clock Configuration
   ============================================================================ */
#define SYSCLK_HZ          80000000UL  /* 80 MHz system clock */

/* ============================================================================
   Interrupt Priorities (ARM Cortex-M)
   ============================================================================
   Lower numbers = higher priority
   STM32L4 has 4 bits = 16 priority levels (0-15)
   
   Priority 0:  Highest (critical hard faults)
   Priority 5:  High-priority peripherals (timers, critical I/O)
   Priority 10: Normal peripherals (UART, SPI, I2C)
   Priority 14: SysTick (lower than most peripherals)
   Priority 15: PendSV (lowest - for context switching)
============================================================================ */
#define MAX_SYSCALL_PRIORITY   5   /* Highest priority that can call RTOS functions */
#define SYSTICK_PRIORITY       14  /* SysTick priority (lower than peripherals) */
#define PENDSV_PRIORITY        15  /* PendSV priority (lowest for context switch) */

#endif /* PLATFORM_CONFIG_H */