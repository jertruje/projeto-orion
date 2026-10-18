.section .text,"ax"

.global orion_x86_64_enter_user
.type orion_x86_64_enter_user,@function
orion_x86_64_enter_user:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, .Lsaved_kernel_rsp(%rip)
    movq %cr3, %rax
    movq %rax, .Lsaved_kernel_cr3(%rip)

    movq %rdi, %r8
    movq %rsi, %r9
    movq %rdx, %cr3
    pushq $0x23
    pushq %r9
    pushq $0x2
    pushq $0x1b
    pushq %r8
    iretq
.size orion_x86_64_enter_user, .-orion_x86_64_enter_user

.global orion_x86_64_user_return_to_kernel
.type orion_x86_64_user_return_to_kernel,@function
orion_x86_64_user_return_to_kernel:
    movq 112(%r12), %rax
    movq .Lsaved_kernel_cr3(%rip), %rdx
    movq %rdx, %cr3
    movq .Lsaved_kernel_rsp(%rip), %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.size orion_x86_64_user_return_to_kernel, .-orion_x86_64_user_return_to_kernel

.section .bss,"aw",@nobits
.balign 8
.Lsaved_kernel_rsp:
    .quad 0
.Lsaved_kernel_cr3:
    .quad 0

.section .bss,"aw",@nobits
.balign 4096
.global orion_x86_64_syscall_stack_bottom
orion_x86_64_syscall_stack_bottom:
    .skip 16384
.global orion_x86_64_syscall_stack_top
orion_x86_64_syscall_stack_top:
