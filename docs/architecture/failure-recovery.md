# Failure handling and recovery architecture

## Classification and ownership

| Failure | Detector/decision owner | Proposed response | Reboot/data implications |
|---|---|---|---|
| Application exit/crash | Kernel reports exit; supervisor/policy service decides restart | Revoke handles, reclaim address space after threads stop; restart only under policy | Volatile application state lost |
| Essential service crash | Kernel lifecycle event; independent supervisor decides bounded restart/fallback | Terminate dependents or enter degraded mode; restart with backoff and new generation | Persistent transaction recovery required |
| Driver hang | Request timeout/watchdog and device manager | Cancel request, mask IRQ, quiesce/reset device if safe, revoke authority | DMA may force retaining buffers or reboot |
| IPC malformed/peer closed | Kernel parser/endpoint lifecycle | Reject request, account abuse, wake peer with explicit error | No system restart unless kernel invariant violated |
| Memory exhaustion | Allocator/accounting | Fail allocation atomically; reclaim only owned caches; reject/terminate policy offender | Never free unknown/live frame |
| Kernel exception/corruption | CPU handler/panic path | Capture bounded diagnostic and halt or controlled reboot by explicit policy | Reboot usually required; unsynced data may be lost |
| Storage/data corruption | Filesystem service checksum/structure validation | Isolate volume, read-only/degraded state, repair only with verified transaction | May require restore; no guaranteed recovery |
| Interrupted update | Boot verifier/slot manager | Select last authenticated known-good slot; cap retries | Without intact fallback, manual recovery may be required |

## Independence and loop prevention

The supervisor cannot depend solely on the service it monitors. Kernel tracks
process termination and resource revocation. Restart counts and backoff are
bounded; crash loops transition to `FAILED/DEGRADED` and require an explicit
operator or policy action. Logs use an independent early console path, but
diagnostics may be lost if hardware is unavailable.

## Recovery contract

An operation is “recovered” only if detection, containment, invariant
preservation, health check, and required data-integrity verification all pass.
Restarting a process alone is not recovery. Recovery policy must declare
idempotency, durable state, retries, resource revocation, dependent behavior
and terminal outcome. Kernel memory corruption, unknown DMA activity, hardware
failure and specification errors are not promised recoverable.

The current implementation adds a bounded, volatile fault-report ring and
records CPU exceptions and kernel panics. The ring is diagnostic evidence,
not a supervisor or restart/recovery subsystem. The existing `orion_panic`
remains a non-returning halt. There are no isolated user processes, service
lifecycle hooks, restart policy, or IPC teardown on process exit.
