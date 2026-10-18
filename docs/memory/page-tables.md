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
callback. The x86-64 bootstrap adapter supplies `invlpg`; host tests use a
test callback. The active adapter copies the inherited PML4 before changing
CR3 and preserves the inherited entries. Its single-address mapping and
permission-fault paths pass in QEMU/EDK2. This does not implement an SMP TLB
shootdown, general CR3 migration, or process address spaces.

The adapter accesses its statically reserved page-table pages through equal
numeric virtual and physical addresses. That mapping assumption is validated
only in the tested QEMU/EDK2 environment and must be replaced or independently
proven before claiming support for other firmware or hardware.
