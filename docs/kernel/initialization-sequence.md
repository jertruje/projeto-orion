# Kernel initialization sequence — Phase 6

## Actual sequence

1. `_start` disables maskable interrupts, loads the 64 KiB static kernel stack,
   aligns `RSP`, clears `RBP`, and calls `kernel_main`.
2. `kernel_main` configures the existing serial output and logs the actual
   kernel-entry event.
3. The core advances `ENTRY` → `DIAGNOSTICS_CONFIGURED`. This means the
   diagnostics path was configured and logged; it does not claim hardware
   presence or successful transmission beyond the operation's return value.
4. The boot-info structure is validated. No boot-info fields are consumed
   before this validation.
5. The core advances to `BOOT_INFO_VALID` and reports the validated contract.
6. `orion_x86_64_validate_entry_state` checks the current stack pointer is in
   `[kernel_stack_bottom, kernel_stack_top)`, has the System V AMD64 function
   entry alignment, CR0.PE and CR0.PG are set, CR4.PAE is set, EFER.LMA is set,
   and RFLAGS.IF is clear.
7. The core advances to `CPU_STATE_VALID` and reports those checks.
8. It installs and verifies the kernel GDT/TSS and exception IDT while IF
   remains clear, then advances to `ARCH_TABLES_READY`.
9. The core advances to `CORE_READY`, reports the validated boot memory-map
   byte count and that UEFI boot services have exited, then advances to `IDLE`.
10. The core logs entry into the minimal idle loop and repeatedly executes
   `cli; hlt`.

Each transition must be exactly the next valid enum value and must match the
current state. An invalid transition, invalid boot contract, failed CPU-state
check, or failed required log leads to panic. Panic does not attempt recovery
or automatic reboot.

## Limits and dependencies

The CPU check is an assertion about inherited loader/firmware state, not a
CPU-feature discovery framework, a page-table audit, or proof that all
referenced memory is mapped. The kernel's own GDT/IDT cover ring-0 descriptor
setup and CPU exceptions only; external IRQs remain disabled. Since IF remains
clear and only one CPU is started, the idle loop has no timer, preemption, or
scheduler. These are later-phase dependencies, not claims of implemented
functionality.
