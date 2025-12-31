#ifndef ARCH_OPS_H
#define ARCH_OPS_H

#include <stdint.h>
#include "device_registers.h"

/** @brief Disable Global Interrupts (PRIMASK). Returns original state. */
static inline uint32_t arch_irq_lock(void) {
    uint32_t state;
    __asm__ volatile (
        "MRS %0, PRIMASK\n\t"
        "CPSID I\n\t"
        : "=r" (state) :: "memory"
    );
    return state;
}

/** @brief Restore Global Interrupts (PRIMASK). */
static inline void arch_irq_unlock(uint32_t state) {
    __asm__ volatile (
        "MSR PRIMASK, %0\n\t"
        : : "r" (state) : "memory"
    );
}

/** 
 * @brief Disable Interrupts up to a specific priority (BASEPRI). 
 * Allows high-priority interrupts (value < new_basepri) to continue firing.
 * @param new_basepri Priority threshold.
 * @return Original BASEPRI value.
 */
static inline uint32_t arch_irq_lock_soft(uint32_t new_basepri) {
    uint32_t old;
    __asm volatile (
        "MRS %0, BASEPRI\n"
        "MSR BASEPRI, %1\n"
        : "=r"(old) : "r"(new_basepri) : "memory"
    );
    return old;
}

/** @brief Restore BASEPRI. */
static inline void arch_irq_unlock_soft(uint32_t old) {
    __asm volatile (
        "MSR BASEPRI, %0\n"
        : : "r"(old) : "memory"
    );
}


/** @brief Data Synchronization Barrier. Completes explicit memory transfers. */
static inline void arch_dsb(void) { __asm volatile ("dsb 0xF" ::: "memory"); }

/** @brief Instruction Synchronization Barrier. Flushes pipeline. */
static inline void arch_isb(void) { __asm volatile ("isb 0xF" ::: "memory"); }

/** @brief Data Memory Barrier. Ensures ordering of memory accesses. */
static inline void arch_dmb(void) { __asm volatile ("dmb 0xF" ::: "memory"); }

/** @brief Wait For Interrupt. Puts CPU to sleep. */
static inline void arch_wfi(void) { __asm volatile ("wfi"); }

/** @brief No Operation. */
static inline void arch_nop(void) { __asm volatile ("nop"); }

/** @brief Set Process Stack Pointer. */
static inline void arch_set_psp(uint32_t psp) {
    __asm__ volatile ("MSR psp, %0" : : "r" (psp));
}

/** @brief disable interrupts */
static inline void arch_irq_disable(void) {
    __asm volatile ("cpsid i" : : : "memory");
}

/** @brief Trigger PendSV for Context Switch. */
static inline void arch_yield(void) {
    // SCB->ICSR bit 28 (PENDSVSET)
    #define SCB_ICSR_PENDSVSET_Msk  (1UL << 28)
    
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; 
    
    arch_dsb();
    arch_isb();
}

/** @brief Reset the processor. */
void arch_reset(void);

/** @brief Initialize the stack frame for a task. */
void *arch_initialize_stack(void *top_of_stack, 
                            void (*task_func)(void *), 
                            void *arg, 
                            void (*exit_handler)(void));

#endif // ARCH_OPS_H