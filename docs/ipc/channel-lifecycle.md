# IPC channel and endpoint lifecycle

1. `orion_ipc_channel_create` allocates one bounded channel and two endpoint
   objects, then installs capabilities in the current process. Creation fails
   atomically with zeroed output handles when a fixed pool or capability table
   is exhausted.
2. Each endpoint owns a FIFO of messages sent by its peer. Capabilities are
   separate objects; revoking/closing one endpoint does not revoke the other.
3. `orion_ipc_close` requires `CLOSE`, marks that side closed, discards its
   incoming queue as an explicit consequence of close, revokes every copy of
   that endpoint capability, and releases its lease.
4. Messages already queued for a still-open peer remain available after the
   sender closes. Once drained, receive returns `PEER_CLOSED`; future sends to
   the closed side also return `PEER_CLOSED`.
5. The endpoint object destructor runs after capabilities and active leases
   are gone. The channel storage is reclaimed only after both endpoint objects
   have been destroyed. Active leases keep endpoint/channel storage alive
   during an operation.
6. Closing the peer queue owner discards messages addressed to that endpoint.
   No concurrent waiter can be stranded because blocking operations are not
   implemented.

There is no process teardown hook for non-kernel processes yet; only the
existing kernel process can create a channel in this implementation.
Integration with process exit and service supervision remains future work.
