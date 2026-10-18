# x86-64 interrupt frame

The stubs normalize the top of the frame to `vector`, `error_code`, then the
CPU-saved return frame. Before calling C they push, in order, `RAX`, `RBX`,
`RCX`, `RDX`, `RBP`, `RSI`, `RDI`, `R8` through `R15`. Therefore the C pointer
starts with these fields in order:

```text
r15..r8, rdi, rsi, rbp, rdx, rcx, rbx, rax,
vector, error_code, rip, cs, rflags
```

The fixed register prefix is 120 bytes; vector/error/RIP offsets are 120, 128
and 136 bytes. Compile-time assertions enforce the offsets. On IST stack
switches and privilege transitions, the CPU also saves old RSP/SS after the
fixed fields; this implementation does not include those optional tail fields
in the C struct because the fatal dispatcher does not inspect them.

The common stub clears DF for the C ABI, aligns RSP down to 16 bytes before
`call`, uses the saved R12 slot to restore the original frame pointer, restores
all general registers, removes vector/error slots and executes `iretq`.
CPU exceptions use vectors 0–31. PIT IRQ0 uses vector 32, whose normalized
frame has the same register/vector/error/RIP layout; the timer dispatcher may
switch to another thread and later resume this handler before `iretq`.

This interrupt-time scheduling composition has been exercised only on one
QEMU vCPU. No privilege-transition return frame, APIC vector set, FPU/SIMD
state, or SMP-specific interrupt stack is implemented.
