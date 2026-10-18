# Thread model — Phase 9

`kernel/task/thread.h` defines a thread as saved CPU stack state, an independent
kernel stack, entry function/argument, process link, lifecycle state, and
dispatch/yield/exit counters. The boot execution context is registered as
thread 1 and keeps the 64-KiB entry stack. The fixed pool contains eight
control blocks including that bootstrap thread, so at most seven additional
kernel threads can be created during one boot. Each thread also has a runtime
tick counter; it is not a high-resolution CPU-time clock.

New threads have independent 16-KiB stacks and begin `READY`. Their first
dispatch enters an x86-64 trampoline, which passes the control block to the
C start routine and calls the entry function with its supplied argument.
Returning from the entry function terminates with exit code 0. Exceptions are
not recovered as thread failures; the kernel's existing fatal exception path
remains in effect.

New threads keep IF clear unless the scheduler has successfully enabled its
PIT preemption path. In that bounded path, the trampoline enables maskable
interrupts only after timer/controller initialization; it clears IF before
termination and deferred stack cleanup.

All threads belong to the single kernel process. They share privileged memory
and must not be described as isolated processes or user tasks.
