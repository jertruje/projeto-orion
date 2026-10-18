# Fault subsystem testing

## Host unit tests

`mingw32-make test-unit` runs `tests/kernel/fault/fault_test.c`, which checks
record field preservation, assigned identifiers, invalid arguments and enum
values, retrieval of present/missing entries, bounded capacity, overwrite
count, and restoration of the interrupt-state stub. Panic host tests verify
that a panic records `KERNEL_PANIC` with a halt action and no claimed recovery,
including recursive and logger-failure cases.

## QEMU integration tests

`mingw32-make test-exceptions-qemu` runs controlled #DE, #UD, #GP, #DF, #PF,
read-only and NX fault images. The serial assertions check that each CPU
exception produces a structured event ID before the fatal exception report.
`mingw32-make test-panic-qemu` confirms that #BP resumes and the subsequent
explicit panic is reached. These are disposable QEMU guests.

## Limits

The tests do not demonstrate user-process isolation, application fault
containment, service restart, IPC process teardown, concurrent SMP access,
long-term reliability, durable diagnostics, or successful recovery from a
kernel exception. Panic is expected to halt and is not a successful recovery.
