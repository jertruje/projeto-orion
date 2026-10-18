# IPC synchronization

The current system is single-core. Each queue mutation, endpoint state change,
and invariant inspection saves/disables and restores interrupt state. Copies
are bounded by the 256-byte payload maximum and happen inside the short
critical section, making enqueue/dequeue atomic against the current PIT
preemption on one CPU. Capability acquisition creates a lease before the
channel is accessed; lease release occurs after leaving the queue critical
section.

The implementation has no mutex, semaphore, event, wait queue, or
block/wakeup primitive. Therefore send and receive are non-blocking and never
sleep, spin waiting for a peer, or block in an interrupt handler. Empty/full
conditions are returned to the caller.

Saving IF is not SMP synchronization. Do not enable multiple CPUs for this
subsystem until it uses an SMP-safe lock/atomic protocol with a documented
ordering and interrupt-context policy. Lock ordering must account for the
capability manager; callbacks/destructors run outside its internal critical
section. Priority inversion is not currently applicable because no IPC lock
can be held while blocking.
