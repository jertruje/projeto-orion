# Kernel core invariants — Phase 6

## Enforced by code and tests

1. `_start` is the sole assembly entry and calls the core after selecting the
   reserved stack and disabling maskable interrupts.
2. `kernel_main` validates boot-info before reading its map-size field.
3. Init stages are valid enum values and advance exactly once, monotonically,
   without duplicates or skips. Rejected transitions do not modify state.
4. The entry-state validator confirms `RSP` lies inside the exported static
   stack bounds, observes the ABI entry alignment, and checks long-mode/paging,
   PAE, and `RFLAGS.IF == 0` conditions.
5. Logger module and message output are bounded; invalid modules emit no
   partial record. No log operation allocates or uses libc.
6. Panic does not return; recursive panic skips another log attempt and halts.
7. The core idle loop has no system-library dependency and keeps interrupts
   disabled.
8. Build uses freestanding flags and links the kernel with `--no-undefined`.

## Not established

These checks do not prove virtual-memory permissions, pointer accessibility,
stack guard pages, boot-map semantic validity, exception safety, interrupt
handling, multi-CPU synchronization, device availability, or recovery after a
kernel fault. The current tests also do not invoke privileged CPU validation
on a host process. These invariants require later mechanisms and tests.
