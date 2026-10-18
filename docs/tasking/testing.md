# Tasking tests

Run:

```text
mingw32-make test-unit
mingw32-make test-tasking-qemu
```

The host unit test checks bootstrap registration, stack bounds and canaries,
fixed-pool exhaustion, invalid entry rejection, invalid state transitions,
attempts to destroy a running/ready thread, and scheduler/process invariants.
It deliberately does not claim to emulate a hardware context switch.

The QEMU/EDK2 test runs two kernel threads on separate stacks. Each records
its ID and iteration, preserves a volatile stack-local marker across four
cooperative yields, verifies its argument, returns, and is reaped by the
bootstrap thread only after its stack is inactive. The test script parses the
serial trace and requires eight ordered events `2,3,2,3,2,3,2,3`, with
iterations `0..3` for each. It also checks terminal states, exit codes,
dispatch/yield counts, empty-queue yield behavior, and final invariants.

The Phase 9 test observes QEMU for 15 seconds and then stops it; this is not a
prolonged stability or performance test. Timer preemption is tested separately
by `scheduler-testing.md`. Neither test covers user processes, multicore
execution, or security isolation.
