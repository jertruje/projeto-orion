# Driver and service model

## Placement and authority

Default to one driver process per failure domain, with typed/versioned IPC to
the device manager and clients. The kernel performs only unavoidable resource
mediation: map assigned MMIO ranges, route an assigned IRQ, pin DMA buffers and
apply IOMMU constraints when supported. Device policy, protocol parsing,
retries and class logic remain outside the kernel when isolation can be
preserved.

An explicit trust exception is required for any in-kernel driver: document why
user-mode placement is infeasible, device/authority scope, rollback plan, test
and failure impact. Early bootstrap console/serial can be a narrowly scoped
temporary mechanism, not a general driver architecture.

## Lifecycle

`DISCOVERED -> MATCHED -> AUTHORIZED -> STARTING -> ONLINE -> QUIESCING -> STOPPED`
with `FAILED` transitions from any active stage. Match on stable hardware
identity and supported revision, then validate BAR ranges, IRQ/MSI/MSI-X
capability and DMA requirements. Never enable bus mastering before DMA mappings
and buffers are ready. All grants are revocable; teardown masks interrupts,
quiesces device, unmaps DMA/MMIO and only then reclaims memory.

Timeout is a deadline on the caller; it does not mean the device stopped DMA.
Reset and quiescence require device-specific confirmation. If safe quiescence
cannot be established, retain the memory and require platform reset/reboot
rather than free memory still reachable by hardware.

## Services and dependencies

- Root bootstrap/supervisor starts the device manager.
- Device manager performs enumeration and starts only authorized drivers.
- Log/diagnostic collection is optional and cannot be a prerequisite to panic.
- Storage service depends on a block driver; network service depends on a
  network device driver and timer.
- No driver may require the filesystem that depends on that same driver during
  bootstrap; boot bundle resolves the cycle.

## Compatibility declaration

Conceptual support = protocol described. Implemented = code exists and builds.
Tested = exact device model/revision, QEMU machine or physical platform,
firmware, configuration and test results recorded. Release support additionally
requires repeated regression and failure tests. Enumeration alone is not
support. The current project has no kernel drivers.
