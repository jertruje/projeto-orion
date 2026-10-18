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
| Timer/SMP (10) | Safe IDT, normalized interrupt frame, scheduler critical sections | PIT/PIC IRQ0 preemption with CPU-bound QEMU test and measured counters; SMP stays blocked until AP startup and multi-vCPU ownership tests |
| Block storage/filesystem (16) | Validated memory buffers and block contract; bounded driver I/O | Host range/error/corruption/fail-closed tests; explicit image formatting; normal boot with no disk; device-specific QEMU read/write and persistence across new QEMU processes; no power-loss claim without injection/recovery tests |
| Network (17) | Bounded network interface, tested timer, PCI discovery and explicit device strategy | Malformed/checksum/bounds/timeout unit tests; normal boot without NIC; real device-specific QEMU send/receive and protocol round trip; protocol, buffer ownership and non-goals documented |
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

**Phase 8 accepted for the documented bootstrap-memory scope.** The kernel
validates the final UEFI descriptor map, reserves non-conventional regions,
initializes a bounded physical bitmap allocator, checks its invariants, and
passes QEMU allocate/release plus map/write/lookup/unmap checks using a clone
of the inherited PML4 installed in CR3. Host tests cover map
overlap/alignment, reservation, exhaustion, invalid/double free, four-level
page-table operations, permission policy, and out-of-memory rollback. Isolated
QEMU tests confirm read-only write fault #PF(3) and NX instruction-fetch fault
#PF(17), as well as CR2 and error-bit diagnostics.

This gate does not establish a process address space or memory isolation.
The active adapter relies on static table memory being reachable through
equal numeric physical and virtual addresses; it has been exercised only with
QEMU/EDK2. There is no demand paging, SMP TLB shootdown, or proof of
portability to other firmware/hardware. This completes only the memory
bootstrap gate; it does not provide user address spaces.

## Thread and scheduler gate result

**Phase 9 accepted for cooperative kernel-thread scheduling only.** Host
tests check bootstrap registration, independent stack bounds/canaries,
invalid state transitions, pool exhaustion, and scheduler/process invariants.
QEMU/EDK2 runs two distinct kernel threads on independent stacks; the serial
trace and test assertions prove `2,3` alternation for four rounds, arguments
and volatile local state survive yields, both entry functions terminate, and
the bootstrap stack resumes before thread records are destroyed. The ready
queue is empty afterward and yield-with-no-peer returns without a switch.

There is one kernel process, ring-0 execution only, a fixed non-reused thread
pool, no general block/wakeup operation, no timer or preemption, and no SMP.
IF remains clear. This evidence does not establish user isolation,
multicore-safety, fairness under preemption, long-term stability, or timing
performance. At the time this Phase 9 gate was recorded, Phase 10 had not
started; its separate result follows.

## Timer and preemption gate result

**Phase 10 accepted for single-CPU timer preemption; multicore remains
blocked.** The kernel remaps/masks the legacy PIC, programs PIT channel 0 at
divisor 11,932, installs vector 32 in the IDT and acknowledges IRQ0 before
scheduling. Scheduler mutations save/disable/restore IF. QEMU 11.1.2 with
`-smp 1` completed two CPU-bound workers without yields; each exceeded one
million iterations, timer ticks and preemptions were non-zero, and the
no-ready-thread IRQ return path passed. The test also measured bounded
virtual-TSC samples for no-switch timer dispatch and scheduler handoffs.

The timer is masked again before idle. These results do not establish AP
startup, SMP synchronization, physical-device compatibility, an invariant
hardware tick rate, hard-real-time latency, FPU/SIMD context preservation, or
performance superiority. Phase 11 has a partial implementation result below;
the user-mode isolation and capability-mediated subsystem gate remains open.

## Capability model gate result

**Phase 11 partially implemented and tested; effective user isolation remains
pending.** `kernel/security/capability.*` provides fixed-capacity process,
object, slot and lease tables; opaque process-local handles with table/slot
generations; type-specific rights masks; attenuating derive/transfer;
object-wide revocation; leases that defer destruction; explicit exhaustion
and validation statuses; and invariant checks. It is initialized with and
checked alongside the kernel process. Host negative tests and a kernel startup
self-test in QEMU cover authorization, wrong types, rights denial, attenuation,
revocation, stale handles, leases, teardown and limits.

The implementation is serialized by disabling/restoring interrupts and is
valid only for the existing single-CPU kernel. There is no CPL3, process
address-space isolation, syscall boundary, IPC, device/memory mediation, or
SMP synchronization. The API can create initial objects only for the kernel
process; therefore no hostile process is isolated and no general process
capability-construction policy is yet implemented. Phase 12 may build on the
attenuation/transfer contract only after adding IPC validation, endpoint
ownership, quotas and teardown integration.

## IPC gate result

**Phase 12 partially implemented and tested for kernel threads.** Channels
have two capability-protected endpoints, separate bounded FIFO queues,
copy-based opaque messages (0–256 bytes), explicit empty/full/closed/rights/
resource errors, endpoint revocation and deferred reclamation while operations
hold leases. Static limits are eight channels, eight messages per endpoint,
and 32 KiB aggregate payload storage. Host tests cover message contents,
oversize/short/null buffers, queue saturation, authorization, handle
tampering, remote close/drain, independent channels, pool exhaustion and
invariants. QEMU boot exchanges four messages between two scheduled kernel
threads and verifies payload data.

Operations are non-blocking because no scheduler wait/wakeup primitive exists.
Single-core critical sections save/disable IF; this is not SMP synchronization.
There is no user mode, process address-space isolation, syscall buffer-copy
path, process-exit hook, capability-in-message transfer, request/reply
protocol, fuzzing campaign or performance measurement. Therefore the phase is
not approved as IPC between applications; those items remain gated for later
phases.

## Fault reporting gate result

**Phase 13 partially implemented.** A 32-entry static report ring records CPU
exceptions and kernel panics; host tests cover ring bounds, overwrite behavior,
field preservation, invalid input and panic classification. QEMU tests verify
an event is emitted for controlled CPU exceptions. The panic policy remains
fail-stop. No user process/service isolation, process-exit IPC cleanup,
supervisor, bounded restart policy, or service recovery is implemented; those
acceptance criteria remain open.

## Driver framework gate result

**Phase 14 partially implemented and tested.** The bounded registry, exclusive
driver match/lifecycle, cleanup after failed startup, read-only PCI mechanism
#1 enumeration on bus 0, and a working QEMU COM1 UART driver are exercised by
host tests and QEMU q35 boot. PCI discovery does not mean a PCI function has a
working driver. ECAM, bridge traversal, BAR sizing/allocation, MMIO mapping,
device IRQ routing, DMA/IOMMU, hot-plug and user-mode driver isolation remain
open; the phase is not approved as broad hardware support.

## Time and power gate result

**Phase 15 partially implemented and tested.** PIT IRQ0 provides a checked,
coarse monotonic nanosecond clocksource, and a static 32-slot timer queue
supports one-shot/periodic deadlines, cancellation and callbacks dispatched
outside interrupt context. Host tests cover conversion, monotonic clamping,
timer lifecycle, reentrant cancellation and callback failure. QEMU/OVMF q35
on one vCPU passed a 30 ms one-shot timer with a recorded nominal PIT quantum
of 10,000,150 ns and one observed lateness sample of 453 ns.

The scheduler has no general block/wakeup operation, so timed thread sleep,
blocking IPC/device timeouts and event wakeups are not implemented. The boot
contract supplies no ACPI RSDP/configuration table list; the kernel reads no
ACPI data. No civil clock, power event, shutdown, reboot, suspend, hibernate
or resume state is supported. The `hlt` idle instruction is not an ACPI
system-state transition. The phase remains partial pending these integrations
and statistically meaningful timing tests.

## Storage and filesystem gate result

**Phase 16 partially implemented and tested for one QEMU configuration.**
The block layer checks device state, ranges, byte counts and alignment. The
primary-master ATA PIO driver uses IDENTIFY/LBA28, 512-byte sectors, bounded
polling, synchronous I/O and cache flush. ORIONFS v1 and the internal VFS
provide bounded metadata/files/directories; I/O failures invalidate the
mounted instance so callers must remount before continuing.

Host unit tests cover block errors, filesystem operations, remount/corruption
and fail-closed behavior. QEMU 11.1.2/OVMF q35 discovered a dedicated 16 MiB
image, wrote a proof file, and a fresh QEMU process read and compared the
persisted bytes. The normal no-data-disk boot also passed. Formatting is a
separate explicit host tool action; the normal kernel boot never formats a
disk. The QEMU harness stops each VM after the kernel has flushed and
unmounted; this is not a power-cut simulation or a graceful guest shutdown.

This does not establish generic IDE/PCI support, physical hardware support,
partition discovery, journaling, atomic updates, power-loss recovery, SMP
safety, user filesystem descriptors, asynchronous I/O or performance
superiority. ORIONFS uses at most 4096 sectors (2 MiB), even if the backing
device is larger. See [filesystem.md](storage/filesystem.md) for format,
commands and exact limits.

## Network gate result

**Phase 17 partially implemented and tested for one QEMU device/network path.**
Host unit tests exercise protocol checksums, ARP, IPv4, ICMP Echo, UDP endpoint
limits/lifecycle, malformed/truncated input, finite ARP/ICMP timeouts and
transmit errors. QEMU 11.1.2/OVMF q35 detects the NE2000-compatible PCI device
(`10ec:8029`); its PIO driver transfers frames through the card's on-device
memory without system RAM DMA. A Windows host sent a 22-byte UDP payload
through QEMU user networking to guest port 40171 and received the identical
payload via the forwarded host port 40170. The network-test image also emits
bounded ICMP ping and counter diagnostics.

Only that QEMU NIC and slirp path are tested. TCP, DHCP, IPv6, user sockets,
general routing, physical link state, device IRQs, system DMA, SMP safety and
physical hardware support remain unimplemented or unvalidated. Ordinary boot
continues when the NIC is absent. See
[networking.md](network/networking.md) for bounds, buffer lifetime, exact
configuration and commands.
