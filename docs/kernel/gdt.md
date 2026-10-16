# Global Descriptor Table — Phase 7

`kernel/arch/x86_64/gdt.c` builds five descriptors: null, ring-0 64-bit code,
ring-0 data, and the two-slot 64-bit TSS descriptor. `interrupts.S` executes
`lgdt`, reloads CS through a far return, reloads data/stack selectors, and
executes `ltr`. Runtime readback verifies GDTR, CS, DS and TR selectors.

Before replacing inherited descriptor tables, initialization briefly masks
NMI using the PC-compatible CMOS/NMI control at port `0x70`, preserves the
previous port value, and restores it only after the new IDT is loaded and
verified. If descriptor setup fails, panic halts with NMI masked rather than
continuing with a possibly inconsistent table pair.

The TSS sets RSP0 to the kernel stack top, installs a dedicated 16 KiB IST1
stack for double fault, and sets the I/O-map base to the TSS limit so no
bitmap grants port access. Static assertions verify the 104-byte TSS and
10-byte GDTR layout. The TSS does not create user mode or make RSP0 use
operational without a privilege transition.

QEMU ordinary boot and the isolated double-fault test exercise GDT/TSS loading.
The IST has no guard page and its behavior is not evidence of memory
protection.
