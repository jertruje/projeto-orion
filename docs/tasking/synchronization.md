# Scheduler synchronization

## Current single-CPU rules

There is no spinlock: only the bootstrap CPU may access scheduler state.
Public scheduler mutations use an atomic save-IF-and-CLI operation, perform
the queue/state transition with maskable interrupts disabled, then restore
the caller's original IF state. This prevents IRQ0 from observing a partially
updated queue or interrupting code that holds the scheduler's implicit
single-CPU critical section.

The interrupt gate clears IF on entry. The timer handler acknowledges IRQ0
before it may switch away; no nested maskable timer interrupt can acquire the
scheduler while the previous invocation is suspended. No allocation,
logging, or blocking operation occurs in the timer handler.

## Ownership and lock order

The current ownership invariant is one running thread on the single bootstrap
CPU, with every other runnable thread present at most once in the FIFO queue.
Thread state and queue links are updated only with IF disabled. Termination
marks the current thread and switches stacks before cleanup. There are no
cross-CPU lock orders because APs do not start.

This IF discipline is not sufficient for SMP: local interrupt masking cannot
exclude another CPU. AP startup, per-CPU state, atomics/spinlocks, memory
ordering, interrupt routing and race tests are prerequisites before enabling
more than one CPU.
