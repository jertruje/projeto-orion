# Service supervision

There is no service manager or independent supervisor in the current kernel.
Services do not execute as isolated user processes, and IPC does not yet
provide process-exit teardown or blocking wakeups. The kernel therefore does
not restart a component after failure.

The future policy should use explicit states, a bounded restart budget,
progressive backoff, crash-loop transition to a terminal failed/degraded
state, and a new process/capability generation after restart. Restart must be
disabled where operations are not known to be idempotent or old DMA/resource
ownership cannot be safely revoked. Dependencies and dependent-service
behavior must be policy data, not inferred by the kernel.

These are design constraints only; there is no implemented supervisor,
restart counter, backoff timer, health check, or crash-loop test.
