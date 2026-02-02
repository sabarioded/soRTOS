.syntax unified
.cpu cortex-m4
.thumb

.extern schedule_next_task      
.extern task_get_current

.global task_create_first
.global PendSV_Handler

/* Symbols from linker script */     
.extern _estack                 /* top of ISR stack */

/* Interrupt Control and State Register */
.equ SCB_ICSR,       0xE000ED04 

/* System Handler Priority Register 3 - [31:24]SysTick, [23:16] PendSV */
.equ SCB_SHPR3,      0xE000ED20

/* Change PendSV to pending */
.equ SCB_ICSR_PENDSVSET,  (1 << 28)

/* Priorities: use lowest (left-aligned) for PendSV & SysTick (0xF0 for 4 implemented bits) */
.equ PENDSV_PRIO_SHIFT,   16
.equ SYSTICK_PRIO_SHIFT,  24

/* EXC_RETURN: Thread mode, return to use PSP */
.equ EXC_RETURN_THREAD_PSP, 0xFFFFFFFD

/*
 * Stack layout expectations:
 *
 * 1) Initial task stack (created by arch_initialize_stack):
 *    [R4-R11][HW frame: R0-R3,R12,LR,PC,xPSR]
 *
 * 2) After a context switch (PendSV saves):
 *    [optional s16-s31][R4-R11][EXC_RETURN][HW frame]
 *
 * task_create_first restores R4-R11 then performs an exception return
 * so the CPU pops the HW frame. PendSV restores EXC_RETURN/R4-R11 and
 * optionally FP regs before returning.
 */

/* Set up and start the first task */
.type task_create_first, %function
task_create_first:
    /* set the Main Stack pointer to the top of the ISR stack */
    LDR r0, =_estack            /* load address of stack into R0 */
    MSR MSP, r0                 /* Copy R0 into the Main Stack Pointer */

    /* Setup Priorities (PendSV lowest) */
    LDR r0, =SCB_SHPR3          /* address of SCB_SHPR3 */
    LDR r1, [r0]                /* current SHPR3 value */

    /* Set PendSV = 0xF0 (lowest) */
    MOVS r2, #0xF0                          /* r2 = 0xE0 */
    BIC r1, r1, r2, LSL #PENDSV_PRIO_SHIFT  /* clear: r1  &= ~(r2 << PENDSV_PRIO_SHIFT) */
    ORR r1, r1, r2, LSL #PENDSV_PRIO_SHIFT  /* insert: r1 |=  (r2 << PENDSV_PRIO_SHIFT) */

    /* Set SysTick = 0xE0 (one level higher than PendSV) */
    MOVS r3, #0xE0                          /* r3 = 0xE0 */
    BIC r1, r1, r3, LSL #SYSTICK_PRIO_SHIFT /* clear: r1  &= ~(r3 << SYSTICK_PRIO_SHIFT) */
    ORR r1, r1, r3, LSL #SYSTICK_PRIO_SHIFT /* insert: r1 |=  (r3 << SYSTICK_PRIO_SHIFT) */

    STR r1, [r0]                            /* write back updated SHPR3 */

    /* set PSP = task_get_current()->psp so thread mode tasks will use it */
    BL  task_get_current        /* r0 = task_current */
    LDR r0, [r0]                /* r0 = task_current->psp */
    LDMIA r0!, {r4-r11}         /* restore R4-R11 from task stack */
    MSR PSP, r0                 /* PSP = task_current->psp (now at HW frame) */

    /* update control to priviliged thread mode */
    MOVS r0, #2                 /* r0 = 0x2 */
    MSR CONTROL, r0             /* CONTROL = 0x2 */
    ISB                         /* instruction sync barrier */

    /* Exception return to start the first task */
    LDR lr, =EXC_RETURN_THREAD_PSP
    BX  lr


/* Perform a context switch */
.type PendSV_Handler, %function
PendSV_Handler:
    /* first we need to save the task context in the stack */
    MRS r0, PSP                 /* r0 = PSP */

    /* Check if FPU was used (Bit 4 of LR is 0) */
#if defined(__ARM_FP) && (__ARM_FP != 0)
    TST lr, #0x10               /* test EXC_RETURN bit 4 (0 means FP context stacked) */
    IT EQ                       /* if zero, execute next instruction */
    VSTMDBEQ r0!, {s16-s31}     /* Save FPU Callee-Saved Registers */
#endif

    STMDB r0!, {r4-r11}         /* save R4-R11 on task stack */
    STMDB r0!, {lr}             /* save EXC_RETURN on task stack */

    /* Save PSP into current task */
    MOV r4, r0                  /* r4 = updated PSP */
    BL  task_get_current        /* result of task_current returned to r0 */
    MOV r1, r0                  /* r1 = task_current */
    STR r4, [r1]                /* task_current->psp = r4 */

    /* call the scheduler to select the next task */
    PUSH {lr}                   /* save handler’s LR (EXC_RETURN) */
    BL   schedule_next_task     /* call scheduler to select the next task */
    POP  {lr}                   /* restore EXC_RETURN so BX lr works later */

    /* restore the context of the next task */
    BL  task_get_current        /* result of task_current returned to r0 */
    LDR r0, [r0]                /* r0 = task_current->psp */
    
    LDMIA r0!, {lr}             /* restore EXC_RETURN */
    LDMIA r0!, {r4-r11}         /* restore R4-R11 from task stack */

    /* Check if we need to restore FPU */
#if defined(__ARM_FP) && (__ARM_FP != 0)
    TST lr, #0x10
    IT EQ
    VLDMIAEQ r0!, {s16-s31}     /* Restore FPU Callee-Saved Registers */
#endif

    MSR PSP, r0                 /* PSP = r0 */

    BX lr                       /* exception return to the next task */
