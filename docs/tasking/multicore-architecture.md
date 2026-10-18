# Multicore architecture status

## Status: blocked, not implemented

Only the boot CPU is initialized. QEMU validation uses `-smp 1`. The kernel
does not parse ACPI MADT processor/APIC entries, allocate AP bootstrap
resources, send INIT/SIPI, provide a trampoline, install per-CPU GDT/TSS/IDT
state, configure local APIC timers, or coordinate AP failure. The current
global scheduler state is single-writer only and must not be used concurrently.
No SMP support is claimed or enabled.

## Candidate strategies

| Strategy | Benefits | Costs / current blocker |
|---|---|---|
| One shared ready queue | Minimal policy change; easy global FIFO ordering | Requires SMP-safe queue locking and atomic thread ownership. |
| Local queue per CPU | Local operations and simpler CPU-local scheduling | Needs per-CPU structures, affinity and an idle/balance policy. |
| Migration | Can balance uneven workloads | Requires safe ownership transfer and cache/TLB consequences. |
| Fixed CPU affinity | Simplifies ownership and avoids migration | Can strand work when a CPU is idle or unavailable. |

For a future first SMP experiment, prefer pinned per-CPU queues with migration
disabled until AP startup and unique thread ownership are demonstrated. This
is a proposal only; no SMP queue or AP code exists.

Required future evidence includes multi-vCPU QEMU startup, per-CPU identity and
stack checks, concurrent work on at least two CPUs, duplicate-execution
prevention, queue stress/race tests, and explicit AP timeout/failure handling.
Memory reclamation also needs TLB shootdown before SMP can be considered safe.
