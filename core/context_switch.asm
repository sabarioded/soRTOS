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

/* Priorities: use lowest (0xFF) for PendSV & SysTick */
.equ PENDSV_PRIO_SHIFT,   16
.equ SYSTICK_PRIO_SHIFT,  24

/* EXC_RETURN: Thread mode, return to use PSP */
.equ EXC_RETURN_THREAD_PSP, 0xFFFFFFFD


/* Set up and start the first task */
task_create_first:
    /* set the Main Stack pointer to the top of the ISR stack */
    LDR r0, =_estack            /* load address of stack into R0 */
    MSR MSP, r0                 /* Copy R0 into the Main Stack Pointer */

    /* set PendSV and SysTick to lowest priority */
    LDR r0, =SCB_SHPR3          /* save the address of SCB_SHPR3 in R0 */
    LDR r1, [r0]                /* load the 32 bit value at SCB_SHPR3 in R1 */
    LDR r2, =0xFF               /* save the 8bit mask in R2 */
    ORR r1, r1, r2, LSL #PENDSV_PRIO_SHIFT  /* set PendSV priority byte */
    ORR r1, r1, r2, LSL #SYSTICK_PRIO_SHIFT /* set SysTick priority byte */
    STR r1, [r0]                /* store the updated priority value back to SCB_SHPR3 */

    /* set PSP = task_current->psp so thread mode tasks will use it */
    LDR r0, =task_current       /* r0 = &task_current */
    LDR r0, [r0]                /* r0 = task_current */
    LDR r0, [r0]                /* r0 = task_current->psp */
    LDMIA r0!, {r4-r11}         /* restore R4-R11 from task stack */
    MSR PSP, r0                 /* PSP = task_current->psp */

    /* update control to priviliged thread mode */
    MOVS r0, #2                 /* r0 = 0x2 */
    MSR CONTROL, r0             /* CONTROL = 0x2 */
    ISB                         /* instruction sync barrier */

    /* perform an exception return to start the first task */
    LDR r0, =EXC_RETURN_THREAD_PSP  /* load EXC_RETURN value into R0 */
    BX r0                           /* branch to EXC_RETURN value to start the first task */


/* Perform a context switch */
PendSV_Handler:
    /* first we need to save the task context in the stack */
    MRS r0, PSP                 /* r0 = PSP */
    STMDB r0!, {r4-r11}         /* save R4-R11 on task stack */
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
    LDMIA r0!, {r4-r11}         /* restore R4-R11 from task stack */
    MSR PSP, r0                 /* PSP = r0 */

    /* update task_current = task_next */
    LDR r2, =task_next          /* r2 = &task_next */
    LDR r2, [r2]                /* r2 = task_next */
    LDR r1, =task_current       /* r1 = &task_current */
    STR r2, [r1]                /* task_current = task_next */

    BX lr                       /* exception return to the next task */

