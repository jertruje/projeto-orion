# ORION OS

ORION OS is an independent operating-system project intended to be developed
from scratch. Its long-term goals include isolation, security, controlled
failure recovery, predictable performance, and hardware/application support.
These are objectives, not demonstrated advantages over Linux or any other OS.

## Current evidence

- A freestanding x86-64 ELF kernel and a UEFI PE/COFF loader build with the
  installed Clang/LLVM toolchain.
- The QEMU/EDK2 boot test has observed the loader validate and reserve the
  kernel image, exit UEFI boot services, enter the kernel, validate its boot
  information, and remain in its halt loop.
- Phase 6's minimal kernel core has deterministic boot stages, bounded serial
  logging, boot-info and inherited CPU/stack checks, a non-returning panic
  path, host unit tests, and a QEMU boot observation through the idle loop.
  Those checks alone do not provide memory management, scheduling, recovery,
  SMP, or security/performance results.
- Phase 7 adds a kernel GDT/TSS/IDT and isolated QEMU checks for selected CPU
  exceptions. External IRQs remain disabled; this is not evidence of process
  isolation or system-wide security/performance.
- This is a bring-up scaffold, not a general-purpose OS. Phase 8 has a limited
  physical page allocator and a host-tested page-table manager, but the manager
  is not connected to active CR3. There are no processes/threads, scheduler,
  timer preemption, SMP, drivers, userspace, or GUI.
- Phase 2 research is approved as a documentary synthesis with explicit
  evidence limits. One seL4 IPC microbenchmark is recorded, but there is no
  comparable cross-OS benchmark and no ORION performance or security result.
- The workspace was initially not a Git repository. Consult the current Git
  status and `docs/phase-status.md` for the recorded state.

## Build and test

The current Windows setup uses the portable MSYS2 installation under
`%LOCALAPPDATA%\orion-msys64\msys64`; no packages are installed by the project
build. With that toolchain available:

```powershell
mingw32-make check-tools
mingw32-make toolchain-test
mingw32-make build
mingw32-make inspect
mingw32-make test-unit
mingw32-make test-boot
mingw32-make test-panic-qemu
mingw32-make test-exceptions-qemu
mingw32-make validate-docs
```

Kernel core module contracts and validation limits are documented in
[the Phase 6 core guide](docs/kernel-core.md) and its linked `docs/kernel/`
documents.

The limited Phase 8 memory implementation and its remaining live-page-table
gate are documented under [memory](docs/memory/memory-model.md).

The standalone toolchain probe and environment/language decisions are
documented in [Toolchain](docs/toolchain.md),
[Development environment](docs/development-environment.md),
[Language decisions](docs/language-decisions.md), and
[the probe instructions](tests/toolchain/README.md). The probe is not kernel
or bootloader code and does not require QEMU.

`test-boot` runs QEMU with EDK2 and checks serial output through the kernel
entry and idle loop. A passing boot test does not prove interrupts, memory
protection, application isolation, recovery, or real-hardware support.

## Technical baseline

- [Requirements](docs/requirements.md)
- [Architecture options](docs/architecture-options.md)
- [System architecture overview](docs/architecture/system-overview.md)
- [Kernel boundaries](docs/architecture/kernel-boundaries.md)
- [Component model](docs/architecture/component-model.md)
- [Security architecture](docs/architecture/security-architecture.md)
- [IPC design](docs/architecture/ipc-design.md)
- [Memory and concurrency](docs/architecture/memory-and-concurrency.md)
- [Scheduling design](docs/architecture/scheduling-design.md)
- [Driver model](docs/architecture/driver-model.md)
- [Failure recovery architecture](docs/architecture/failure-recovery.md)
- [Boot sequence architecture](docs/architecture/boot-sequence.md)
- [Interfaces and contracts](docs/architecture/interfaces-and-contracts.md)
- [Architecture risks](docs/architecture/architecture-risks.md)
- [Phase dependencies](docs/architecture/phase-dependencies.md)
- [Phase gates](docs/phase-gates.md)
- [Toolchain](docs/toolchain.md)
- [Development environment](docs/development-environment.md)
- [Language decisions](docs/language-decisions.md)
- [Threat model](docs/threat-model.md)
- [Success metrics](docs/success-metrics.md)
- [Failure recovery and compatibility](docs/recovery-compatibility.md)
- [Linux comparison protocol](docs/linux-comparison.md)
- [Decision log](docs/decision-log.md)
- [Risk register](docs/risk-register.md)
- [Requirements traceability](docs/traceability.md)
- [Phase 2 source notes](docs/research/phase-2-evidence.md)
- [Evidence register](docs/research/evidence-register.md)
- [Linux limitations](docs/research/linux-limitations.md)
- [Architecture comparison](docs/research/architecture-comparison.md)
- [Security analysis](docs/research/security-analysis.md)
- [Performance analysis](docs/research/performance-analysis.md)
- [Driver and hardware compatibility](docs/research/driver-and-hardware-compatibility.md)
- [Failure recovery](docs/research/failure-recovery.md)
- [Benchmark plan](docs/research/benchmark-plan.md)
- [Research gaps](docs/research/research-gaps.md)
- [Phase 2 conclusions](docs/research/phase2-conclusions.md)
- [UEFI-to-kernel boot contract](docs/boot-contract.md)
- [Phase status and validation evidence](docs/phase-status.md)
- [Phases 11–20 audit and blockers](docs/phase-status-11-20.md)
- [Final batch (21–26) audit and blockers](docs/phase-status-21-26.md)

Documentation and code are intentionally explicit about provisional decisions,
unmeasured metrics, and incomplete phases. Do not infer implementation from a
planned requirement.
