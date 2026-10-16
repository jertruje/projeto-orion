# Kernel core structure — Phase 6

## Scope

This document describes the implemented single-CPU, freestanding x86-64 kernel
bring-up. It does not describe a complete microkernel, process model, scheduler,
memory manager, interrupt framework, or production kernel.

## Modules

| Area | Implementation | Responsibility |
|---|---|---|
| Entry | `kernel/arch/x86_64/entry.S` | Disable maskable interrupts, select the statically reserved stack, align it, and call `kernel_main` once. |
| Core | `kernel/core/main.c` | Coordinate boot-info and entry-state validation, report completed stages, then enter the idle halt loop. |
| Shared boot contract | `include/orion/boot_info.h` | Validate the loader-provided structure before the core reads its contents. |
| Initialization state | `include/orion/init_state.h` | Define valid stages and reject invalid, repeated, or skipped transitions. |
| Console | `kernel/console/serial.c` | Bounded polling output through the existing COM1-compatible serial path. |
| Logging | `kernel/log/log.c` | Emit bounded level/module/message records without libc or allocation. |
| Panic | `kernel/panic/panic.c` | Disable interrupts, log one fatal reason when possible, prevent recursive logging, and never return. |
| x86-64 entry validation | `kernel/arch/x86_64/cpu_state.S` | Check stack bounds/alignment, long mode and paging state, PAE, and IF clear. |
| GDT/TSS | `kernel/arch/x86_64/gdt.c`, `interrupts.S` | Install kernel selectors and TSS, including IST1 stack for double fault. |
| IDT/exceptions | `kernel/arch/x86_64/idt.c`, `interrupts.S`, `exceptions.c` | Install CPU exception gates, normalize frames, log exceptions and fail-stop unless handling #BP. |
| IF control | `kernel/arch/x86_64/interrupt_state.S` | Query and explicitly enable/disable maskable interrupts; normal boot keeps IF clear. |
| x86-64 fatal backend | `kernel/arch/x86_64/fatal.S` | Implement `cli` and a non-returning `cli; hlt` loop. |

The C modules depend on small architecture-specific contracts. No dynamic
allocation, libc, external IRQ framework, scheduler, user mode, or
general-purpose driver framework is present.

## Build and debug boundary

The development kernel is built with the freestanding flags in the root
`Makefile`, including `-ffreestanding`, `-fno-builtin`, `-mno-red-zone`, and
`-g`. The `build/` ELF includes debug information; it is not a production
artifact or stripped release image. `mingw32-make inspect` checks the final ELF
and undefined symbols.

## Limits

The boot CPU state is observed, not established by this core: UEFI/loader
remain responsible for the initial long-mode environment. Stack-range checking
does not establish page permissions, guard pages, or memory ownership. UART
configuration does not prove physical serial hardware exists. The logger and
panic state are not synchronized for SMP or interrupt-handler use.
