# Timer preemption

## Implemented path

The x86-64 IDT contains vector 32 as an interrupt gate using the existing
normalized no-error-code stub. The stub saves all general-purpose registers,
clears DF for the C ABI, aligns the handler stack, and returns with `iretq`.
The handler acknowledges legacy PIC IRQ0 before asking the scheduler to
rotate the ready queue.

Preemption reuses the Phase 9 call-based context switch safely: the complete
interrupted-register frame remains on the interrupted thread's own stack,
while the ordinary context-switch call saves the C interrupt-dispatch
continuation. When that thread is selected again, the continuation returns
through the IRQ handler, restores the saved register frame, and executes
`iretq` to the exact interrupted instruction. Maskable interrupts remain
disabled throughout queue mutation and the scheduler portion of the IRQ.

New threads enable IF only after the timer/PIC path is configured. Their
entry function disables IF before the non-returning scheduler exit path.
Scheduler APIs atomically save IF and disable interrupts around mutations,
then restore the caller's prior IF state. The bootstrap remains with IF clear
after the self-test.

## Verified scope

QEMU with one virtual CPU delivered PIT IRQ0 and recorded timer-driven
preemptions. Two CPU-bound workers each completed at least 1,000,000
iterations; neither called `yield`, and both preserved a volatile stack-local
marker. The serial verifier requires non-zero timer and preemption counts,
the iteration thresholds, and zero worker yields. This is direct evidence of
preemption in the tested QEMU/EDK2 configuration, not a hardware-wide claim.

## Safety limits

The path is single-CPU. The interrupt gate prevents nested maskable IRQs, and
local scheduler operations disable IF; this is not an SMP lock. The context
switch saves integer ABI state. No task uses FPU/SIMD state, and lazy/eager
FPU context management is not implemented. Interrupts not explicitly
supported remain masked or fail-stop. Long interrupt-disabled sections delay
the tick and are not bounded by a latency guarantee.
