.section .text,"ax"
.global orion_x86_64_validate_entry_state
.type orion_x86_64_validate_entry_state,@function

orion_x86_64_validate_entry_state:
    # The System V AMD64 ABI requires RSP % 16 == 8 at function entry.
    movq %rsp, %rax
    cmpq %rdi, %rax
    jb .Linvalid_entry_state
    cmpq %rsi, %rax
    jae .Linvalid_entry_state
    andq $0x0f, %rax
    cmpq $8, %rax
    jne .Linvalid_entry_state

    movq %cr0, %rax
    testl $0x00000001, %eax
    jz .Linvalid_entry_state
    btq $31, %rax
    jnc .Linvalid_entry_state

    movq %cr4, %rax
    btq $5, %rax
    jnc .Linvalid_entry_state

    movl $0xc0000080, %ecx
    rdmsr
    testl $0x00000400, %eax
    jz .Linvalid_entry_state

    pushfq
    popq %rax
    testq $0x00000200, %rax
    jnz .Linvalid_entry_state

    movl $1, %eax
    ret

.Linvalid_entry_state:
    xorl %eax, %eax
    ret

.size orion_x86_64_validate_entry_state, .-orion_x86_64_validate_entry_state

.global orion_x86_64_nx_enabled
.type orion_x86_64_nx_enabled,@function

orion_x86_64_nx_enabled:
    pushq %rbx
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb .Lnx_unavailable

    movl $0x80000001, %eax
    cpuid
    btl $20, %edx
    jnc .Lnx_unavailable

    movl $0xc0000080, %ecx
    rdmsr
    btl $11, %eax
    jnc .Lnx_unavailable

    movl $1, %eax
    popq %rbx
    ret

.Lnx_unavailable:
    xorl %eax, %eax
    popq %rbx
    ret

.size orion_x86_64_nx_enabled, .-orion_x86_64_nx_enabled
