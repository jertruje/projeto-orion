# Phase gates and architecture acceptance

Each gate requires executable evidence where the phase is implementation work.
Document presence alone is insufficient.

| Gate | Entry condition | Exit evidence |
|---|---|---|
| Technical research (2) | Phase 1 requirements/metrics and initial risk baseline exist | Source register with claim/scope/date/confidence; evidence-labeled comparison of required systems/axes; classified limitation analysis; security/performance distinction; benchmark protocol; gaps/unavailable sources recorded; no copied code or implementation |
| Architecture decision (3) | Requirements and research inventory reviewed; open evidence explicitly scoped | ADR with alternatives, subsystem placement, interfaces, security/IPC/memory/scheduling/drivers/recovery/boot contracts, risks and dependency validation |
| Toolchain and repository baseline (4) | Architecture baseline and host inspection complete | Documented versions/targets/flags; freestanding probe linked as x86-64 ELF with no undefined symbols; format/architecture/symbol inspection; repeat hash comparison; documented commands and clean outputs |
| Kernel core (6) | Boot contract builds and passes in QEMU | Documented module contracts/invariants; deterministic boot stages; boot-info and inherited CPU/stack validation; bounded logger and panic host tests; freestanding ELF with no undefined symbols; normal and dedicated fatal-path QEMU tests |
| CPU/interrupts (7) | Kernel core baseline passes | GDT/TSS and IDT compile/runtime checks; normalized frames; controlled #BP/#DE/#UD/#GP/#PF/#DF tests in isolated QEMU; explicit external IRQ/IF policy |
| Memory (8) | Safe exception path and boot map audited | Physical allocation, page tables/permissions, page fault diagnosis, reserved region tests |
| Threads/scheduler (9) | Memory and ABI validated | At least two kernel threads alternate, lifecycle and queue invariants tested |
| Timer/SMP (10) | Safe IDT, context switch and synchronization | Preemption test on one CPU; SMP separately needs multiple-vCPU race/ownership evidence |
| Capabilities/IPC (11–12) | User/process address spaces and lifecycle exist | Negative rights, attenuation/revocation, malformed messages, quotas and teardown tests |
| Supervisor/drivers/time/storage/network (13–17) | Prerequisite mechanism and device mediation complete | Fault injection, lifecycle, device-specific QEMU/hardware, data integrity and performance data |
| Verified boot/package (18, 21) | Trust root and persistent transactional storage designed | Wrong key/corrupt/rollback rejection; power-cut update/install/remove integrity tests |
| Syscalls/GUI (19–20) | User mode, API, process model and display/input access exist | User app execution, ABI tests, framebuffer/input tests within actual guest |
| Audit/benchmark/release (22–24) | Relevant subsystems implemented and versioned | Security review + negative/regression suite; reproducible benchmark data; RC criteria and artifact manifest |

## Architecture gate result

**Architecture baseline selected; implementation gates remain open.** The
selected baseline is a microkernel with capability-mediated authority and
user-mode services/drivers by default (ADR-0009, ADR-0012). The documentary
choice is grounded in project requirements and the evidence register, with
research limitations recorded. It is not a measured claim that ORION is safer
or faster and is not implementation evidence.

The baseline is ready to guide implementation because subsystem placement,
boot dependencies, IPC default, memory invariants, scheduling progression and
fault policy are documented. The following remain mandatory before claims
about corresponding properties:

1. Fases 7–10: safe CPU entry/exit, memory ownership, user mappings and tested
   thread/scheduler lifecycle.
2. Fases 11–12: capability attenuation/revocation, IPC validation, quotas,
   teardown and negative tests.
3. Fases 13–14: independent service supervision, device mediation, DMA/IOMMU
   policy and injected-failure containment.
4. Fase 18: authenticated boot/update trust policy and rollback handling.
5. Before release: measurements and security review on documented
   workload/platform; revise ADRs if results contradict their assumptions.

## Kernel core gate result

**Phase 6 accepted for its defined minimal-core scope.** Host unit tests,
freestanding ELF inspection, normal QEMU boot through the idle message, and a
separate QEMU panic-path image passed. Both VM runs remain alive for their
configured observation interval and are then deliberately stopped by the
harness. This is not evidence for interrupt safety, memory protection,
recovery, SMP, physical UART availability, or long-term stability.

## CPU and exception gate result

**Phase 7 accepted for CPU exceptions only.** The kernel installs and reads
back its GDT/TSS/IDT, retains IF clear, returns from a controlled breakpoint,
and routes isolated divide, invalid-opcode, general-protection, page-fault and
double-fault tests through the expected vector/error-code logging and
fail-stop path. The #DF test deliberately removes the #UD and #NP present bits
to force failure during exception delivery and exercises IST1. No external
PIC/APIC IRQ is enabled or tested; there is no page-fault recovery, privilege
transition, SMP, or performance/security claim.

## Memory gate result

**Phase 8 incomplete.** The kernel now validates the final UEFI descriptor map,
keeps all non-conventional regions reserved, initializes a bounded physical
bitmap allocator, checks its invariants, and performs a QEMU allocate/release
smoke test. Host tests cover map overlap/alignment, reservation, exhaustion,
invalid/double free, four-level page-table map/lookup/unmap, permission policy,
and out-of-memory rollback. QEMU #PF output includes CR2 and decoded error
bits. The virtual page-table manager is not wired to the inherited CR3 and
has not been exercised against active hardware page tables; no new address
space is installed. Therefore the Phase 8 gate is not closed, and Phases 9–10
remain blocked.
