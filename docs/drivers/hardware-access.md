# Hardware access

The initial backend uses only two forms of direct port I/O:

- PCI configuration mechanism #1 read cycles on CF8/CFC, with bus/device/
  function/offset validation.
- The fixed legacy COM1 UART register range 0x3F8–0x3FF used by the
  16550-compatible serial driver already responsible for kernel diagnostics.

There is no generic arbitrary-port API exposed to callers, no MMIO mapping
interface, and no validated BAR allocation. The PCI scanner reads BAR
configuration dwords but does not interpret them as trusted address ranges.
Therefore no unvalidated MMIO range is accessed. Driver authority stays
inside Ring 0 because no user mode or device-resource capability mediation
exists yet.
