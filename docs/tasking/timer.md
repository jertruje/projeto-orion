# Timer and interrupt controller

## Source and setup

The first timer is the legacy 8254-compatible PIT channel 0 routed through the
8259-compatible master PIC. The kernel remaps the PIC master/slave vectors to
0x20/0x28, masks every IRQ during setup, programs PIT channel 0 in rate
generator mode, and unmasks only master IRQ0. Vector 32 is present in the
IDT before any IRQ is enabled. The timer handler sends the master PIC EOI
before scheduling so that a suspended handler cannot leave IRQ0 in service.

PIT input is nominally 1,193,182 Hz and the configured divisor is 11,932:

`1,193,182 / 11,932 = approximately 99.9985 Hz`

The resulting nominal interval is 10.00015 ms. No claim is made that this is
the measured physical frequency; QEMU/EDK2 is the only tested platform. The
PIT is periodic, so each delivered interrupt is one scheduler quantum.

## Failure behavior and ownership

Initialization fails closed if the scheduler is not initialized, IF is
already set, the timer was already initialized, or preemption cannot be
enabled. The controller is masked before leaving the timer path. Runtime
invariant/overflow failures enter the kernel panic path. Stopping the timer
requires IF clear, masks master and slave PIC lines, and disables scheduler
preemption. The timer is initialized once per boot and is not restarted.

Only IRQ0 is enabled. This code does not initialize the local APIC, IOAPIC,
HPET, TSC-deadline timer, or APIC timer. IRQ delivery and PIT frequency on
physical systems remain unverified.
