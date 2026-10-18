# Interrupt controller boundary — Phases 7 and 10

The CPU exception path is installed for vectors 0–31. Phase 10 adds only the
legacy PIT/PIC path needed for single-CPU scheduling: the master/slave 8259
vectors are remapped to 0x20/0x28, all lines are masked during setup, PIT
channel-0 IRQ0 is routed to IDT vector 32, and the master PIC receives EOI
before the scheduler can switch contexts. Only IRQ0 is unmasked; unsupported
external vectors fail-stop.

The timer is enabled for the bounded preemption test only and is masked again
before the idle loop. QEMU/EDK2 with one vCPU is the only tested configuration.
No APIC/IOAPIC routing, spurious IRQ policy, device IRQ support, or physical
hardware compatibility is claimed.
