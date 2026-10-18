# Driver testing

`mingw32-make test-unit` runs host tests for registration validation,
duplicate names/devices, unmatched devices, successful/failed startup,
cleanup after partial failure, stop lifecycle, invariant checks, PCI
multifunction enumeration, class/revision/BAR metadata and invalid scan
arguments. A failed driver start is also checked against the Phase 13 fault
record. PCI configuration reads are injectable in these host tests; this does
not emulate hardware port I/O.

`mingw32-make test-boot` boots QEMU 11.1.2 with EDK2 using `-machine q35`,
`-m 256M`, `-smp 1`, an FAT-backed ESP and a serial log. The guest runs the
real legacy PCI port-I/O scanner on bus 0, registers and starts the COM1
driver, emits the UART online marker, checks six discovered PCI functions,
reports zero inventory rejections, and continues through the existing
scheduler/IPC tests to idle.

No test covers BAR sizing, MMIO, hardware IRQ routing, DMA, hot-plug,
multi-bus traversal, user-mode drivers, physical hardware or fault isolation
of a privileged driver.
