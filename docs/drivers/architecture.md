# Driver framework architecture

## Implemented

`kernel/drivers/core/` provides a bounded registry for up to eight drivers and
32 discovered devices. A driver supplies a bounded name, match callback,
start callback and cleanup callback. Devices carry a bus identity and
PCI/platform metadata. Exclusive binding uses the first registered matching
driver; an unmatched device remains discovered, a failed start becomes FAILED,
and a successful start becomes ONLINE. Stop calls cleanup once and transitions
ONLINE to STOPPED. Failed startup invokes cleanup for partial resources and
records a driver failure using the Phase 13 fault ring.

The callbacks are not invoked while maskable interrupts are disabled. State
updates use save/disable/restore IF. This is single-core exclusion, not SMP
locking. Driver descriptors and platform-name strings are borrowed and must
remain valid for the lifetime of the registry.

The first concrete platform driver is a 16550-compatible COM1 UART, used by
the kernel's existing bounded serial logger. PCI configuration reads use
legacy x86 mechanism #1 and enumeration is currently limited to bus 0. No
PCI device is yet bound to an operational PCI driver.

## Deliberately absent

There is no user-mode driver process, resource capability transfer, generic
MMIO mapper, IRQ handler registration, hot-plug, DMA, IOMMU, ACPI/MCFG ECAM
support, bus-bridge recursion, or device removal. In-kernel drivers execute
with kernel privilege; a defective driver can compromise the whole kernel.
