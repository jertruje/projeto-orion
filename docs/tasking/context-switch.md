# x86-64 context switching

## Cooperative call ABI

`orion_x86_64_context_switch` is an ordinary System V AMD64 function. It saves
the caller's RSP after pushing RBP, RBX and R12–R15, loads the next saved stack
pointer, restores those six callee-saved GPRs, and returns through the
selected stack's call return address. Caller-saved registers are preserved by
the ordinary C caller when needed.

New kernel-thread stacks contain this restore frame, the thread pointer in
R12, a trampoline address, and a fail-stop return target. Stacks are 16-byte
aligned; the trampoline aligns the stack before calling C.

## Timer-preemption composition

The IDT interrupt stub saves all interrupted GPRs and the CPU return frame on
the interrupted thread's kernel stack. The timer's C handler then invokes the
same call-based switch while IF is clear. That call saves the handler's
callee-saved state and continuation on that thread's stack. When selected
again, it resumes the handler, restores the full IRQ frame and returns with
`iretq` to the interrupted instruction. This is tested only on one CPU.

The call switch itself does not save RFLAGS, an interrupt frame, SIMD/FPU
registers, or per-thread floating-point control state. The timer path's
separate interrupt stub owns interrupted GPR/RFLAGS preservation. FPU/SIMD
task state and SMP-safe switching are not implemented; do not infer user-mode
or multicore support from this ABI.
