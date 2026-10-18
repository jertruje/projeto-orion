# Tasking concurrency and invariants

The scheduler and ready queue have one CPU owner. Public scheduler mutations
save IF and disable maskable interrupts around state/queue changes. IRQ0 is
delivered through an interrupt gate, acknowledged before scheduling, and is
not nested while its handler is running. No spinlock is present or needed for
this single-CPU execution model.

A thread is present at most once in the ready queue; the selected thread is
the only `RUNNING` thread; terminated threads are never requeued. The timer
may suspend ordinary CPU-bound code but not scheduler mutations, which are
performed with IF clear. The handler does not log, allocate, or block.

This discipline does not protect against NMIs, DMA, or a second CPU. AP boot,
per-CPU ownership, atomic/spinlock primitives, memory ordering, TLB shootdown
and multi-vCPU tests are absent. Do not boot or schedule additional CPUs until
those dependencies are implemented and verified.
