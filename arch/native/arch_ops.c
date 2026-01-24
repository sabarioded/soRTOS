#include "arch_ops.h"
#include <stdlib.h>

/* Initialize the stack frame for a task */
void* arch_initialize_stack(void *top_of_stack, 
                            void (*task_func)(void *), 
                            void *arg, 
                            void (*task_exit)(void)) {
    (void)task_func;
    (void)arg;
    (void)task_exit;
    /* 
     * For host tests, we don't simulate the hardware stack frame layout.
     * We just return the pointer as-is because the context switching 
     * is handled by setjmp/longjmp which uses the host's actual stack.
     */
    return top_of_stack;
}

/* Reset the processor (Exit the test runner) */
void arch_reset(void) {
    exit(0);
}