# Virtual-memory manager

`kernel/mm/page_tables.*` implements 4 KiB map, lookup, and unmap against a
caller-supplied four-level page-table root. It validates 48-bit canonicality,
alignment, a 4 GiB physical-address cap, supported flags, and W^X. Large-page
entries are rejected. Page-table frames are created/resolved/released through
callbacks; TLB invalidation is mandatory and supplied by the caller.

On allocation failure, newly allocated intermediate tables are removed and
released before returning failure. Unmap removes an empty intermediate table
only if the caller's ownership callback confirms that the manager owns it.
Leaf data frames are not freed by unmap; their owner must manage their
lifetime.

The manager is covered by host tests with a synthetic page-table frame pool.
The bootstrap adapter copies the inherited PML4 into reserved static memory,
installs the copy in CR3, and exercises map/write/lookup/unmap on a temporary
address in an empty higher-half PML4 slot. Isolated QEMU images also confirm
that writes to read-only mappings and instruction fetches from NX mappings
fault with the expected page-fault error bits.

This is a tested bootstrap mapping path, not process address-space support.
The adapter relies on static kernel table memory being accessible at the same
numeric virtual and physical addresses, an assumption verified only with
QEMU/EDK2. The 32-page static table pool is bounded; no independent user root,
physical-to-virtual mapping window, demand paging, or SMP TLB shootdown exists.
