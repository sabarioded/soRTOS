#ifndef ARCH_OPS_H
#define ARCH_OPS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Disable Global Interrupts.
 * 
 * On the native host platform (single-threaded tests), this is a no-op
 * or simply returns a dummy status.
 * 
 * @return Current interrupt state (always 0 on host).
 */
static inline uint32_t arch_irq_lock(void) { return 0; }

/**
 * @brief Restore Global Interrupts.
 * 
 * @param s The interrupt state to restore.
 */
static inline void arch_irq_unlock(uint32_t s) { (void)s; }

/**
 * @brief No Operation.
 */
static inline void arch_nop(void) { }

/**
 * @brief Data Memory Barrier.
 */
static inline void arch_dmb(void) { }

/**
 * @brief Trigger a context switch (Yield).
 * 
 * On host, context switching is handled via setjmp/longjmp in the test harness,
 * not by this instruction.
 */
static inline void arch_yield(void) { }

/**
 * @brief Relax the CPU.
 * 
 * Used inside spinloops to reduce power consumption or hint the pipeline.
 */
static inline void arch_cpu_relax(void) { }

/**
 * @brief Full memory barrier.
 * 
 * Ensures compiler does not reorder memory accesses across this point.
 */
static inline void arch_memory_barrier(void) { 
    __asm__ volatile("" ::: "memory"); 
}

/** 
 * @brief Atomic Test and Set. 
 * 
 * Sets the value at ptr to 1 and returns the previous value.
 * Mock implementation for single-threaded host environment.
 * 
 * @param ptr Pointer to the lock variable.
 * @return 0 if lock acquired (was 0), 1 if failed (was 1).
 */
static inline uint32_t arch_test_and_set(volatile uint32_t *ptr) {
    uint32_t old = *ptr;
    *ptr = 1;
    return old;
}

/**
 * @brief Get the current CPU ID.
 * 
 * @return Always 0 for single-threaded host simulation.
 */
static inline uint32_t arch_get_cpu_id(void) {
    return 0;
}

/**
 * @brief Reset the processor.
 * 
 * On host, this exits the process.
 */
void arch_reset(void);

/**
 * @brief Initialize the stack frame for a task.
 * 
 * @param top_of_stack Pointer to the top of the allocated stack.
 * @param task_func Pointer to the task entry function.
 * @param arg Argument to pass to the task function.
 * @param exit_handler Function to call if the task function returns.
 * @return The new top of stack pointer.
 */
void *arch_initialize_stack(void *top_of_stack, 
                            void (*task_func)(void *), 
                            void *arg, 
                            void (*exit_handler)(void));

#ifdef __cplusplus
}
#endif

#endif /* ARCH_OPS_H */