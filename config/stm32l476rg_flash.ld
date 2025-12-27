/* Entry point */
ENTRY(Reset_Handler)

/* ============================================================================
   Memory Map for STM32L476RG/JE
   ============================================================================
   FLASH : 1 MB  at 0x08000000
   SRAM1 : 96 KB at 0x20000000
   SRAM2 : 32 KB at 0x10000000
   
   Memory Allocation Strategy:
   ---------------------------
   SRAM1: Used for .data, .bss, and heap
          - Task stacks are dynamically allocated from the heap
          - Heap grows upward from end of .bss to end of SRAM1
   
   SRAM2: Used exclusively for interrupt/kernel stack (MSP)
          - 32 KB dedicated to MSP (Main Stack Pointer)
          - Used by interrupt handlers and kernel code
   
   Stack Management:
   -----------------
   - MSP (Main Stack Pointer): Lives in SRAM2, used by interrupts/exceptions
   - PSP (Process Stack Pointer): Each task gets its own stack allocated from
     SRAM1 heap using heap_malloc()
============================================================================ */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
  SRAM1 (rwx) : ORIGIN = 0x20000000, LENGTH = 96K
  SRAM2 (rwx) : ORIGIN = 0x10000000, LENGTH = 32K
}

/* ============================================================================
   MSP Stack Configuration (SRAM2)
   ============================================================================
   The Main Stack Pointer (MSP) is used by:
   - Reset handler and startup code
   - Interrupt/exception handlers
   - Kernel/scheduler code (before task switching)
   
   MSP grows downward from top of SRAM2
============================================================================ */
__msp_stack_start__ = ORIGIN(SRAM2);
__msp_stack_end__   = ORIGIN(SRAM2) + LENGTH(SRAM2);

/* Initial MSP value (loaded from vector table word 0) */
_estack = __msp_stack_end__;

/* ============================================================================
   Heap Configuration (SRAM1)
   ============================================================================
   The heap extends from the end of .bss to the end of SRAM1.
   All task stacks are dynamically allocated from this heap.
   
   Heap usage:
   - Task stacks (allocated via heap_malloc during task_create)
   - Other dynamic allocations as needed
   
   The heap_init() function should use:
   - heap_start = (uint32_t)&_end
   - heap_end   = (uint32_t)__heap_limit__
============================================================================ */
__heap_limit__ = ORIGIN(SRAM1) + LENGTH(SRAM1);  /* Use all available SRAM1 */

/* ============================================================================
   Code and Read-Only Data (FLASH)
============================================================================ */
SECTIONS
{
  .text :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))  /* Vector table must be at start of FLASH */
    . = ALIGN(4);

    *(.text)              /* Code */
    *(.text.*)
    *(.glue_7)            /* ARM/Thumb interworking */
    *(.glue_7t)
    *(.init)              /* Constructor/destructor support */
    *(.fini)
    *(.rodata)            /* Read-only data */
    *(.rodata.*)

    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)        /* Exception index table */
    __exidx_end = .;

    . = ALIGN(4);
    _etext = .;           /* End of code section in FLASH */
  } > FLASH

  /* C++ constructor arrays */
  .preinit_array :
  {
    . = ALIGN(4);
    __preinit_array_start = .;
    KEEP(*(.preinit_array*))
    __preinit_array_end = .;
  } > FLASH

  .init_array :
  {
    . = ALIGN(4);
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array*))
    __init_array_end = .;
  } > FLASH

  .fini_array :
  {
    . = ALIGN(4);
    __fini_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
    KEEP(*(.fini_array*))
    __fini_array_end = .;
  } > FLASH

  /* ============================================================================
     Initialized Data (SRAM1)
     Loaded from FLASH at startup
  ============================================================================ */
  .data : AT (_etext)
  {
    . = ALIGN(4);
    _sdata = .;           /* Start of .data in RAM */
    *(.data)
    *(.data.*)
    . = ALIGN(4);
    _edata = .;           /* End of .data in RAM */
  } > SRAM1
  
  _sidata = LOADADDR(.data);  /* .data location in FLASH (for startup copy) */

  /* ============================================================================
     Zero-Initialized Data (SRAM1)
     Cleared to zero at startup
  ============================================================================ */
  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;            /* Start of .bss */
    *(.bss)
    *(.bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;            /* End of .bss */
  } > SRAM1

  /* ============================================================================
     End of static allocations - heap begins here
     Symbol '_end' marks the start of available heap space
  ============================================================================ */
  _end = .;               /* End of .bss = start of heap */

  /* ============================================================================
     Heap Section (Remainder of SRAM1)
     We define the start symbol here. The end symbol is simply the limit of SRAM1.
     We DO NOT force '.' to move to the end, preventing the overflow error.
   ============================================================================ */
  .heap (NOLOAD) :
  {
    . = ALIGN(8);
    __heap_start__ = .;
    /* We do not move '.' here. We just define the end symbol for the C code. */
    __heap_end__ = __heap_limit__;
  } > SRAM1

  /* ============================================================================
     MSP Stack Section (SRAM2)
   ============================================================================ */
  .msp_stack (NOLOAD) :
  {
    . = ALIGN(8);
    __msp_stack_bottom__ = .;
    /* Again, just define the symbol, don't move the dot */
    __msp_stack_top__ = __msp_stack_end__;
  } > SRAM2

  /* Discard debugging sections */
  /DISCARD/ :
  {
    *(.comment)
    *(.note*)
    *(.gnu*)
    *(.eh_frame*)
  }
}

/* ============================================================================
   Symbol Summary (for reference)
   ============================================================================
   
   FLASH:
   ------
   _etext           : End of code/rodata in FLASH
   _sidata          : Source address of .data in FLASH
   
   SRAM1 (.data/.bss):
   -------------------
   _sdata, _edata   : Start/end of initialized data
   _sbss, _ebss     : Start/end of zero-initialized data
   _end             : End of static allocations
   
   SRAM1 (Heap):
   -------------
   __heap_start__   : Start of heap (same as _end)
   __heap_limit__   : End of heap (end of SRAM1)
   __heap_end__     : End of heap (same as __heap_limit__)
   
   SRAM2 (MSP):
   ------------
   _estack          : Initial MSP value (top of SRAM2)
   __msp_stack_start__ : Bottom of MSP stack region
   __msp_stack_end__   : Top of MSP stack region
   
   Typical Usage in Code:
   ----------------------
   extern uint8_t _end;           // Start of heap
   extern uint32_t __heap_limit__; // End of heap
   
   void heap_init(void) {
       heap_start = (uint32_t)&_end;
       heap_end = (uint32_t)&__heap_limit__;
   }
==============================================================================*/
