
OUTPUT_ARCH(riscv)
ENTRY( _start )
MEMORY
{
  MEM           : ORIGIN = 0x00000004, LENGTH = 0x00100000
}


SECTIONS
{
  .init :
  {
    . = ALIGN(8);
    KEEP( *(.init) )
    . = ALIGN(8);
  } > MEM


  .fini :
  {
    . = ALIGN(8);
    KEEP( *(.fini) )
    . = ALIGN(8);
  } > MEM


  .preinit_array : {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(8);
  } > MEM


  .init_array : {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__init_array_start = .);
    __CTOR_LIST__ = .;
    LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
    KEEP(*(.ctors.start))
    KEEP(*(.ctors))
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array ))
    LONG(0)
    __CTOR_END__ = .;
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(8);
  } > MEM


  .fini_array : {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__fini_array_start = .);
    __DTOR_LIST__ = .;
    LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
    KEEP(*(.dtors.start))
    KEEP(*(.dtors))
    LONG(0)
    __DTOR_END__ = .;
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array ))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(8);
  } > MEM


  .boot : {
    . = ALIGN(8);
    *(.boot)
    *(.boot.data)
    . = ALIGN(8);
  } > MEM


  .rodata : {
    . = ALIGN(8);
    *(.rodata);
    *(.rodata.*)
    *(.srodata);
    *(.srodata.*)
    *(.eh_frame*)
    *(.gnu.linkonce.r.*)
    . = ALIGN(8);
  } > MEM


  .got : {
    . = ALIGN(8);
    *(.got.plt) * (.igot.plt) *(.got) *(.igot)
    . = ALIGN(8);
  } > MEM


  .shbss : {
    . = ALIGN(8);
    *(.shbss)
    . = ALIGN(8);
  } > MEM


  .talias : {
  } > MEM


  .gnu.offload_funcs : {
    . = ALIGN(8);
    KEEP(*(.gnu.offload_funcs))
    . = ALIGN(8);
  } > MEM


  .gnu.offload_vars : {
    . = ALIGN(8);
    KEEP(*(.gnu.offload_vars))
    . = ALIGN(8);
  } > MEM


  .stack : {
    . = ALIGN(8);
    . = ALIGN(16);
    stack_start = .;
    . = . + 0x800;
    stack = .;
    . = ALIGN(8);
  } > MEM


  .data : {
    . = ALIGN(8);
    sdata  =  .;
    _sdata  =  .;
    *(.data);
    *(.data.*)
    *(.sdata);
    *(.sdata.*)
    . = ALIGN(8);
    edata  =  .;
    _edata  =  .;
  } > MEM


  .bss : {
    . = ALIGN(8);
    _bss_start = .;
    *(.bss)
    *(.bss.*)
    *(.sbss)
    *(.sbss.*)
    *(COMMON)
    . = ALIGN(8);
    _bss_end = .;
  } > MEM



  .text :
  {
    . = ALIGN(16);
    _stext = .;
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t.*)
    _etext  =  .;
    *(.lit)
    *(.shdata)
    /* This alignment is needed on RTL platforms to avoid getting X in the code */
    . = ALIGN(16);
    _endtext = .;
    __cluster_text_end = .;
    /* This alignment is needed on RTL platforms to avoid getting X in the code */
    . = ALIGN(16);
  } > MEM

  __mem_end = ALIGN(8);

}
