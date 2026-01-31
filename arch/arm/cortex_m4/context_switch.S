.syntax unified
.cpu cortex-m4
.thumb

.extern task_current             
.extern task_next                
.extern schedule_next_task      

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

    /* set PSP = task_current->psp so thread mode tasks will use it */
    LDR r0, =task_current       /* r0 = &task_current */
    LDR r0, [r0]                /* r0 = task_current */
    LDR r0, [r0]                /* r0 = task_current->psp */
    ADD r0, r0, #4              /* Skip LR (EXC_RETURN) on stack */
    LDMIA r0!, {r4-r11}         /* restore R4-R11 from task stack */
    MSR PSP, r0                 /* PSP = task_current->psp */

    /* update control to priviliged thread mode */
    MOVS r0, #2                 /* r0 = 0x2 */
    MSR CONTROL, r0             /* CONTROL = 0x2 */
    ISB                         /* instruction sync barrier */

    /* Manually pop hardware frame */
    POP {r0-r3}       /* Pop R0 (arg), R1, R2, R3 */
    POP {r4}          /* Pop R12 (garbage) into R4 */
    POP {lr}          /* Pop LR (Return Address / Error Handler) */
    POP {r5}          /* Pop PC (Task Entry Point) into R5 */
    POP {r6}          /* Pop xPSR (Status Reg) into R6 */

    /* Jump to the task */
    BX  r5            /* Jump to the address we popped into R5 */


/* Perform a context switch */
.type PendSV_Handler, %function
PendSV_Handler:
    /* first we need to save the task context in the stack */
    MRS r0, PSP                 /* r0 = PSP */

    /* Check if FPU was used (Bit 4 of LR is 0) */
    TST lr, #0x10
    IT EQ
    VSTMDBEQ r0!, {s16-s31}     /* Save FPU Callee-Saved Registers */

    STMDB r0!, {r4-r11}         /* save R4-R11 on task stack */
    STMDB r0!, {lr}             /* save EXC_RETURN on task stack */

    LDR r1, =task_current       /* r1 = &task_current */
    LDR r1, [r1]                /* r1 = task_current */
    STR r0, [r1]                /* task_current->psp = r0 */

    /* call the scheduler to select the next task */
    PUSH {lr}                   /* save handler’s LR (EXC_RETURN) */
    BL   schedule_next_task     /* call scheduler to select the next task */
    POP  {lr}                   /* restore EXC_RETURN so BX lr works later */

    /* restore the context of the next task */
    LDR r1, =task_next          /* r1 = &task_next */
    LDR r1, [r1]                /* r1 = task_next */
    LDR r0, [r1]                /* r0 = task_next->psp */
    
    LDMIA r0!, {lr}             /* restore EXC_RETURN */
    LDMIA r0!, {r4-r11}         /* restore R4-R11 from task stack */

    /* Check if we need to restore FPU */
    TST lr, #0x10
    IT EQ
    VLDMIAEQ r0!, {s16-s31}     /* Restore FPU Callee-Saved Registers */

    MSR PSP, r0                 /* PSP = r0 */

    /* update task_current = task_next */
    LDR r2, =task_next          /* r2 = &task_next */
    LDR r2, [r2]                /* r2 = task_next */
    LDR r1, =task_current       /* r1 = &task_current */
    STR r2, [r1]                /* task_current = task_next */

    BX lr                       /* exception return to the next task */
