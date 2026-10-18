# IPC message format

The internal API accepts an explicit `uint32_t` byte length and a pointer to
the payload. The queue stores that length with exactly that many bytes.
Payloads are opaque bytes; the IPC mechanism does not interpret protocol
headers, native structs, pointers, or endianness.

| Property | Value/behavior |
|---|---|
| Maximum payload | 256 bytes |
| Minimum payload | 0 bytes; zero-length messages are valid |
| Queue capacity | 8 messages per endpoint |
| Ordering | FIFO within each receiving endpoint |
| Copy semantics | Send copies into a kernel queue; receive copies out |
| Oversize send | `MESSAGE_TOO_LARGE`; no queue mutation |
| Too-small receive buffer | `BUFFER_TOO_SMALL`, reports required length and leaves the message queued |
| Invalid pointer convention | A null source/destination is rejected when nonzero bytes would be copied |
| Malformed internal length | Invariant violation returns `CORRUPTION`; message is not consumed |

There is no user-space caller yet. Kernel pointers are trusted by this internal
API and cannot be validated as user mappings; future syscalls must validate
and safely copy user buffers before exposing this API to Ring 3.
