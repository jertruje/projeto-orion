# Memory concurrency and safety

The allocator and page-table manager are currently single-CPU components.
Kernel entry keeps IF clear; no APs or concurrent callers exist. They contain
no locks and must not be described as SMP-safe.

The physical allocator owns its free/allocated bitmap state. The page-table
manager does not own mapped leaf frames. It may release only empty intermediate
tables acknowledged by its ownership callback. Callers must keep any mapped
frame alive until after unmapping and required TLB invalidation. There is no
cross-CPU shootdown, pinned/DMA frame tracking, or concurrent map/unmap
protection.

All range arithmetic is checked before use; unknown map types remain reserved.
Invalid frees, double frees, unaligned addresses, noncanonical addresses,
unknown flags, W+X, duplicate mappings, and out-of-memory table allocation
fail without claiming success. These controls do not constitute a general
kernel memory-safety or isolation guarantee.
