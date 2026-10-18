# IPC security and capability contract

All operations derive the owner from the currently scheduled thread, not from
a caller-supplied process ID. An endpoint is an `ORION_OBJECT_CHANNEL`
capability. A numeric handle without a valid slot in the current process table
does not grant access.

| Operation | Required endpoint right | Effect |
|---|---|---|
| Send | `ORION_RIGHT_SEND` | Append payload to the peer's incoming FIFO |
| Receive | `ORION_RIGHT_RECEIVE` | Remove and copy one payload from this endpoint's incoming FIFO |
| Query | `ORION_RIGHT_QUERY` | Read queue count/capacity and endpoint open state |
| Close | `ORION_RIGHT_CLOSE` plus `ORION_RIGHT_REVOKE` | Close this endpoint and revoke every copy |
| Derive/transfer endpoint authority | `DERIVE` / `TRANSFER` | Capability manager permits only a rights subset |

Authorization is checked by the capability manager before message buffers are
read or queue state is modified. Invalid and forged handles fail closed.
Message contents do not carry sender identity or capability authority.

Capabilities cannot yet be transferred as part of an IPC message. Atomic
move/copy semantics, receiver installation rollback and process-exit cleanup
need additional guarantees; do not serialize a handle into a payload and treat
it as authority. Since user mode and syscalls are absent, these APIs only
protect trusted kernel callers and do not establish a hostile-process boundary.
