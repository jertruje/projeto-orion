# Scheduling policy

## Implemented policy

The first preemptive policy is single-CPU FIFO round-robin. The ready queue
remains bounded by the static thread pool. A runnable thread is appended at
the tail; a timer tick ends the current quantum and rotates it behind the
other ready threads. A yield uses the same FIFO rotation. Newly created
threads enter at the tail. A terminated thread is never queued again and its
stack is reclaimed only after execution has moved to another stack.

The PIT channel-0 period is the quantum: divisor 11,932 from the nominal
1,193,182-Hz input clock gives approximately 99.9985 ticks/s, or 10.00015 ms
per tick. This is a configured nominal rate, not a hard real-time guarantee.
The initial implementation has no priorities, general blocking/wakeup,
deadline scheduling, migration, or user-controlled policy.

If no other thread is ready, the interrupted thread continues without a
context switch. When the ready queue is empty, the existing bootstrap thread
returns to its idle path after its work completes; the timer is masked after
the bounded boot self-tests.

## Accounting and policy limits

The scheduler counts context switches, explicit yield calls, timer interrupts,
timer-driven preemptions, and per-thread runtime timer ticks. Timer ticks are
the only current time unit. Queue wait time, sub-tick runtime, wake-to-run
latency, and CPU idle duration are not yet instrumented. Round-robin does not
imply a precise execution-time share because interrupt-disabled sections and
timer delivery add jitter.

## Future policy alternatives

| Choice | Initial trade-off |
|---|---|
| One shared queue | Simplest to reason about on one CPU; needs SMP-safe ownership before multiple writers exist. |
| Per-CPU queues | Local ownership can reduce contention, but requires AP startup, affinity and migration rules. |
| Migration | Can improve balance but introduces ownership races and cache/TLB costs; not implemented. |
| Priorities | Deferred until inversion, starvation and admission policies have measurable requirements. |

The current implementation selects only the single-CPU FIFO queue. A later
change must provide workload measurements before changing the quantum or
adding scheduling classes.
