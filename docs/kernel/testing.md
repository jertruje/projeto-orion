# Kernel core, CPU exception, and memory testing — Phases 6–8

## Host unit tests

Run `mingw32-make test-unit`. The target compiles:

- `tests/boot_info_test.c`: boot-info fields and initialization-state
  transition validity, duplicates, invalid values, null state, and names.
- `tests/kernel/log_test.c`: logger format, CRLF, decimal `uint64_t` maximum,
  message truncation, invalid inputs, exact/overlong module bounds, and
  simulated serial failure.
- `tests/kernel/panic_test.c`: normal/default source and reason, logging
  failure, wrapper behavior, and recursive panic. Architecture operations are
  mocked; each scenario runs as a separate process because panic is
  intentionally non-returning and its guard is static.
- `tests/kernel/memory/memory_test.c`: UEFI map validation, reserved ranges,
  physical allocation/alignment/release/exhaustion and allocator invariants.
- `tests/kernel/memory/page_tables_test.c`: canonical addresses, map/lookup/
  unmap, permissions/W^X, duplicate/unaligned rejection, ownership and OOM
  rollback using a synthetic page-table frame pool.

Host tests do not substitute for kernel execution and do not execute privileged
instructions.

## Kernel, ELF, and QEMU checks

Run `mingw32-make build inspect test-boot test-panic-qemu test-exceptions-qemu`.
The build retains development debug symbols and links with `--no-undefined`.
`inspect` reports ELF headers, program headers and symbols, plus undefined
symbols. The QEMU test verifies
loader-to-kernel serial milestones through the idle-loop message and observes
the VM for its configured interval before deliberately stopping the test
process. `test-panic-qemu` boots a separate test kernel, checks #BP return and
panic origin/reason, and requires the halted VM to remain alive for its observation
interval. `test-exceptions-qemu` separately injects #DE, #UD, #GP, #PF and a
double fault caused by deliberately clearing the #UD/#NP IDT present bits; it
checks #DF dispatch through IST1. These are path checks, not stability,
performance, hardware, or general fault-injection results.

Normal QEMU boot validates its actual UEFI map, initializes the physical
bitmap, checks allocator invariants, and allocates/releases one page. The
page-table unit test is not a live page walk: the virtual manager is not
attached to CR3, so live mappings and hardware permission enforcement remain
unverified.

`mingw32-make validate-docs` checks required documentation, phase rows,
traceability, Markdown links and architecture documents.

## Known test gaps

Only dedicated test kernels invoke fatal paths in QEMU. There are no physical
hardware, serial disconnect, long-duration, CPU-feature matrix, memory-safety,
external IRQ, or SMP tests in this phase.

No performance benchmark is defined for this bring-up kernel: it has no
scheduler, representative workloads, or stable timer framework. Security
validation is limited to descriptor/exception-path checks and negative
exception tests; it is not a security audit and does not establish user/kernel
isolation, capability enforcement, memory safety, or comparative security.
