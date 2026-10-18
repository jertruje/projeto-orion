# Time and power failure handling

Clocksource initialization fails if the PIT is not active or its first
counter read/conversion is invalid. A failed read, invalid conversion or
overflow is returned to the caller; values are not fabricated. The monotonic
clock clamps a backwards counter observation and reports the clamp in its
diagnostics. The architecture PIT handler fails stop on an inactive or
overflowed tick counter.

Timer capacity exhaustion, invalid/stale handles, clock unavailability and
cancel-during-callback are explicit API results. A callback failure is counted,
the timer is disarmed and dispatch returns failure. Periodic deadline overflow
also disarms the timer and fails dispatch. The static pool avoids allocation
failure and offers cancellation to release an armed slot.

There is no supported power transition to coordinate with active timers.
Shutdown, reset, suspend and hibernation must not be simulated by writes to
undocumented registers. Timer service teardown and SMP cancellation are not
implemented.
