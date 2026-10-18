# Kernel scheduler

The scheduler uses one bounded FIFO ready queue on the single boot CPU.
Explicit yield and the PIT timer both rotate a running thread to the queue
tail and select the head. New tasks enter at the tail. A timer interrupt with
no other ready thread increments accounting but does not switch. Terminated
threads are never requeued; destruction occurs on another stack.

Scheduler APIs protect state and queue mutations by atomically saving IF and
disabling maskable interrupts. The timer handler arrives through an interrupt
gate, acknowledges IRQ0 before scheduling, and calls the same queue transition
with IF clear. After a suspended context-switch call resumes, current-thread,
state, queue and stack invariants are checked.

The kernel has no general wait/wakeup, idle-thread abstraction, priority
levels, user mode, APs, CPU affinity, or migration. At the end of the bounded
boot self-tests, the PIT is masked and the bootstrap thread enters the
existing `cli; hlt` idle loop.

`orion_scheduler_stats` reports context switches, explicit yields, timer
interrupts, timer preemptions, ready-thread count and managed-thread count.
Each thread accumulates runtime in delivered timer ticks. Tick resolution and
measurement limitations are in `scheduler-metrics.md`.
