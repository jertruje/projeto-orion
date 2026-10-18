# Thread lifecycle

States are `NEW`, `READY`, `RUNNING`, `BLOCKED`, and `TERMINATED`. Valid
transitions are centralized in `orion_thread_transition`:

| From | To |
|---|---|
| NEW | READY, RUNNING (bootstrap only) |
| READY | RUNNING |
| RUNNING | READY, BLOCKED, TERMINATED |
| BLOCKED | READY, RUNNING, TERMINATED |
| TERMINATED | none |

The scheduler uses `BLOCKED` only for the bootstrap thread while it waits for
its bounded ready batch; there is no general wait/wakeup API yet. A yielded or
timer-preempted thread becomes `READY` and is appended to the FIFO queue; a
returning entry is marked `TERMINATED` and is never queued again.

Termination does not free or reuse the executing stack. A different running
thread may later destroy a terminated thread. The static TCB slot is retired
for the rest of the boot, preventing an old pointer from aliasing a new thread;
thread IDs are monotonically assigned and never reused. This bounded strategy
trades capacity for simple lifetime safety.
