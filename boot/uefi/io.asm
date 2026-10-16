bits 64

section .text
global orion_outb
global orion_inb

orion_outb:
    mov al, dl
    mov dx, cx
    out dx, al
    ret

orion_inb:
    mov dx, cx
    xor eax, eax
    in al, dx
    ret
