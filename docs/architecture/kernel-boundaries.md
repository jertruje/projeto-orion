# Kernel and user-space boundaries

**Status:** selected architecture baseline. No user-mode boundary is
implemented in the current scaffold.

## Placement

| Subsystem | Selected placement | Responsibility/state | Privilege and interface | Dependencies/failures | Performance/test gate | Phase |
|---|---|---|---|---|---|---|
| Entry/context switch | Kernel + minimal x86-64 assembly | CPU register/stack transition | Ring 0 internal ABI | GDT/TSS, stack, ABI; corrupt frame is fatal | ABI/frame tests and QEMU context round-trip | 7, 9 |
| Exceptions/interrupt entry | Kernel | IDT, vector/frame, EOI bookkeeping | Ring 0 stubs; internal handler contract | IDT, controller, safe stacks; unexpected fatal exception panics | Controlled #BP/#UD/#GP, external IRQ and nesting tests | 7, 10 |
| Physical memory | Kernel | Page/frame ownership and reservations | Internal allocator API, physical ranges | Validated boot map; exhaustion/invalid free | Property tests, reservation checks, exhaustion | 8 |
| Virtual memory/address spaces | Kernel mechanism | Page tables, permissions, TLB, address-space identity | Internal VM API; user operations later through syscalls/capabilities | Physical allocator, CPU features; faults fail closed | Map/unmap/permission/canonical address tests | 8, 11, 19 |
| Threads/scheduler | Kernel | Runnable state, context, accounting | Kernel thread API; user scheduling later | Memory, interrupt frames, timer; invalid transition panics/asserts | Deterministic cooperative scheduling before preemption | 9, 10 |
| Synchronization | Kernel primitives + user libraries | Locks/queues and memory ordering | Kernel internal; user wait primitives via API | Interrupt context and SMP model; deadlock/starvation | Lock-order, stress and SMP tests | 9–10, 12 |
| IPC transport | Kernel minimal mechanism | Bounded endpoints, wait queues, message transfer | Syscall/handle API authorized by capabilities | Address-space validation, scheduler; malformed messages rejected | Round-trip/throughput/jitter and fuzz tests | 11–12 |
| Capabilities | Kernel mechanism; policy/bootstrap in supervisor | Object handles, rights, derivation/revocation | Unforgeable process-local handles; explicit transfer | Object lifecycle, process identity; stale/revoked handles fail | Negative rights, reuse, race and confused-deputy tests | 11 |
| Timer/time | Kernel mechanism for monotonic clock and scheduling event; policy/API in service | Monotonic ticks/deadlines, timer queue | Kernel internal + capability-checked API | Calibrated source, interrupts, scheduler | Resolution/jitter/overflow and expiration tests | 10, 15 |
| Device mediation | Kernel grants bounded MMIO/IRQ/DMA authority; policy in device manager | Resource reservations and interrupt routing | Device capabilities, IOMMU mappings when available | PCI/ACPI parse, VM, IRQ; absent IOMMU limits DMA isolation | Malformed tables, BAR/IRQ bounds and DMA containment | 11, 14 |
| System call interface | Kernel entry/validation; stable wrappers in user libraries | Narrow ABI, copying/validating arguments | Explicit syscall number + caller address space + handles | GDT/TSS/IDT, process VM, scheduler; invalid args return defined error | ABI tests, syscall fuzzing and perf measurement | 19 |
| Diagnostics/logging | Kernel emergency console only; normal collection in log service | Early boot/panic output vs event aggregation | No authority to alter system state; bounded records | Serial/device failure, concurrency; loss reported | Buffer bounds, recursion, event integrity, overhead | 6, 13 |
| Service supervision | User-space supervisor; kernel supplies process lifecycle and event notification | Restart policy, backoff, dependency health | Cannot recover kernel; independent authority over managed tasks only | Process/IPC/capabilities, persistent state protocol | Crash injection, loops, recovery data integrity | 13 |
| Storage/filesystem | User-mode service and block drivers | Volume/filesystem policy, transaction/recovery | Capability-mediated block and file objects | Driver, memory, timer, IPC; corruption isolated, not hidden | Power-cut, checksum and consistency tests | 14–16 |
| Network stack | User-mode service/driver where feasible | Protocol state, sockets, quotas | Explicit network capabilities | Device, buffers, timer and IPC; malformed packet isolated | Protocol fuzz, loss/throughput and exhaustion | 14–17 |
| GUI/compositor | User mode | Framebuffer/display policy, windows/input | Display/input capabilities per session | Driver, process/API; renderer crash must not corrupt kernel | Pixel, clipping, input and restart tests | 19–20 |

## Kernel inclusion rule

A feature belongs in ring 0 only when it must perform a privileged operation,
maintain an invariant that cannot safely be delegated, or mediate a resource
without creating a kernel/user race. Policy, protocol complexity and
presentation belong in services by default. Any exception needs an ADR stating
why, authority granted, performance evidence, failure impact and test plan.

“User mode” is not synonymous with safe: shared physical buffers, DMA, interrupt
ownership, and kernel bugs remain cross-boundary risks.
