# x86-64 page tables

The implementation walks indices at bits 39, 30, 21, and 12 and uses 512
entries per table. PTEs are limited to present, writable, user, write-through,
cache-disable, and NX attributes; huge pages and global-page handling are not
implemented. The manager accepts a 48-bit canonical virtual address and a
page-aligned physical address below 4 GiB.

Callbacks explicitly distinguish a physical frame number from the virtual
pointer used to access the table. The callback implementation owns the mapping
between them. The manager does not assume that equal numeric physical and
virtual addresses are interchangeable.

Every successful map/unmap calls the supplied single-address invalidation
callback. There is no built-in `invlpg`, CR3 reload, or SMP TLB shootdown.
Before live use, the callback and table-frame access must be validated in the
active address space; the current kernel intentionally does not mutate CR3.
