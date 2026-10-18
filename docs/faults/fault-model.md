# Fault model and containment

## Current implementation

The kernel has a fixed-size in-memory diagnostic ring in
`kernel/fault/report.c`. It records structured CPU exception and kernel panic
events without dynamic allocation. Ordinary operations can report failures
through the same API, but existing subsystems are not all instrumented.

The current kernel has one Ring 0 process. There is no user process, isolated
service, driver framework, or independent supervisor. Therefore no user or
service failure can currently be contained or restarted as an independent
component.

## Classification and response

| Category | Detection/context | Record and response | Continue/restart policy |
|---|---|---|---|
| Recoverable operation error | Subsystem operation | Record category, detail and explicit outcome when wired; return an error | Continue only if the subsystem preserves its invariants |
| Invalid kernel input | Kernel API boundary | Reject input; report when caller wires the diagnostic API | No privilege grant or silent default |
| Thread exit | Scheduler lifecycle | Exit code and lifecycle are tracked by the scheduler | Current normal thread exit is not fault containment |
| User process failure | Not available: no Ring 3 process | No process report or containment path | Pending |
| IPC error | IPC returns explicit status | Caller receives the status; no centralized fault hook yet | Kernel continues unless an invariant is violated |
| Resource exhaustion | Allocator, scheduler and bounded IPC pools | Existing APIs return failure/status; diagnostics are not uniformly centralized | No automatic restart |
| CPU exception | x86-64 exception dispatch | Record vector, CPU error code, RIP, CPL context and CR2 for #PF; #BP resumes, others panic | No general exception recovery |
| Driver failure | Registry start callback returns failure | Partial cleanup runs, binding enters FAILED, and a Phase 13 fault event records the PCI identity where available | The device can remain unused; no driver restart or containment of arbitrary kernel-code faults |
| Suspected kernel corruption | Invariant checks and fatal paths | Panic and halt; best-effort bounded report | Never resume normal execution |
| Kernel panic | Panic path | Record a bounded event, emit existing serial diagnostics and halt | No automatic reboot/recovery |

The report ring keeps the newest 32 entries and overwrites the oldest entry
when full. Event identifiers do not wrap: once exhausted, writes fail
explicitly. The ring is volatile and is lost on reboot; it is not a durable
audit log.
