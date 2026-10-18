.section .text,"ax"

.global orion_x86_64_context_switch
.type orion_x86_64_context_switch,@function
orion_x86_64_context_switch:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, (%rdi)
    movq %rsi, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
.size orion_x86_64_context_switch, .-orion_x86_64_context_switch

.global orion_x86_64_thread_start_trampoline
.type orion_x86_64_thread_start_trampoline,@function
orion_x86_64_thread_start_trampoline:
    subq $8, %rsp
    movq %r12, %rdi
    call orion_thread_start
    ud2
.size orion_x86_64_thread_start_trampoline, .-orion_x86_64_thread_start_trampoline

.global orion_x86_64_thread_returned
.type orion_x86_64_thread_returned,@function
orion_x86_64_thread_returned:
    ud2
    cli
.Lthread_return_halt:
    hlt
    jmp .Lthread_return_halt
.size orion_x86_64_thread_returned, .-orion_x86_64_thread_returned
