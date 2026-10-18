# Virtual-address layout

The current kernel retains the loader's inherited mappings and
identity-addressed ELF layout. The kernel ELF entry is linked at `0x200000`;
the loader limits PT_LOAD addresses to below `0x4000000`. The current 64 KiB
kernel stack and the allocator bitmaps reside in kernel BSS. No guard page,
high-half kernel mapping, user range, temporary mapping window, or MMIO
virtual region has been established.

The bootstrap adapter clones the inherited PML4 and switches CR3 to the clone
for a temporary map/unmap smoke test. Existing mappings, the return address,
active stack, and executing code are preserved. The temporary high-half test
mapping is removed; no permanent high-half layout or process address space is
established. The adapter's identity access to static page-table storage has
only been validated in QEMU/EDK2. A future layout change must inventory all
active ranges and test a complete new root before switching.
