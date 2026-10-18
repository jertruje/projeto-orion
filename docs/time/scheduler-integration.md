# Timer and scheduler integration

The PIT IRQ0 path retains the Phase 10 single-core preemption behavior.
Preemption testing masks the PIT after its workers finish; Phase 15 then
restarts IRQ0 in timekeeping-only mode. In that mode the handler advances the
clock counter and acknowledges the PIC without invoking the scheduler.

The kernel idle loop dispatches due timer callbacks in thread context and
executes `hlt` with interrupts enabled between polls. QEMU boot runs a real
one-shot timer against PIT interrupts before entering idle.

The scheduler has no general blocking/wakeup queue, sleep-until operation or
thread teardown hook for timers. Therefore this phase does not implement
timed thread sleep, blocking IPC timeouts, or device waits. A callback queue
is not proof of scheduler wakeup semantics. Such integration requires an
explicit blocked-thread state transition, race-safe wake/cancel rules, and
tests for thread exit and timeout contention.
