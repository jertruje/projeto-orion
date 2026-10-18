# PCI enumeration

The x86-64 backend implements read-only PCI configuration mechanism #1 through
I/O ports CF8/CFC. It validates device/function ranges, dword alignment and
configuration offset bounds before issuing the address/read cycle. It scans
all 32 device numbers on one selected bus, honors the multifunction bit in
function zero, and reads vendor/device, revision, programming interface,
subclass, class, header type and the BAR dwords that the header type defines.

The kernel currently scans bus 0 only. Bridge traversal, ACPI table validation,
ECAM/MCFG, extended configuration space, BAR sizing/probing, resource
reservation, IRQ routing and PCI command-register writes are absent. BAR values
are recorded as untrusted metadata and are never dereferenced or enabled.

QEMU validation uses the existing `-machine q35` configuration and observed
six functions on bus 0:

| BDF | Vendor:device | QEMU function observed |
|---|---|---|
| 00:00.0 | 8086:29c0 | PCI host bridge |
| 00:01.0 | 1234:1111 | QEMU display adapter |
| 00:02.0 | 8086:10d3 | QEMU network adapter |
| 00:1f.0 | 8086:2918 | ISA bridge |
| 00:1f.2 | 8086:2922 | SATA controller |
| 00:1f.3 | 8086:2930 | SMBus controller |

Enumeration identifies functions only. It does not imply that any listed PCI
device has a driver or is operationally supported.
