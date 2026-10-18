# Memory and concurrency architecture

## Memory ownership

The kernel owns physical frame accounting and page-table mutation. Each
process owns a virtual address space identified by a kernel object. A frame is
not reusable while mapped, pinned, referenced by a boot structure, executing
code/stack, held by DMA, or visible through shared memory. All ranges use
checked half-open intervals `[base, end)` with overflow rejection.

### Implemented bring-up slice and remaining progression

1. **Implemented, limited:** validate the final UEFI memory-map descriptors
   and reserve all non-conventional memory; the allocator exposes conventional
   pages only below 4 GiB.
2. **Implemented, limited:** two static bitmaps track free and allocated
   frames; allocation is single-CPU first-fit.
3. **Implemented and tested, limited:** page-table operations use four-level
   4-KiB tables, typed addresses, canonicality checks, W^X, ownership and TLB
   callbacks. A QEMU/EDK2 adapter clones the inherited PML4 and validates
   temporary live mappings and RO/NX faults. Its static tables are accessed
   through equal numeric physical/virtual addresses; portability is unproven.
4. Replace or prove that table-frame mapping assumption for other firmware,
   keep NX CPU support/state checks explicit (the kernel does not enable NXE),
   and build independent address spaces before mapping user addresses.
5. Establish the full virtual layout and address spaces before mapping user
   addresses; add TLB shootdown only after APIC and per-CPU state work.
6. Add shared pages only through capability-scoped mappings with lifetime and
   revocation rules.

**Selected bring-up baseline:** a reserved-by-default, fixed 4-GiB bitmap
allocator with statically allocated metadata in kernel BSS. The kernel image
is classified as UEFI loader memory, so this BSS is not marked free. This is
selected because page ownership and invalid-free checks are straightforward
to inspect; it is not claimed to be faster.
A free-list/buddy allocator may better support contiguous allocations but adds
split/coalesce invariants. Reconsider only if memory-map scale, fragmentation
tests or measured contiguous-allocation needs justify it (ADR-0013). The
address-width bound and bitmap size must be derived from validated platform
data; never size it with unchecked firmware values.

## DMA and device memory

MMIO mappings have distinct cacheability and permission attributes and cannot
be passed as ordinary RAM. DMA buffers are pinned and tracked. IOMMU mappings
must be created before bus mastering and removed only after device quiescence.
If IOMMU unavailable, document driver/device as trusted or refuse bus mastering
for untrusted device classes; do not claim equivalent isolation.

## Concurrency policy

- Bring-up begins single-CPU with interrupts disabled until IDT/controller and
  all required handlers are validated.
- Introduce lock ordering and interrupt-context rules before enabling IRQs.
  Never block, allocate without a bounded path, or take a potentially
  contended sleeping lock in interrupt context.
- Shared kernel state has a documented owner/lock; lock-free atomics require
  memory-ordering rationale and architecture tests.
- SMP comes after stable single-core preemption. Per-CPU run queues reduce
  shared contention but need migration/IPI balancing; a global queue is simpler
  but contended. Decide using measured workload and correctness tests.
- Deadlock prevention: define lock rank, avoid callbacks while holding internal
  locks, cancellation cleanup, and watchdog diagnostics. Starvation protection
  must be measured; no real-time guarantee is made.

## Invariants and tests

- No frame is simultaneously free and reserved/allocated.
- Every mapped frame has a live owner/reference; no freed frame remains mapped.
- User mapping has U/S set only for allowed pages; kernel mappings remain
  supervisor-only; writable executable mapping is rejected by policy.
- Overflows, noncanonical addresses, unaligned ranges and unknown flags fail
  without partial mutation.
- Out-of-memory leaves page tables and allocator state consistent.
- Concurrent map/unmap/free cannot double-release or expose stale mappings.
- Lock ordering and IRQ-safe paths are checked in stress/fault-injection tests.

The current kernel implements the limited physical allocator and a standalone
page-table manager described above. User address spaces, live CR3 replacement,
page reclamation while mapped, DMA ownership, and concurrent operation remain
unimplemented.
