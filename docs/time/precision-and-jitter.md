# Time precision and timer latency

The clocksource resolution is quantized by the PIT interrupt period. With
input frequency 1,193,182 Hz and divisor 11,932, the code reports a nominal
integer quantum of 10,000,150 ns. This is a conversion from hardware constants,
not a measured accuracy guarantee.

The QEMU integration test uses the configured EDK2/OVMF boot, `q35`, one vCPU
and 256 MiB. It schedules a one-shot 30 ms deadline, waits for real PIT IRQ0
delivery, dispatches the callback outside interrupt context, records lateness
as `dispatch clock reading - requested deadline`, and requires that sample to
be no greater than 200 ms. The test also checks that PIT ticks continue after
the scheduler-preemption test has stopped.

In the recorded run for this change, the nominal quantum was 10,000,150 ns.
One QEMU callback sample had 453 ns lateness; the logged PIT IRQ count advanced
from 7 at the end of the preemption test to 10 during the timekeeping test.
This single virtualized sample is not a jitter distribution, oscillator
calibration, wall-clock accuracy measurement, real-time guarantee, or
hardware result. No loaded percentile, long-duration drift or physical-device
measurement was performed. The PIT tick quantum remains the limiting
resolution regardless of the small observed sample.

The timer API exposes active/peak timer counts, callback failures, scan count,
expirations, cancellations and maximum observed lateness. In this phase,
only the one-shot QEMU sample is gathered; a statistical benchmark remains
future work.
