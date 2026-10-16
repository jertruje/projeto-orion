# Physical page allocator

The early allocator is a reserved-by-default bitmap covering 4 GiB:
1,048,576 pages at 4 KiB each. One availability bitmap and one allocation
bitmap consume 128 KiB each (256 KiB total), statically reserved in kernel
BSS. A set availability bit means a page is free; a set allocation bit means
the allocator handed that page out. Initially every page is reserved; only
validated UEFI conventional-memory pages below 4 GiB are made available.

Allocation is first-fit with a power-of-two page alignment. It returns a
physical address or `false` without modifying state on invalid alignment or
exhaustion. Release accepts only aligned pages currently marked allocated;
reserved pages, unaligned addresses, out-of-range addresses, and double frees
are rejected. Statistics and a full bitmap invariant check are exposed.

The allocator is single-CPU, with interrupts left disabled. It has no lock,
contiguous multi-page allocation, frame references, pinning, DMA ownership,
or reclamation of non-conventional UEFI memory. The address cap is a deliberate
bring-up restriction, not a detected physical-address width.
