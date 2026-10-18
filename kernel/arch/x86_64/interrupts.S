.section .text,"ax"

.global orion_x86_64_load_gdt
.type orion_x86_64_load_gdt,@function
orion_x86_64_load_gdt:
    lgdt (%rdi)
    pushq $0x08
    leaq .Lgdt_reload_cs(%rip), %rax
    pushq %rax
    lretq
.Lgdt_reload_cs:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs
    movw $0x28, %ax
    ltr %ax
    ret
.size orion_x86_64_load_gdt, .-orion_x86_64_load_gdt

.global orion_x86_64_load_idt
.type orion_x86_64_load_idt,@function
orion_x86_64_load_idt:
    lidt (%rdi)
    ret
.size orion_x86_64_load_idt, .-orion_x86_64_load_idt

.macro ISR_NO_ERROR vector
.global orion_x86_64_isr_\vector
.type orion_x86_64_isr_\vector,@function
orion_x86_64_isr_\vector:
    pushq $0
    pushq $\vector
    jmp orion_x86_64_interrupt_common
.size orion_x86_64_isr_\vector, .-orion_x86_64_isr_\vector
.endm

.macro ISR_ERROR vector
.global orion_x86_64_isr_\vector
.type orion_x86_64_isr_\vector,@function
orion_x86_64_isr_\vector:
    pushq $\vector
    jmp orion_x86_64_interrupt_common
.size orion_x86_64_isr_\vector, .-orion_x86_64_isr_\vector
.endm

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_ERROR 8
ISR_NO_ERROR 9
ISR_ERROR 10
ISR_ERROR 11
ISR_ERROR 12
ISR_ERROR 13
ISR_ERROR 14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_ERROR 17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_ERROR 21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_ERROR 29
ISR_ERROR 30
ISR_NO_ERROR 31
ISR_NO_ERROR 32
ISR_NO_ERROR 128

.global orion_x86_64_interrupt_common
.type orion_x86_64_interrupt_common,@function
orion_x86_64_interrupt_common:
    cld
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %rdi
    movq %rsp, %r12
    andq $-16, %rsp
    call orion_x86_64_exception_dispatch
    movq %r12, %rsp
    cmpq $-1, 120(%r12)
    je orion_x86_64_user_return_to_kernel
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq
.size orion_x86_64_interrupt_common, .-orion_x86_64_interrupt_common

.section .rodata,"a"
.balign 8
.global orion_x86_64_isr_stub_table
.type orion_x86_64_isr_stub_table,@object
orion_x86_64_isr_stub_table:
    .quad orion_x86_64_isr_0, orion_x86_64_isr_1
    .quad orion_x86_64_isr_2, orion_x86_64_isr_3
    .quad orion_x86_64_isr_4, orion_x86_64_isr_5
    .quad orion_x86_64_isr_6, orion_x86_64_isr_7
    .quad orion_x86_64_isr_8, orion_x86_64_isr_9
    .quad orion_x86_64_isr_10, orion_x86_64_isr_11
    .quad orion_x86_64_isr_12, orion_x86_64_isr_13
    .quad orion_x86_64_isr_14, orion_x86_64_isr_15
    .quad orion_x86_64_isr_16, orion_x86_64_isr_17
    .quad orion_x86_64_isr_18, orion_x86_64_isr_19
    .quad orion_x86_64_isr_20, orion_x86_64_isr_21
    .quad orion_x86_64_isr_22, orion_x86_64_isr_23
    .quad orion_x86_64_isr_24, orion_x86_64_isr_25
    .quad orion_x86_64_isr_26, orion_x86_64_isr_27
    .quad orion_x86_64_isr_28, orion_x86_64_isr_29
    .quad orion_x86_64_isr_30, orion_x86_64_isr_31
    .quad orion_x86_64_isr_32
.size orion_x86_64_isr_stub_table, .-orion_x86_64_isr_stub_table
