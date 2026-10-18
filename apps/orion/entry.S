.section .text.entry,"ax"
.global _start
.type _start,@function
_start:
    andq $-16, %rsp
    call orion_app_main
    movq %rax, %rdi
    call orion_exit
.Lunexpected_exit_return:
    ud2
    jmp .Lunexpected_exit_return
.size _start, .-_start
