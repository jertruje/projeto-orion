# Time and power testing

Run:

```text
mingw32-make test-unit build inspect test-boot
```

Host tests use an injected counter to verify unavailable-source behavior,
checked conversion, monotonic clamping, one-shot expiration, periodic
fixed-delay behavior, cancellation, stale handles, capacity exhaustion,
callback failure, firing cancellation and resource cleanup. These host tests
do not emulate port I/O or prove that a PIT fires.

The QEMU boot test uses the real emulated PIT/PIC IRQ0 path. It checks
monotonic reads, waits for a 30 ms one-shot timer, validates numeric timing
metrics against the documented 200 ms maximum-lateness test threshold, and
confirms that PIT ticks advance after the scheduler-preemption run. This
establishes operation in the configured QEMU/OVMF machine only.

ACPI, power buttons, ACPI state transitions, suspend/resume, shutdown and
restart have no implementation and therefore no passing hardware tests. The
boot-info contract currently passes no ACPI table pointer; the kernel does not
read firmware tables.
