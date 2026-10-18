# ACPI support status

ACPI is not parsed or consumed by the kernel. The UEFI loader's version 1
`orion_boot_info` contract carries only the final memory-map pointer, its size
and descriptor size. It does not pass the UEFI configuration table list or
an ACPI RSDP. The kernel does not discover or dereference firmware-provided
ACPI addresses.

The QEMU/OVMF machine configuration may expose ACPI to firmware, but ORION
does not currently inspect that data. Consequently this implementation makes
no claim that ACPI tables are absent or present, and has no RSDP, RSDT/XSDT,
FADT, MADT or HPET validation results. No ACPI checksum or firmware address
is trusted.

Before ACPI support can be added, the boot contract must provide a bounded,
validated route to the RSDP or a safe ACPI discovery mechanism. Any parser
must validate signature, revision, lengths, checksums, address ranges and
mapping lifetime before reading referenced tables. ACPI remains optional and
failure to discover/validate it must not prevent basic boot.
