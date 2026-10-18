# Interrupt Descriptor Table — Phase 7

The kernel creates a 256-entry, 16-byte-aligned IDT. CPU exception vectors
0–31 each have an explicit 64-bit interrupt gate targeting the kernel code
selector. Vector 8 selects IST1; all other installed exceptions use the
current kernel stack. The IDTR limit is exactly the byte size of the table
minus one. `sidt` readback checks base and limit after `lidt`.

Vector 32 is installed for the normalized PIT IRQ0 stub and verified alongside
the exception gates. Other vectors 33–255 are left not-present. The bounded
Phase 10 timer test remaps/masks the legacy PIC, unmasks only IRQ0, and
acknowledges it before scheduler dispatch. This is not general device IRQ or
APIC/IOAPIC setup; those paths require vector allocation, acknowledgement and
spurious-interrupt policy.

The initialization routine verifies selector, gate attributes, handler
address presence, and IST selection for all installed CPU exception gates.
