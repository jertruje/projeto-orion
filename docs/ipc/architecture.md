# IPC architecture

## Implemented scope

The first mechanism is a kernel-internal, bounded, non-blocking, full-duplex
message channel. Each channel has two endpoint capabilities and one incoming
ring queue per endpoint. Sending on one endpoint appends to the peer's queue;
receiving drains the caller endpoint's queue. Message bytes are copied into
kernel-owned storage. No queue memory or caller pointer is retained.

The current caller is obtained from `orion_scheduler_current()->process`.
Operations validate a channel capability against that process before touching
the channel. Channel creation is effectively kernel-only because the current
capability object-creation API only installs new objects in the kernel process.
Endpoints can be handed off later using the existing capability transfer
operation, but user-mode delivery is not implemented.

## Why copied, fixed-size queues

Static bounded storage avoids allocator failure and ownership ambiguity in the
current kernel, which has no user address spaces or safe user-copy ABI. The
ring queue makes full/empty behavior explicit and permits deterministic
teardown. This is an implementation slice, not an established performance
choice; benchmark before replacing it with referenced or shared memory.

The channel is full duplex, permits multiple kernel threads to access either
endpoint sequentially, and preserves FIFO order independently in each
direction. Operations are non-blocking. A full queue returns `QUEUE_FULL`;
an empty queue returns `QUEUE_EMPTY` unless the peer has closed, in which case
it returns `PEER_CLOSED`.

## Not implemented

There is no wait queue, blocking send/receive, syscall ABI, Ring 3 interface,
process address-space isolation, message protocol, timeout, cancellation,
request/reply correlation, capability transfer inside a message, or shared
memory. The existing architecture document for future request/reply protocols
is [ipc-design.md](../architecture/ipc-design.md); the implementation contract
is documented in [security-and-capabilities.md](security-and-capabilities.md).
