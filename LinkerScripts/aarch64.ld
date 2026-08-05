OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(__rocrt_init)

PHDRS
{
  text      PT_LOAD     FLAGS(5); /* PF_R | PF_X */
  rodata    PT_LOAD     FLAGS(4); /* PF_R */
  data      PT_LOAD     FLAGS(6); /* PF_R | PF_W */
  dynamic   PT_DYNAMIC;
}

SECTIONS
{
  .text : {
    PROVIDE_HIDDEN(__EX_start = .);
    KEEP(*(.rocrt.init))
    KEEP(*(.text.crt0))
    . = ALIGN(0x100);
    *(.text .text.*)
    PROVIDE_HIDDEN(__EX_end = .);
  } :text

  . = ALIGN(0x1000);

  .rocrt.initro : {
    KEEP(*(.rocrt.initro))
  } :rodata

  .nx_debuglink : {
    PROVIDE_HIDDEN(__nx_debuglink_start = .);
    KEEP(*(.nx_debuglink))
    PROVIDE_HIDDEN(__nx_debuglink_end = .);
  } :rodata

  .rocrt.info : {
    KEEP(*(.rocrt.info))
  } :rodata

  .rodata : {
    PROVIDE_HIDDEN(__rodata_start = .);
    *(.rodata .rodata.*);
    PROVIDE_HIDDEN(__rodata_end = .);
  } :rodata

  .hash     : { *(.hash) } :rodata
  .gnu.hash : { *(.gnu.hash) } :rodata
  .dynsym   : { *(.dynsym .dynsym.*) } :rodata
  .dynstr   : { *(.dynstr .dynstr.*) } :rodata

  PROVIDE_HIDDEN(__rel_dyn_start = .);
  .rel.dyn : { *(.rel.dyn) } :rodata
  PROVIDE_HIDDEN(__rel_dyn_end = .);

  PROVIDE_HIDDEN(__rela_dyn_start = .);
  .rela.dyn : { *(.rela.dyn) } :rodata
  PROVIDE_HIDDEN(__rela_dyn_end = .);

  PROVIDE_HIDDEN(__rel_plt_start = .);
  .rel.plt        : { *(.rel.plt) } :rodata
  PROVIDE_HIDDEN(__rel_plt_end = .);

  PROVIDE_HIDDEN(__rela_plt_start = .);
  .rela.plt       : { *(.rela.plt) } :rodata
  PROVIDE_HIDDEN(__rela_plt_end = .);

  .gcc_except_table : { *(.gcc_except_table .gcc_except_table.*) } :rodata
  .eh_frame_hdr : {
    PROVIDE_HIDDEN(__eh_frame_hdr_start = .);
    *(.eh_frame_hdr)
    PROVIDE_HIDDEN(__eh_frame_hdr_end = .);
  } :rodata
  .eh_frame : { KEEP (*(.eh_frame)) } :rodata

  .note.gnu.build-id : {
    PROVIDE_HIDDEN(__note_gnu_build_id_start = .);
    *(.note.gnu.build-id)
    PROVIDE_HIDDEN(__note_gnu_build_id_end = .);
  } :rodata

  . = ALIGN(0x1000);

  PROVIDE_HIDDEN(__relro_start = .);

  .init_array ALIGN(8) : {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP (*(.init_array))
    PROVIDE_HIDDEN(__init_array_end  = .);
  } :data

  .fini_array ALIGN(8) : {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
    KEEP (*(.fini_array))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } :data

  .tdata : {
    /* these values make no sense */
    PROVIDE_HIDDEN(__tdata_align_abs = __EX_start + 1);
    PROVIDE_HIDDEN(__tdata_align_rel = . + 1);
    PROVIDE_HIDDEN(__tdata_start = .);
    *(.tdata .tdata.*)
    PROVIDE_HIDDEN(__tdata_end = .);
  } :data

  .tbss : {
    PROVIDE_HIDDEN(__tbss_align_abs = __EX_start + 1);
    PROVIDE_HIDDEN(__tbss_align_rel = . + 1);
    PROVIDE_HIDDEN(__tbss_start = .);
    *(.tbss .tbss.*)
    *(.tcommon)
    PROVIDE_HIDDEN(__tbss_end = .);
  } :data

  .data.rela.ro : {
    *(.data.rela.ro.local*)
    *(.data.rela.ro .data.rela.ro.*)
  } :data

  .data.rel.ro : {
    *(.data.rel.ro.local*)
    *(.data.rel.ro .data.rel.ro.*)
  } :data

  .dynamic : {
    PROVIDE_HIDDEN(__dynamic_start = .);
    *(.dynamic)
    PROVIDE_HIDDEN(__dynamic_end = .);
  } :data :dynamic

  PROVIDE_HIDDEN(__got_start = .);
  .got : { *(.got) *(.igot) } :data
  PROVIDE_HIDDEN(__got_end = .);

  PROVIDE_HIDDEN(__relro_end = .);

  PROVIDE_HIDDEN(__got_plt_start = .);
  .got.plt : { *(.got.plt) *(.igot.plt) } :data
  PROVIDE_HIDDEN(__got_plt_end = .);

  . = ALIGN(0x1000);

  PROVIDE_HIDDEN(__full_relro_end = .);

  .data : {
    *(.data .data.*)
  } :data

  .atexit : {
    PROVIDE_HIDDEN(__atexit_start = .);
    *(.atexit)
    PROVIDE_HIDDEN(__atexit_end = .);
  } :data

  .bss : {
    PROVIDE_HIDDEN(__bss_start = .);
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(8);
    PROVIDE_HIDDEN(__bss_end = .);
    . = ALIGN(0x1000);
  } :data

  PROVIDE_HIDDEN(__rtld_arguments = .);

  /DISCARD/ : {
    *(.interp)
  }
}