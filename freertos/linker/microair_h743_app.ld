ENTRY(Reset_Handler)

/* Keep the original PX4 bootloader below the application. The application starts
 * at 0x08020000 and must stay below the parameter sector at 0x081E0000. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08020000, LENGTH = 1792K
  PARAM (rw)  : ORIGIN = 0x081E0000, LENGTH = 128K
  DTCM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  RAM   (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
  RAM_D2(xrw) : ORIGIN = 0x30000000, LENGTH = 288K
  RAM_D3(xrw) : ORIGIN = 0x38000000, LENGTH = 64K
}

_Min_Heap_Size = 0x1000;
_Min_Stack_Size = 0x1000;

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    /* PX4 bootloader delay signature used by the existing board. */
    . = ALIGN(32);
    KEEP(*(.bootloader_signature))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text .text.*)
    *(.glue_7) *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata .rodata.*)
    . = ALIGN(4);
  } > FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > FLASH
  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > FLASH

  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data .data.*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .bss (NOLOAD) :
  {
    . = ALIGN(8);
    _sbss = .;
    *(.bss .bss.*) *(COMMON)
    . = ALIGN(8);
    _ebss = .;
  } > RAM

  .dma_buffer (NOLOAD) :
  {
    . = ALIGN(32);
    *(.dma_buffer .dma_buffer.*)
    . = ALIGN(32);
  } > RAM_D2

  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . += _Min_Heap_Size;
    /* The board bootloader disables DTCM. Use the reserved AXI RAM stack
     * from the first instruction of Reset_Handler, before any C call. */
    _sstack = .;
    . += _Min_Stack_Size;
    . = ALIGN(8);
    _estack = .;
  } > RAM

  .parameter_store (NOLOAD) :
  {
    __parameter_start__ = ORIGIN(PARAM);
    __parameter_end__ = ORIGIN(PARAM) + LENGTH(PARAM);
  } > PARAM

  /DISCARD/ : { *(.note.GNU-stack) }
}
