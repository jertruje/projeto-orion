.section .text,"ax"

.global orion_x86_64_interrupts_enabled
.type orion_x86_64_interrupts_enabled,@function
orion_x86_64_interrupts_enabled:
    pushfq
    popq %rax
    andq $0x200, %rax
    setne %al
    movzbl %al, %eax
    ret
.size orion_x86_64_interrupts_enabled, .-orion_x86_64_interrupts_enabled

.global orion_x86_64_interrupts_save_disable
.type orion_x86_64_interrupts_save_disable,@function
orion_x86_64_interrupts_save_disable:
    pushfq
    popq %rax
    cli
    andq $0x200, %rax
    setne %al
    movzbl %al, %eax
    ret
.size orion_x86_64_interrupts_save_disable, .-orion_x86_64_interrupts_save_disable

.global orion_x86_64_interrupts_restore
.type orion_x86_64_interrupts_restore,@function
orion_x86_64_interrupts_restore:
    testq %rdi, %rdi
    jz .Lrestore_interrupts_disabled
    sti
    ret
.Lrestore_interrupts_disabled:
    cli
    ret
.size orion_x86_64_interrupts_restore, .-orion_x86_64_interrupts_restore

.global orion_x86_64_interrupts_disable
.type orion_x86_64_interrupts_disable,@function
orion_x86_64_interrupts_disable:
    cli
    ret
.size orion_x86_64_interrupts_disable, .-orion_x86_64_interrupts_disable

.global orion_x86_64_interrupts_enable
.type orion_x86_64_interrupts_enable,@function
orion_x86_64_interrupts_enable:
    sti
    ret
.size orion_x86_64_interrupts_enable, .-orion_x86_64_interrupts_enable
