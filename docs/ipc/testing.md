# IPC testing and evidence

## Host unit tests

`mingw32-make test-unit` now builds and runs `tests/kernel/ipc/ipc_test.c`.
It covers channel create/close, FIFO byte/size preservation, zero-length
messages, maximum-size rejection, empty/full queues, non-consuming
`BUFFER_TOO_SMALL`, invalid/forged capabilities, missing rights, remote close
with queued-message drain, channel independence, fixed-pool exhaustion and
invariants. Pool exhaustion exercises the no-heap resource-failure path.

## Kernel runtime

`mingw32-make test-boot` executes a boot self-test using two actual kernel
threads. They exchange four sequenced messages through the channel, yield to
each other, validate payload bytes and guards, then close both endpoints.
The serial result `Kernel-thread channel message exchange passed` is required.
This is runtime evidence for kernel-thread messaging in the one kernel process,
not process isolation or Ring 3 IPC.

## Limits

There are no true simultaneous CPUs, blocking/wakeup operations, user buffers,
syscalls, or application processes to test. The QEMU exchange uses cooperative
thread handoffs; it does not prove SMP safety or stress interrupt preemption
inside queue operations. No performance benchmark or protocol fuzz campaign
was performed.
