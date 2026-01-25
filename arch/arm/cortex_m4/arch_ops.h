#ifndef ARCH_OPS_H
#define ARCH_OPS_H

#include <stdint.h>
#include "device_registers.h"

/**
 * @brief Disable Global Interrupts.
 * 
 * Returns original state.
 */
static inline uint32_t arch_irq_lock(void) {
    uint32_t state;
    __asm__ volatile (
        "MRS %0, PRIMASK\n\t"
        "CPSID I\n\t"
        : "=r" (state) :: "memory"
    );
    return state;
}

/**
 * @brief Restore Global Interrupts.
 */
static inline void arch_irq_unlock(uint32_t state) {
    __asm__ volatile (
        "MSR PRIMASK, %0\n\t"
        : : "r" (state) : "memory"
    );
}

/** 
 * @brief Disable Interrupts up to a specific priority.
 * 
 * Allows high-priority interrupts to continue firing.
 * @param priority_threshold Priority threshold.
 * @return Original value.
 */
static inline uint32_t arch_irq_lock_soft(uint32_t priority_threshold) {
    uint32_t old;
    __asm volatile (
        "MRS %0, BASEPRI\n"
        "MSR BASEPRI, %1\n"
        "isb\n"  /* Ensures priority change takes effect immediately */
        : "=r"(old) : "r"(priority_threshold) : "memory"
    );
    return old;
}

/**
 * @brief Restore interrupt state.
 */
static inline void arch_irq_unlock_soft(uint32_t old) {
    __asm volatile (
        "MSR BASEPRI, %0\n"
        : : "r"(old) : "memory"
    );
}


/**
 * @brief Data Synchronization Barrier.
 * 
 * Completes explicit memory transfers.
 */
static inline void arch_dsb(void) {
    __asm volatile ("dsb 0xF" ::: "memory");
}

/**
 * @brief Instruction Synchronization Barrier.
 * 
 * Flushes pipeline.
 */
static inline void arch_isb(void) {
    __asm volatile ("isb 0xF" ::: "memory");
}

/**
 * @brief Data Memory Barrier.
 * 
 * Ensures ordering of memory accesses.
 */
static inline void arch_dmb(void) {
    __asm volatile ("dmb 0xF" ::: "memory");
}

/**
 * @brief Wait For Interrupt.
 * 
 * Puts CPU to sleep.
 */
static inline void arch_wfi(void) {
    __asm volatile ("wfi");
}

/**
 * @brief No Operation.
 */
static inline void arch_nop(void) {
    __asm volatile ("nop");
}

/**
 * @brief Set Process Stack Pointer.
 */
static inline void arch_set_psp(uint32_t psp) {
    __asm__ volatile ("MSR psp, %0" : : "r" (psp));
}

/**
 * @brief Disable interrupts.
 */
static inline void arch_irq_disable(void) {
    __asm volatile ("cpsid i" : : : "memory");
}

/**
 * @brief Trigger PendSV for Context Switch.
 */
static inline void arch_yield(void) {
    /* SCB->ICSR bit 28 (PENDSVSET) */
    #define SCB_ICSR_PENDSVSET_Msk  (1UL << 28)
    
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; 
    
    arch_dsb();
    arch_isb();
}

/**
 * @brief Relax the CPU (yield).
 */
static inline void arch_cpu_relax(void) {
    __asm volatile("yield" ::: "memory");
}

/**
 * @brief Full memory barrier.
 */
static inline void arch_memory_barrier(void) {
    arch_dmb();
}

/** 
 * @brief Atomic Test and Set. 
 * 
 * Sets the value at ptr to 1 and returns the previous value.
 * Used for spinlocks.
 * @return 0 if lock acquired (was 0), 1 if failed (was 1)
 */
static inline uint32_t arch_test_and_set(volatile uint32_t *ptr) {
    uint32_t old_val;
    uint32_t res;
    
    __asm volatile (
        "1: ldrex %0, [%2]\n"           /* Load exclusive */
        "   strex %1, %3, [%2]\n"       /* Try to store 1 */
        "   cmp %1, #0\n"               /* Check if store succeeded */
        "   bne 1b\n"                   /* If store failed, retry */
        "   dmb\n"                      /* Memory barrier */
        : "=&r" (old_val), "=&r" (res)  /* Output: old_val=%0, res=%1 (& = early clobber) */
        : "r" (ptr), "r" (1)            /* Input: ptr=%2, 1=%3 */
        : "cc", "memory"                /* Clobbers: flags changed, memory barrier */
    );
    return old_val;
}

/**
 * @brief Get the current CPU ID.
 * 
 * @return Always 0 for single-core Cortex-M4.
 */
static inline uint32_t arch_get_cpu_id(void) {
    return 0;
}

/**
 * @brief Reset the processor.
 */
void arch_reset(void);

/**
 * @brief Initialize the stack frame for a task.
 */
void *arch_initialize_stack(void *top_of_stack, 
                            void (*task_func)(void *), 
                            void *arg, 
                            void (*exit_handler)(void));

#endif // ARCH_OPS_H