OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)
ENTRY(__module_start)
PHDRS { text PT_LOAD FLAGS(5); rodata PT_LOAD FLAGS(4); data PT_LOAD FLAGS(6); dynamic PT_DYNAMIC FLAGS(6); }
SECTIONS {
  . = 0;
  .text : { KEEP(*(.text.crt0)) *(.text .text.*) } :text
  . = ALIGN(0x1000);
  .rodata : { KEEP(*(.rodata.mod0)) *(.rodata .rodata.*) } :rodata
  .hash : { *(.hash) } :rodata
  .dynsym : { *(.dynsym) } :rodata
  .dynstr : { *(.dynstr) } :rodata
  .rel.dyn : { *(.rel.dyn) } :rodata
  . = ALIGN(0x1000);
  .data : { *(.data .data.*) *(.got .got.*) } :data
  .dynamic : { *(.dynamic) } :data :dynamic
  . = ALIGN(16);
  HIDDEN(__bss_start = .);
  .bss (NOLOAD) : { *(.bss.module) *(.bss .bss.*) *(COMMON) } :data
  HIDDEN(__bss_end = .);
  /DISCARD/ : { *(.ARM.exidx* .ARM.extab*) *(.comment) *(.note*) }
  HIDDEN(native_input = 0x0027E774 - 0x02715000);
  HIDDEN(native_attachment = 0x003078B8 - 0x02715000);
  HIDDEN(native_matrices = 0x0093AF60 - 0x02715000);
  HIDDEN(idle_gateway = 0x013F1DEC + 224 - 0x02715000);
  HIDDEN(sync_gateway = 0x013F1DEC + 232 - 0x02715000);
  HIDDEN(mark_gateway = 0x013F1DEC + 240 - 0x02715000);
  HIDDEN(native_transition = 0x002AB670 - 0x02715000);
  HIDDEN(native_guard = 0x002F0670 - 0x02715000);
  HIDDEN(guard_end_gateway = 0x013F1DEC + 376 - 0x02715000);
  HIDDEN(guard_success_gateway = 0x013F1DEC + 384 - 0x02715000);
  HIDDEN(motion_gateway = 0x013F1DEC + 424 - 0x02715000);
  HIDDEN(native_geometry = 0x0016DDDC - 0x02715000);
}
