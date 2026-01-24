#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "arch_ops.h"
#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t flag;
} spinlock_t;

/**
 * @brief Initialize a spinlock.
 * @param lock Pointer to the spinlock structure.
 */
static inline void spinlock_init(spinlock_t *lock) {
    lock->flag = 0;
}

/**
 * @brief Acquire a spinlock.
 * 
 * Disables interrupts locally. On SMP systems, it also acquires the 
 * hardware lock, spinning if necessary.
 * 
 * @param lock Pointer to the spinlock structure.
 * @return uint32_t Previous interrupt state (to be passed to unlock).
 */
static inline uint32_t spin_lock(spinlock_t *lock) {
    /* Always disable local interrupts first */
    uint32_t flags = arch_irq_lock();
    
#if defined(CONFIG_SMP)
    /* Atomic spin wait */
    while (arch_test_and_set(&lock->flag)) {
        arch_cpu_relax();
    }
#else
    (void)lock;
#endif
    
    return flags;
}

/**
 * @brief Release a spinlock.
 * 
 * Releases the hardware lock (on SMP) and restores the interrupt state.
 * 
 * @param lock Pointer to the spinlock structure.
 * @param flags Previous interrupt state returned by spin_lock().
 */
static inline void spin_unlock(spinlock_t *lock, uint32_t flags) {
#if defined(CONFIG_SMP)
    /* Release lock with memory barrier */
    arch_memory_barrier(); 
    lock->flag = 0;
#else
    (void)lock;
#endif
    
    /* Restore interrupts */
    arch_irq_unlock(flags);
}

#ifdef __cplusplus
}
#endif

#endif /* SPINLOCK_H */