# CPU affinity status

The current scheduler has one runnable CPU: the BSP/boot CPU. Threads have no
affinity mask or CPU identifier, and no migration or cross-CPU wakeup exists.
This is not an implicit guarantee that a thread is SMP-safe.

Before SMP, define CPU identity from firmware topology and maintain per-CPU
current-thread, interrupt-stack, idle-thread, scheduler and timer state. The
first multicore experiment should pin each thread to one CPU and disable
migration. An affinity API should be added only when at least two CPUs are
initialized and there are tests for unavailable CPUs, empty masks, CPU
shutdown/failure and ownership transfer. Until then, no public affinity
contract is exposed.
