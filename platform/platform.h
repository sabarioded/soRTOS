#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include "queue.h"

/**
 * @brief Stack alignment requirement in bytes.
 * * Defines the byte alignment for task stacks.
 */
#define PLATFORM_STACK_ALIGNMENT 8

/**
 * @brief Initialize the platform core hardware.
 * * This must be the first call in main().
 */
void platform_init(void);

/**
 * @brief Enter a critical error state (Panic).
 * * Call this when an unrecoverable error occurs (e.g., Stack Overflow, 
 * HardFault).
 * Implementation: Should disable interrupts and loop forever (blinking an LED).
 */
void platform_panic(void);

/**
 * @brief Get the current CPU Core frequency in Hz.
 * * Generic drivers (like UART or Timers) use this to calculate 
 * * prescalers and baud rates without hardcoding values.
 * @return Frequency in Hz (e.g., 80000000 for 80MHz)
 */
size_t platform_get_cpu_freq(void);

/**
 * @brief Initializes the system tick timer.
 * * This is called by the scheduler before starting.
 * @param tick_hz The desired tick frequency in Hz.
 */
void platform_systick_init(size_t tick_hz);

/**
 * @brief Retrieves the current system tick count.
 * @return The number of ticks since the scheduler started.
 */
size_t platform_get_ticks(void);

/**
 * @brief Put the CPU into a low-power idle state.
 * * This function is called by the idle task to save power.
 * * It should wait for the next interrupt to wake the CPU.
 */
void platform_cpu_idle(void);

/**
 * @brief Starts the scheduler.
 * * This function starts the RTOS scheduler and begins execution of the first
 * * task. It should never return.
 * @param stack_pointer The initial stack pointer for the first task.
 */
void platform_start_scheduler(size_t stack_pointer);

/**
 * @brief Initialize a task's stack frame.
 * @param top_of_stack Pointer to the top of the allocated stack.
 * @param task_func The function pointer for the task's entry point.
 * @param arg The argument to be passed to the task.
 * @param exit_handler Function to call if the task function returns.
 * @return The new stack pointer (top of stack) after initialization.
 */
void *platform_initialize_stack(void *top_of_stack, void (*task_func)(void *), void *arg, void (*exit_handler)(void));

/**
 * @brief Trigger a context switch.
 */
void platform_yield(void);

/**
 * @brief Initialize the platform UART for CLI/Logging.
 */
void platform_uart_init(void);

/**
 * @brief Platform-specific UART receive char (non-blocking).
 */
int platform_uart_getc(char *out_ch);

/**
 * @brief Platform-specific UART send string (non-blocking).
 */
int platform_uart_puts(const char *s);

/**
 * @brief Resets the system.
 * * This function triggers a software reset of the microcontroller.
 */
void platform_reset(void);

/**
 * @brief Register a task to be notified when UART data arrives.
 * @param task_id The ID of the task to notify.
 */
void platform_uart_set_rx_notify(uint16_t task_id);

/**
 * @brief Register a queue to receive incoming UART bytes.
 * @param q Pointer to the queue.
 */
void platform_uart_set_rx_queue(queue_t *q);

#endif // PLATFORM_H