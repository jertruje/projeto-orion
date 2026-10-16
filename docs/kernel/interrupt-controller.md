# Interrupt controller boundary — Phase 7

This phase configures CPU exception delivery only. It does not remap, mask,
acknowledge, or otherwise reprogram the legacy PIC; it does not initialize an
APIC or route device interrupts. The fact that an IDT exists is not evidence
that an external interrupt controller is operational.

IF remains clear throughout the normal kernel path. IDT vectors 32–255 are
not-present, so enabling IRQs without a later controller/vector phase would be
unsafe. PIC/APIC selection, vector allocation, EOI and spurious IRQ handling
remain open work for the timer/device phases.
