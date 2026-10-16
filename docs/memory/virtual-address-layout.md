# Virtual-address layout

The current kernel retains the loader's inherited paging structures and
identity-addressed ELF layout. The kernel ELF entry is linked at `0x200000`;
the loader limits PT_LOAD addresses to below `0x4000000`. The current 64 KiB
kernel stack and the allocator bitmaps reside in kernel BSS. No guard page,
high-half kernel mapping, user range, temporary mapping window, or MMIO
virtual region has been established.

No address-space migration is performed: CR3, the return address, active stack,
and currently executing code remain under the loader's mappings. A future
layout change must first inventory all active ranges and test a complete new
root before switching.
