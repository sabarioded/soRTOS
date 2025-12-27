#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include "device_registers.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NULL ((void *)0)
#define __WFI()         __asm volatile ("wfi")
#define KERNEL_NOP()    __asm volatile ("nop")

#define MAX_SYSCALL_PRIORITY     0x80

/**
 * @brief   Data Synchronization Barrier (DSB).
 * @details Ensures that all explicit memory accesses occurring before the DSB 
 * instruction complete before any instruction after the DSB executes.
 */
static inline void __DSB(void) {
    __asm volatile ("dsb 0xF" ::: "memory");
}


/**
 * @brief   Instruction Synchronization Barrier (ISB).
 * @details Flushes the processor pipeline and instruction cache. ensures that 
 * context-changing operations (like priority changes) take effect 
 * before subsequent instructions are fetched.
 */
static inline void __ISB(void) {
    __asm volatile ("isb 0xF" ::: "memory");
}


/**
 * @brief   Data Memory Barrier (DMB).
 * @details Ensures that all memory accesses requested prior to the DMB are 
 * complete and visible to all other bus masters before any subsequent memory 
 * accesses are started.
 */
static inline void __DMB(void) {
    __asm volatile ("dmb 0xF" ::: "memory");
}


/**
 * @brief   Waits for specific bits in a register to be SET.
 * @param   reg      Pointer to the volatile register to monitor.
 * @param   mask     Bitmask of the flags to check.
 * @param   max_iter Timeout counter (loop iterations, not time units).
 * @return  0 on success, -1 if timeout occurred.
 */
int wait_for_flag_set(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter);

/**
 * @brief   Waits for specific bits in a register to be CLEARED.
 * @param   reg      Pointer to the volatile register to monitor.
 * @param   mask     Bitmask of the flags to check.
 * @param   max_iter Timeout counter (loop iterations).
 * @return  0 on success, -1 if timeout occurred.
 */
int wait_for_flag_clear(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter);

/**
 * @brief   Waits for masked bits in a register to equal an expected value.
 * @param   reg      Pointer to the volatile register to monitor.
 * @param   mask     Bitmask of the bits to check.
 * @param   expected The exact value the masked bits should equal.
 * @param   max_iter Timeout counter (loop iterations).
 * @return  0 on success, -1 if timeout occurred.
 */
int wait_for_reg_mask_eq(volatile uint32_t *reg, uint32_t mask, uint32_t expected, uint32_t max_iter);


/**
 * @brief   Enter critical section (Global Interrupt Disable).
 * @details Uses the PRIMASK register to disable all configurable interrupts.
 * Only NMI and HardFault are left enabled.
 * @return  The original value of the PRIMASK register (0 or 1).
 */
static inline uint32_t enter_critical_primask(void) {
    uint32_t primask;
    __asm__ volatile (
        "MRS %0, PRIMASK\n\t"   /* Save current PRIMASK state */
        "CPSID I\n\t"           /* Disable interrupts globally */
        : "=r" (primask)
        :
        : "memory"
    );
    return primask;
}


/**
 * @brief   Exit critical section (Restore Interrupt State).
 * @details Restores the PRIMASK register to its previous state.
 * @param   state The original PRIMASK value returned by enter_critical_primask().
 */
static inline void exit_critical_primask(uint32_t state) {
    __asm__ volatile (
        "MSR PRIMASK, %0\n\t"   /* Restore PRIMASK state */
        :
        : "r" (state)
        : "memory"
    );
}


/**
 * @brief   Enter critical section (Selective Interrupt Masking).
 * @details Uses the BASEPRI register to mask interrupts at or below a specific
 * priority level. Higher priority interrupts remain enabled.
 * @param   new_basepri The priority threshold (e.g., 0x50).
 * 0 disables masking (enables all).
 * @return  The original value of the BASEPRI register.
 */
static inline uint32_t enter_critical_basepri(uint32_t new_basepri) {
    uint32_t old;
    __asm volatile (
        "MRS %0, BASEPRI\n"     /* Save current BASEPRI */
        "MSR BASEPRI, %1\n"     /* Set new masking threshold */
        : "=r"(old) : "r"(new_basepri) : "memory"
    );
    return old;
}


/**
 * @brief   Exit critical section (Restore BASEPRI).
 * @param   old The original BASEPRI value returned by enter_critical_basepri().
 */
static inline void exit_critical_basepri(uint32_t old) {
    __asm volatile (
        "MSR BASEPRI, %0\n"     /* Restore BASEPRI */
        : : "r"(old) : "memory"
    );
}


/**
 * @brief   Trigger a context switch.
 * @details Sets the PendSV (Pendable Service Call) flag. The context switch 
 * handler (PendSV_Handler) will execute as soon as interrupts allow.
 */
void yield_cpu(void);


/**
 * @brief Converts a string of numbers, for example "1234" into an int
 */
int atoi(const char *string);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */