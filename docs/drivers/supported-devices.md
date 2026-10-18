# Supported devices

## Implemented and tested

| Device/interface | State | Exact evidence |
|---|---|---|
| 16550-compatible COM1 at legacy ports 0x3F8–0x3FF | Driver implemented and exercised | QEMU 11.1.2, EDK2, `-machine q35`, serial output contains `16550-compatible COM1 driver online`; existing logger continues to report boot and scheduler events. |
| PCI configuration mechanism #1, bus 0 | Read-only discovery implemented and exercised | Same QEMU boot observed six functions and validated the expected device IDs listed in `pci-enumeration.md`. No PCI driver is bound to them. |

## Not supported

PCI bridge traversal beyond bus 0, storage/SATA, network, USB, audio, GPU
acceleration, display modesetting, ACPI/ECAM, MSI/MSI-X, DMA/IOMMU, hot-plug
and physical hardware are not implemented or tested as ORION drivers.

Detection is not driver binding. Driver binding is not proof that a device
class is fully supported. The table above is specific to the stated QEMU
configuration and is not a general PC compatibility claim.
