#ifndef ARCH_OPS_H
#define ARCH_OPS_H

#include <stdint.h>

/* --- Native/Host Architecture Implementation (Mock) --- */

/* Interrupt locking is a no-op on single-threaded host tests */
static inline uint32_t arch_irq_lock(void) { return 0; }
static inline void arch_irq_unlock(uint32_t s) { (void)s; }

/* Stubs for other architecture operations */
static inline void arch_nop(void) { }
static inline void arch_dmb(void) { }
static inline void arch_yield(void) { }

#endif /* ARCH_OPS_H */