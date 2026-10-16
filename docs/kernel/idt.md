# Interrupt Descriptor Table — Phase 7

The kernel creates a 256-entry, 16-byte-aligned IDT. CPU exception vectors
0–31 each have an explicit 64-bit interrupt gate targeting the kernel code
selector. Vector 8 selects IST1; all other installed exceptions use the
current kernel stack. The IDTR limit is exactly the byte size of the table
minus one. `sidt` readback checks base and limit after `lidt`.

Vectors 32–255 are left not-present. IF remains clear, so this is not an
external-interrupt setup; any future PIC/APIC enablement requires its own
vector allocation, acknowledgement and spurious-interrupt policy.

The initialization routine verifies selector, gate attributes, handler
address presence, and IST selection for all installed CPU exception gates.
