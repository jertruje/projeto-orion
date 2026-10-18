# Scheduler metrics and measurement

## Implemented counters

| Metric | Definition | Unit / resolution | Source |
|---|---|---|---|
| Context switches | Successful scheduler dispatches, including initial and exit dispatch | count | Scheduler |
| Timer interrupts | Accepted PIT IRQ0 events | count; nominal 10.00015-ms tick | Scheduler and timer driver |
| Timer preemptions | Timer events that rotate to another ready thread | count | Scheduler |
| Runtime per thread | Timer ticks observed while that thread is current | tick; 10.00015-ms nominal resolution | Thread control block |
| No-switch handler samples | PIT handler samples when no scheduling switch occurred | count | Timer driver |
| Handler TSC total/max | TSC delta in the C timer-dispatch/scheduler portion for no-switch ticks | virtual TSC cycles; frequency not calibrated | Timer driver |
| Handoff TSC average | Total TSC delta across a bounded two-thread cooperative ping-pong divided by scheduler dispatch count | virtual TSC cycles per dispatch, includes queue/invariant and test overhead | QEMU boot self-test |

The QEMU test logs exact counters and rejects zero ticks, preemptions, samples,
or handoff measurements. Two workers must each complete at least one million
iterations and report zero cooperative yields.

## Reproduction and limitations

Run `mingw32-make test-preemption-qemu`. The guest uses one QEMU vCPU and the
configured PIT period. The test waits for four additional no-switch timer
events, samples their C handler cost with serialized `lfence; rdtsc`, then
performs 1,000 ping-pong rounds with the PIT masked. The observed TSC values
are environment-specific virtual-cycle observations, not nanoseconds,
physical CPU cycles, confidence intervals, or comparisons with Linux.

The handler sample excludes IDT assembly entry/restore/`iretq`; it measures
the timer handler's C section through scheduler dispatch and includes its
instrumentation overhead. The handoff average is end-to-end scheduler
dispatch work, not the isolated assembly instruction cost. No benchmark
claim is made from one QEMU run. Future performance work should repeat runs,
report distributions and guest/toolchain/host settings, and measure the full
interrupt path separately.
