# Monotonic clocksource

The current clocksource is the count of PIT channel 0 interrupts delivered
through PIC IRQ0. It is intentionally separate from the timer queue: the PIT
interrupt advances a counter, while callbacks are dispatched later by an
ordinary kernel execution context.

The PIT input frequency is treated as 1,193,182 Hz and the programmed divisor
is 11,932. Each delivered IRQ therefore represents
`11,932 / 1,193,182` seconds (nominally 10,000,150 ns per tick after integer
conversion). This is a coarse tick clock, not a high-resolution counter.
`orion_clocksource_now_nanoseconds` converts the cumulative tick count with
checked 64-bit arithmetic and clamps a backwards source observation to the
last returned value.

The clock is unavailable until the PIT is initialized and its timekeeping mode
is started. It does not read TSC, RTC, HPET, APIC or ACPI data. The conversion
uses the documented PIT constants; it does not calibrate the oscillator
against wall time. The single-core IRQ-safe sections are not SMP
synchronization.

Nanoseconds are the internal unit. A `uint64_t` result has a finite range;
conversion or counter overflow is reported as failure rather than wrapping.
There is no civil clock, timezone, calendar conversion or external time
synchronization. PIT ticks provide a monotonic interval reference only while
IRQ0 continues to be delivered.
