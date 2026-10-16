# IPC architecture baseline

**Status:** protocol architecture only; no IPC implementation exists.

## Selected initial mechanism

Begin with bounded synchronous request/reply messages copied through validated
caller buffers. Messages have a fixed-width header (`protocol_id`, major/minor
version, opcode, payload length, request ID, flags) and an explicitly defined
payload schema. Avoid serializing native pointers or compiler-dependent structs.
Kernel validates all lengths, arithmetic, endpoint generation, rights and
queue quota before copying. Unknown major version/opcode returns an explicit
error; compatible minor extensions require a specified optional-field rule.

Bounded copied messages are the selected control-plane baseline (ADR-0010)
because their ownership and lifetime are simple. Measure their cost before
adopting shared memory. Later options:

| Mechanism | Benefit | Risk/cost | Initial status |
|---|---|---|---|
| Copied messages | Bounded ownership and simple validation | Copy and kernel entry cost | Selected control-plane baseline; not implemented |
| Referenced buffers | Less copying for larger payloads | Pin/lifetime/TOCTOU and access-right complexity | Defer until measured |
| Shared memory | High throughput for bulk data | Synchronization, ownership, stale mappings and covert channels | Capability-gated later |
| Notifications/events | Efficient state change/wakeup | Lost/coalesced event semantics and denial of service | Define separately after scheduler |
| Synchronous calls | Simple request/reply ordering | Caller blocks; deadlock and priority inversion risks | Initial small control protocol |
| Asynchronous calls | Concurrency and throughput | Correlation, cancellation, queue growth and races | Defer until needed |

## Contract and errors

- The kernel authenticates sender from current process identity; a payload
  cannot assert a sender.
- Endpoint capability limits connect/send/receive/transfer rights separately.
- Maximum inline payload, per-endpoint queue depth, per-process outstanding
  requests and aggregate memory are compile-time or policy-bounded values; exact
  values remain open pending workload and memory measurements.
- Full queue returns `WOULD_BLOCK` or blocks only when the caller requested it
  and a valid scheduler/wait queue exists. No silent drop for reliable request
  messages. Notifications may coalesce only if protocol specifies it.
- Cancellation carries a request ID and is best-effort; a completed operation
  wins the race. Timeout does not prove the remote side stopped.
- Endpoint/process teardown marks the endpoint closed, wakes waiters with a
  peer-closed result, cancels owned requests and revokes transferred rights per
  explicit lifecycle rules.
- User pointers are copied and validated at operation time. No pointer remains
  retained after the syscall unless a separate pinned-buffer capability owns
  that memory.
- Rate/queue quotas and per-client accounting prevent one sender from consuming
  unbounded memory or starving unrelated endpoints.
- ABI changes use protocol and syscall versioning; incompatible change needs a
  new major version and transition plan.

## Metrics

Measure empty request/reply round-trip, one-way latency, p50/p95/p99, jitter,
messages/s, bytes/s, CPU cycles/operation, copy size, queueing delay and
behavior under saturation. Fix build, CPU, accelerator, vCPU count, guest RAM,
payload, batch size and clock source. Compare copy/shared-buffer variants only
when semantics and validation guarantees are equivalent. Current result:
**not measured; IPC absent**.
