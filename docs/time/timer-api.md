# Kernel timer API

`kernel/time/timer.*` provides a static pool of 32 timers. It supports absolute
deadlines (`orion_timer_arm_at`), relative deadlines
(`orion_timer_arm_after`), cancellation, one-shot callbacks and periodic
callbacks. Periodic timers use fixed-delay semantics: after a successful
callback, the next deadline is the callback-time clock reading plus the
period. Missed periods are not replayed.

Each timer handle contains a slot and generation. A cancelled or expired
handle cannot address a later timer that reuses the slot. Capacity exhaustion,
clock unavailability, invalid handles and cancellation while a callback is
already firing have explicit statuses. A firing callback cannot be cancelled;
the owner must keep its context alive until callback completion. A callback
returning `false` is reported by `orion_timer_run_expired`, counted, and
disarmed.

The pool is scanned linearly. At 32 entries this keeps allocation and
cancellation simple and bounded; expiration dispatch scans the pool for the
earliest due deadline. The current workload does not justify a heap.

Callbacks run only from `orion_timer_run_expired`, never from the PIT handler.
Dispatch requires interrupts enabled, holds no timer critical section while
calling user-supplied callback code, and expects callbacks to be bounded and
non-blocking. A poll is limited to at most 32 callbacks to bound work. The pool
and critical sections assume one CPU. They do not
provide SMP safety or a wait/join operation for a firing callback.

The API supplies a reusable deadline mechanism, not a blocking operation
timeout contract. Existing IPC is non-blocking and drivers have no generic
wait API, so neither is currently wired to timer cancellation or wakeup.
