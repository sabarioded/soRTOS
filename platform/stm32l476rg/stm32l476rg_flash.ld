/*
 * Linker Script for STM32L476RG
 *
 * Memory Layout:
 * - FLASH (1MB)  : Code, Read-only data, ISR Vector
 * - SRAM1 (96KB) : .data, .bss, and Unified Heap (Task Stacks + User Malloc)
 * - SRAM2 (32KB) : Main Stack Pointer (MSP) for ISRs and Kernel
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(SRAM2) + LENGTH(SRAM2); /* MSP at end of SRAM2 */

/* Memory Definitions */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
  SRAM1 (xrw)     : ORIGIN = 0x20000000, LENGTH = 96K
  SRAM2 (xrw)     : ORIGIN = 0x10000000, LENGTH = 32K
}

SECTIONS
{
  /* -------------------------------------------------------------------------
   * FLASH SECTIONS
   * ------------------------------------------------------------------------- */

  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  /* The program code and other data */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* Global symbol at end of code */
  } > FLASH

  /* Constant data */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } > FLASH

  /* ARM Exception Handling */
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH

  /* C++ Constructors/Destructors */
  .preinit_array : {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } > FLASH

  .init_array : {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } > FLASH

  .fini_array : {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } > FLASH

  /* -------------------------------------------------------------------------
   * SRAM1 SECTIONS (Main RAM)
   * ------------------------------------------------------------------------- */

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* Global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    
    . = ALIGN(4);
    _edata = .;        /* Global symbol at data end */
  } > SRAM1 AT> FLASH

  /* Uninitialized data section */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;         /* Global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* Global symbol at bss end */
    __bss_end__ = _ebss;
  } > SRAM1

  /* * Unified Heap Section
   * Starts immediately after .bss and extends to the end of SRAM1.
   */
  .heap (NOLOAD) :
  {
    . = ALIGN(8);
    
    /* Define symbols used by memory_map.c */
    __heap_start__ = .;
    
    /* Optional standard symbols */
    PROVIDE ( end = . );
    PROVIDE ( _end = . );

    /* The heap ends at the physical end of SRAM1 */
    __heap_limit__ = ORIGIN(SRAM1) + LENGTH(SRAM1);
    __heap_end__ = __heap_limit__;
    
  } > SRAM1

  /* -------------------------------------------------------------------------
   * SRAM2 SECTIONS (MSP Stack)
   * ------------------------------------------------------------------------- */
   
  /* Main Stack Pointer (MSP) Region */
  .msp_stack (NOLOAD) :
  {
    . = ALIGN(8);
    __msp_stack_start__ = .;
    
    /* . is not moved to the end to avoid "section overlaps" errors if empty,
       but usually we just rely on _estack being set to ORIGIN+LENGTH */
    __msp_stack_end__ = ORIGIN(SRAM2) + LENGTH(SRAM2);
    
  } > SRAM2

  /* -------------------------------------------------------------------------
   * DEBUGGING
   * ------------------------------------------------------------------------- */
  /DISCARD/ :
  {
    *(.comment)
    *(.note*)
    *(.gnu*)
    *(.eh_frame*)
  }
}