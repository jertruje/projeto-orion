.section .text,"ax"
.global orion_arch_disable_interrupts
.type orion_arch_disable_interrupts,@function

orion_arch_disable_interrupts:
    cli
    ret

.size orion_arch_disable_interrupts, .-orion_arch_disable_interrupts

.global orion_arch_halt
.type orion_arch_halt,@function

orion_arch_halt:
.Lhalt_forever:
    cli
    hlt
    jmp .Lhalt_forever

.size orion_arch_halt, .-orion_arch_halt
