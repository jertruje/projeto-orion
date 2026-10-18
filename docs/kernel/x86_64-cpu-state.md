# x86-64 CPU state — Phases 6–10

The UEFI loader transfers control to an x86-64 ELF entry in long mode with
paging and PAE enabled. `_start` disables maskable interrupts, selects the
kernel's static 64 KiB stack, aligns `RSP` for the System V AMD64 ABI, clears
`RBP`, and calls `kernel_main` with the boot-info pointer in `RDI`.

Before changing descriptor tables, the core checks the inherited long-mode,
paging, PAE, stack-range/alignment, and IF conditions. Phase 7 then installs a
kernel-owned GDT and IDT and verifies their loaded table registers/selectors.
That Phase 7 work does not replace CR0/CR3/CR4/EFER or build page tables.
Phase 8 checks whether the CPU advertises NX and EFER.NXE is already enabled;
it does not enable the feature. In the tested QEMU/EDK2 path, the active-page
table smoke test copies the inherited PML4 into reserved static memory and
switches CR3 to that clone while preserving its entries. It does not establish
an independent address space, and its identity access to static table memory
is not validated for other firmware or hardware.

The kernel code segment is 64-bit (L=1, D=0); the data descriptor is a
flat long-mode data descriptor. Base/limit segmentation does not provide
ordinary memory isolation in 64-bit mode; paging is still inherited from the
loader. Development ELF symbols remain enabled with `-g`.

The kernel remains single-CPU. IF is enabled only during the bounded PIT
preemption test after vector 32 and PIC/PIT delivery are configured; scheduler
mutations save/clear/restore IF, and the timer is masked before idle. No AP
startup, local APIC/IOAPIC, or SMP synchronization is enabled.
