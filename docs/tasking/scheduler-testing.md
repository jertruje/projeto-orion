# Scheduler testing

## Host coverage

`mingw32-make test-unit` checks bounded thread creation, state transitions,
stack bounds/canaries, queue invariants, and scheduler counters with
interrupt-state functions stubbed to IF-clear behavior.

## QEMU coverage

`mingw32-make test-preemption-qemu` boots the normal UEFI image with
`-smp 1`. The serial verifier checks the existing cooperative alternation,
then requires non-zero PIT events and timer-driven preemptions, at least
1,000,000 loop iterations from each CPU-bound worker, zero yields by those
workers, intact termination/stack checks, no-switch handler samples, and a
non-zero TSC handoff measurement. Workers spin until the other has progressed;
without automatic preemption the boot cannot reach the success marker.

The test additionally waits through four timer events with no runnable peer,
exercising the no-reschedule IRQ path, and then measures 1,000 cooperative
ping-pong rounds with the timer masked. Existing `test-boot`,
`test-panic-qemu`, and `test-exceptions-qemu` continue to validate the prior
boot and failure paths.

This is a bounded functional regression test, not a long-duration stability
campaign, physical-hardware validation, latency guarantee, formal proof, or
multicore test. `-smp 2` is intentionally not used because AP initialization
and cross-CPU scheduler synchronization are absent.
