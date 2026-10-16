OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(orion_toolchain_probe)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x100000;
    .text : { *(.text .text.*) } :text
    .rodata : { *(.rodata .rodata.*) } :text
    . = ALIGN(0x1000);
    .data : { *(.data .data.*) } :data
    .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } :data
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
