# IPC architecture baseline

**Status:** ADR-0010 still describes the planned request/reply protocol.
Phase 12 implements only bounded, non-blocking kernel-thread messaging; there
is no user ABI or request/reply protocol. See
[the implementation architecture](../ipc/architecture.md).

## Selected initial mechanism

The planned service protocol remains bounded synchronous request/reply
messages copied through validated caller buffers. Its fixed-width header
(`protocol_id`, major/minor version, opcode, payload length, request ID, flags)
and schema are not implemented. The current mechanism carries opaque bytes
only, with no versioning or opcode interpretation.

Bounded copied messages are the selected control-plane baseline (ADR-0010)
because their ownership and lifetime are simple. Measure their cost before
adopting shared memory. Later options:

| Mechanism | Benefit | Risk/cost | Initial status |
|---|---|---|---|
| Copied messages | Bounded ownership and simple validation | Copy and kernel entry cost | Implemented as bounded internal opaque channel payloads; service protocol not implemented |
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
- Current queue full returns `QUEUE_FULL`; send/receive never block. A future
  request/reply protocol may add blocking only with a scheduler wait queue.
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
bounded messaging is functionally tested in host and QEMU; latency, throughput,
jitter and CPU cost are **not measured**.
