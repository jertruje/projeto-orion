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
transitions are centralized. Idle runs only when no runnable thread exists. A
thread cannot free its active stack; cleanup is deferred to another context.

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

## Safety gates

Do not enable interrupts until IDT entries, controller setup/EOI, stack/frame,
return path and panic behavior are tested. Do not preempt within non-reentrant
critical sections. Do not start APs until per-CPU stack, GDT/TSS, IDT, local
interrupt controller and scheduler ownership are ready. Current kernel is a
single idle execution context; no scheduling claims apply.
