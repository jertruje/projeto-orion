# IPC resource limits

All storage is statically bounded; the IPC path does not call a heap allocator.
Current compile-time limits are declared in `kernel/ipc/channel.h`.

| Resource | Limit |
|---|---:|
| Channels | 8 |
| Endpoint objects | 16 (two per channel) |
| Pending messages per endpoint | 8 |
| Payload bytes per message | 256 |
| Payload capacity per channel | 4,096 bytes (two queues) |
| Payload capacity across all channels | 32,768 bytes |

Queue metadata and endpoint/channel records add bounded overhead. The fixed
pool is not configurable at runtime. Channel/endpoint/capability exhaustion
returns `RESOURCE_EXHAUSTED`; no message is silently dropped on queue full.
There is no per-process quota or rate limiter because there is one kernel
process and no user-facing creation API. Revisit limits with workload and
memory measurements before exposing IPC to user services.
