#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "arch_ops.h"
#include "project_config.h"

typedef struct {
    volatile uint32_t flag;
} spinlock_t;

static inline void spinlock_init(spinlock_t *lock) {
    lock->flag = 0;
}

#if defined(CONFIG_SMP)

/* Multi-Core Implementation */
static inline uint32_t spin_lock(spinlock_t *lock) {
    /* Disable local interrupts */
    uint32_t flags = arch_irq_lock();
    
    /* Atomic spin wait */
    while (arch_test_and_set(&lock->flag)) {
        arch_cpu_relax();
    }
    
    return flags;
}

static inline void spin_unlock(spinlock_t *lock, uint32_t flags) {
    /* Release lock with memory barrier */
    arch_memory_barrier(); 
    lock->flag = 0;
    
    /* Restore interrupts */
    arch_irq_unlock(flags);
}

#else /* Single Core */

/* Single-Core Implementation (Interrupt Disabling) */
static inline uint32_t spin_lock(spinlock_t *lock) {
    (void)lock;
    return arch_irq_lock();
}

static inline void spin_unlock(spinlock_t *lock, uint32_t flags) {
    (void)lock;
    arch_irq_unlock(flags);
}

#endif /* CONFIG_SMP */

#endif /* SPINLOCK_H */