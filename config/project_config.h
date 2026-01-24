#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include <stdint.h>

/* ============================================================================
   CLI Configuration
   ============================================================================ */
#define CLI_MAX_LINE_LEN       128    /* Maximum command line length */
#define CLI_MAX_ARGS           16     /* Maximum number of command arguments */
#define CLI_MAX_CMDS           32     /* Maximum number of registered commands */

/* ============================================================================
   UART Configuration
   ============================================================================ */
#define UART_BAUD_DEFAULT      115200 /* Default UART baud rate */
#define UART_RX_BUFFER_SIZE    256    /* UART receive buffer size */
#define UART_TX_BUFFER_SIZE    512    /* UART transmit buffer size */

/* ============================================================================
   Scheduler Configuration
   ============================================================================ */
#define MAX_TASKS              58     /* Maximum number of tasks */
#define SYSTICK_FREQ_HZ        1000   /* SysTick interrupt frequency (1 kHz = 1ms tick) */

/* Stack overflow detection */
#define STACK_CANARY           0xDEADBEEF  /* Magic value at stack bottom */

/* Garbage collection */
#define GARBAGE_COLLECTION_TICKS 1000U  /* Run GC every 1000 ticks (1 second at 1kHz) */

/* ============================================================================
   Stack Size Configuration
   ============================================================================ */

/* Dynamic allocation limits */
#define STACK_MIN_SIZE_BYTES     512    /* Minimum stack size (128 words) */
#define STACK_MAX_SIZE_BYTES     8192   /* Maximum stack size (2048 words) */

/* Common stack sizes (for convenience) */
#define STACK_SIZE_512B         512    /* For simple tasks */
#define STACK_SIZE_1KB          1024   /* For normal tasks (default) */
#define STACK_SIZE_2KB          2048   /* For tasks with deep call stacks */
#define STACK_SIZE_4KB          4096   /* For tasks with large local variables */

/* ============================================================================
   Allocator Configuration (TLSF)
   ============================================================================ */
/* 
 * FL_INDEX_MAX defines the maximum power of 2 size we support.
 * 30 covers up to 1GB blocks. For 128KB RAM, use 17.
 */
#define FL_INDEX_MAX            30

/* SL_INDEX_COUNT_LOG2 defines the number of linear subdivisions (2^n). */
#define SL_INDEX_COUNT_LOG2     5

/* ============================================================================
   Logger Configuration
   ============================================================================ */
#define LOG_ENABLE              1      /* 1 to enable, 0 to remove code */
#define LOG_QUEUE_SIZE          64     /* Number of log entries to buffer */
#define LOG_HISTORY_SIZE        128    /* Number of entries to keep in RAM history */

/* ============================================================================
   Compile-Time Validation
   ============================================================================ */

/* Verify MAX_TASKS is reasonable */
#if MAX_TASKS < 2
    #error "MAX_TASKS must be at least 2 (for idle + 1 user task)"
#endif

#endif /* PROJECT_CONFIG_H */