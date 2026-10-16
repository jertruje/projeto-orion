.section .text.entry,"ax"
.global _start
.type _start,@function

_start:
    cli
    leaq kernel_stack_top(%rip), %rsp
    andq $-16, %rsp
    xorq %rbp, %rbp
    call kernel_main

.Lhalt:
    cli
    hlt
    jmp .Lhalt

.size _start, .-_start

.section .bss.stack,"aw",@nobits
.balign 4096
.global kernel_stack_bottom
kernel_stack:
kernel_stack_bottom:
    .skip 65536
.global kernel_stack_top
kernel_stack_top:
