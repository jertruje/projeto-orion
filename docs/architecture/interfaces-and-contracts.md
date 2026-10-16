# Architectural interfaces and contracts

All interfaces below are **proposed contracts**, not APIs presently
implemented unless explicitly noted. Integer widths, structure layouts,
versioning and error codes must be fixed in implementation headers and tested.

## Common rules

- Use fixed-width integers, explicit byte order and checked lengths; no raw
  pointers cross process/service protocols.
- Every operation defines caller authority, preconditions, side effects,
  success result, error set and cancellation/lifetime behavior.
- Validate before mutating state; failure is atomic unless the API documents a
  partial result.
- Version public protocols. Reject unsupported major versions and unknown
  mandatory fields.
- User memory is copied/validated while the caller address space is active.
  Asynchronous retained memory requires explicit pin/ownership capability.

## Proposed internal/public contracts

| Interface | Preconditions/authority | Success/postcondition | Errors/failure rules |
|---|---|---|---|
| Boot handoff | Loader has loaded validated image and current memory map; UEFI boot services still active until EBS | Kernel entry receives versioned boot-info; services unavailable after EBS | Loader must stop on invalid image/map/EBS failure; kernel validates signature/version/size/alignment before use |
| Frame allocation | Kernel-only; valid page count/alignment and memory initialized | Returns owned physical range; ownership recorded | `INVALID`, `NO_MEMORY`; no state change on failure |
| VM map/unmap | Kernel-only; address-space ref, aligned canonical range, valid flags, frame authority | Mapping changes atomically; required TLB invalidation complete | `INVALID_ADDRESS`, `PERMISSION`, `CONFLICT`, `NO_MEMORY`; partial mapping rollback |
| Thread create/yield/exit | Kernel API; executable entry and stack owned by caller/process | State transition and queue membership consistent | `INVALID_STATE`, `NO_MEMORY`; active stack cannot be freed |
| Capability lookup | Caller process, handle generation, object type and requested rights checked | Returns reference only if rights subset is possessed | `BAD_HANDLE`, `WRONG_TYPE`, `DENIED`, `REVOKED`; no authority amplification |
| IPC send/receive | Endpoint capability, protocol/version, bounded payload and quotas | Message delivered exactly according to protocol contract | `TOO_LARGE`, `WOULD_BLOCK`, `PEER_CLOSED`, `TIMED_OUT`, `DENIED`; no silent success/drop |
| Device request | Driver owns device/IRQ/DMA capability and buffer lifetime | Completion includes status and transferred byte count | `UNSUPPORTED`, `IO_ERROR`, `TIMED_OUT`; timeout does not itself imply hardware quiescence |
| Process exit event | Kernel observes actual task termination | Supervisor receives process ID + generation + exit reason | Event queue overflow reported; cannot forge source identity |

## Compatibility and observability

Public API stability begins only after ABI review, version policy, test suite and
an application-facing use case exist. Logs identify subsystem/severity and
carry a bounded event; log loss is explicit. Current boot-info v1 is a concrete
loader/kernel ABI but does not satisfy the future syscall/service contracts.
