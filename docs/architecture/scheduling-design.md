# Scheduling and execution model

## Execution model proposal

A process owns an address space, handle namespace, resource budget and lifecycle.
A thread owns CPU state, kernel stack, scheduling state and reference to its
process. The first implementation is kernel-only and cooperative; user
processes require actual CPL3 entry/return and separate page tables.

## Selected progression and baseline policy

Bring-up starts with a single-CPU, cooperative round-robin queue for kernel
threads after the context-switch ABI, stack lifecycle and allocator have been
verified. Threads are `NEW`, `READY`, `RUNNING`, `BLOCKED`, `TERMINATED`;
transitions are centralized. The current bounded run-ready path returns to the
bootstrap thread when its initial ready queue empties; that thread then enters
the existing halt loop. A thread cannot free its active stack; cleanup is
deferred to another context.

The intended general-purpose baseline is preemptive round-robin on one CPU,
with a monotonic timer, bounded scheduling quantum and no user-controlled
priority in the first user-mode release. The quantum is an implementation
parameter selected by measurement, not a real-time guarantee. SMP is a
separate later decision after per-CPU ownership, interrupt routing, TLB
shootdown and synchronization pass their tests. Priority inheritance, deadline
scheduling and hard real-time promises remain out of scope absent formal timing
analysis.

| Goal | Mechanism to evaluate | Risk |
|---|---|---|
| Fairness | Per-thread CPU/wait accounting and bounded queue service | Equal rotation does not imply equal useful work |
| Responsiveness | Measure wake-to-run and input-to-service tail latency | Short quanta raise interrupt/context-switch overhead |
| Throughput | Batch work and measure completed operations/CPU | Optimizing throughput may worsen tails |
| Priority bounds | Begin without user-controlled priorities; later validate limits | Priority abuse and inversion |
| Starvation prevention | Aging or bounded service only after measurement | Aging adds state and policy interactions |
| Overload | Admission/queue/resource quotas and explicit rejection | Dropping/rejecting work can affect availability |
| SMP | Per-CPU queues or shared queue evaluated after AP startup | Migration, affinity and simultaneous ownership races |

## Required measurements

Context-switch cycles/time, timer interrupts/s, runnable depth, wake-to-run
latency distribution, CPU/idle time, throughput, starvation interval and
preemption-disabled critical duration. Record clock source and resolution.
Logging is excluded from hot-path timing or separately measured.

## Phase 9–10 implementation status

The kernel now has one static kernel process, a bounded pool of kernel-thread
records/stacks, a FIFO ready queue, and an ABI-defined cooperative switch. A
PIT channel-0 timer is routed through the remapped legacy PIC to IDT vector 32.
QEMU demonstrates timer preemption of two CPU-bound workers with no yields,
along with timer/handler/scheduler counters and a bounded TSC measurement.
Round-robin and IRQ delivery are implemented and tested only on one QEMU CPU.
The timer is masked after the boot self-tests; user mode, general
blocked-thread wakeup, AP startup, SMP, affinity and migration remain
unimplemented.

## Safety gates

The boot self-test enables interrupts only after the IDT vector, PIT/PIC setup,
EOI, stack/frame and return path are ready, then masks the timer before idle.
Scheduler mutations mask local interrupts. Do not start APs until per-CPU
stack, GDT/TSS, IDT, local interrupt controller, memory/TLB ownership and
cross-CPU scheduler synchronization are implemented and tested. No user-mode
or multicore guarantee follows from the single-CPU preemption result.
