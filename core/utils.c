#include "utils.h"

#define SCB_ICSR_PENDSVSET_Pos  28u
#define SCB_ICSR_PENDSVSET_Msk  (1u << SCB_ICSR_PENDSVSET_Pos)


int wait_for_flag_set(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter)
{
    uint32_t i = max_iter;
    while(((*reg) & mask) == 0U) {
        if (i-- == 0U) return -1;
    }
    return 0;
}


int wait_for_flag_clear(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter)
{
    uint32_t i = max_iter;
    while(((*reg) & mask) != 0U) {
        if (i-- == 0U) return -1;
    }
    return 0;
}


int wait_for_reg_mask_eq(volatile uint32_t *reg, uint32_t mask, uint32_t expected, uint32_t max_iter)
{
    uint32_t i = max_iter;
    while(((*reg) & mask) != expected) {
        if (i-- == 0U) return -1;
    }
    return 0;
}


void yield_cpu(void) {
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; 
    
    __DSB(); /* Ensures that all explicit memory accesses complete before any instruction after the DSB executes. */
    __ISB(); /* Flushes the processor pipeline and instruction cache. */
}
