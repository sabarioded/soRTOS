#include <stdint.h>
#include <stddef.h>
#include "arch_ops.h"


void *arch_initialize_stack(void *top_of_stack, 
                            void (*task_func)(void *), 
                            void *arg, 
                            void (*exit_handler)(void))
{
    /* Cast void* to uint32_t* so we can decrement by 4 bytes */
    uint32_t *stk_ptr = (uint32_t *)top_of_stack;

    /* The Cortex-M stack must be 8-byte aligned at external interfaces. */
    stk_ptr = (uint32_t *)((uintptr_t)stk_ptr & ~0x7);

    /* Simulate the Hardware Stack Frame (Exception Frame)
     * When the task "starts", the CPU acts like it is returning from an interrupt.
     * It will pop these 8 registers automatically.
     */
    
    /* xPSR (Program Status Register) Bit 24 must be 1 for Thumb mode instructions. */
    *(--stk_ptr) = 0x01000000u;      
    
    /* PC (Program Counter) the function to run */
    *(--stk_ptr) = (uint32_t)task_func; 
    
    /* LR (Link Register) where to go if task_func returns */
    *(--stk_ptr) = (uint32_t)exit_handler;
    
    /* R12, R3, R2, R1 are general purpose */
    for (int i = 0; i < 4; i++) {
        *(--stk_ptr) = 0;
    }             
    
    /* R0 the argument passed to the function */
    *(--stk_ptr) = (uint32_t)arg;    

    /* R4-R11 are not saved by hardware but the PendSv_Handler expects to pop them */
    for (int i = 0; i < 8; i++) {
        *(--stk_ptr) = 0;
    }

    /* Return the new Top of Stack (Stack Pointer) */
    return (void *)stk_ptr;
}


void arch_reset(void) {
    /* Standard Cortex-M reset: Write 0x5FA to AIRCR with SYSRESETREQ bit */
    #define SCB_AIRCR_SYSRESETREQ_MASK (1 << 2)
    #define SCB_AIRCR_VECTKEY_POS      16U
    #define SCB_AIRCR_VECTKEY_VAL      0x05FA
    
    SCB->AIRCR = (SCB_AIRCR_VECTKEY_VAL << SCB_AIRCR_VECTKEY_POS) | 
                 SCB_AIRCR_SYSRESETREQ_MASK;
    
    // Wait for reset to happen
    while(1) { 
        arch_nop();
    }
}
