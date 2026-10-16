ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x200000;
    .text : ALIGN(0x1000)
    {
        KEEP(*(.text.entry))
        *(.text .text.*)
    } :text

    .rodata : ALIGN(0x1000)
    {
        *(.rodata .rodata.*)
    } :text

    . = ALIGN(0x1000);
    .data :
    {
        *(.data .data.*)
    } :data

    .bss (NOLOAD) :
    {
        *(COMMON)
        *(.bss.stack)
        *(.bss .bss.*)
    } :data

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
    }
}
