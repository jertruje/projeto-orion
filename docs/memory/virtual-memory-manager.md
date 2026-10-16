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
It is not currently connected to the UEFI-inherited CR3, does not install a
new root, and has not changed any live kernel mapping. Do not use it for a
process address space until a safe physical-to-virtual mapping, CPU-specific
NX setup, frame ownership, and live QEMU permission tests are added.
