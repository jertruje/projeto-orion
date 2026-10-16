# Memory model — Phase 8

## Implemented limits

- Base page size: 4 KiB.
- Virtual page-table format: four-level x86-64 (PML4, PDPT, PD, PT), 48-bit
  canonical virtual addresses. 1 GiB and 2 MiB leaf pages are rejected.
- Physical allocator scope: `[0, 4 GiB)`. The cap keeps metadata fixed-size and
  avoids trusting unchecked firmware address-width claims. Memory above the
  cap is not allocated.
- Existing UEFI page tables, CR3, paging mode, and kernel virtual layout are
  inherited unchanged. The kernel does not switch CR3.
- All UEFI descriptors other than `EfiConventionalMemory` are reserved. This
  conservatively retains kernel/loader images, loader stack and boot map,
  firmware/runtime regions, and existing page-table storage.
- A single logical CPU runs with IF clear. The allocators are not SMP-safe.

The page-table manager uses typed virtual/physical address wrappers and supplied
callbacks for table allocation, physical-to-virtual resolution, ownership,
release, and TLB invalidation. It is host-unit-tested but is not yet attached
to the inherited live CR3. A caller must provide a valid address-space root
and a tested mapping for page-table frames before using it in the kernel.

## Permissions

Read is inherent in x86 page-table entries and must be requested in the API.
Write, user, execute, write-through, and cache-disable are explicit flags.
Writable+executable mappings are rejected. Non-executable mappings require
NX support and EFER.NXE to be enabled; `orion_x86_64_nx_enabled()` reports
both CPU support and the current EFER state, but does not enable NXE.
User-marked PTEs are not evidence of user/kernel isolation: ORION has no ring-3
processes or controlled address spaces yet.

## Failure behavior

Invalid maps, out-of-range addresses, unsupported flags, duplicate mappings,
missing callbacks, or allocation failure return `false`. Failed map operations
roll back intermediate tables allocated by that operation. A map/unmap
operation requires a TLB invalidation callback. Page faults are diagnosed and
remain fatal; demand paging and recovery are not implemented.
