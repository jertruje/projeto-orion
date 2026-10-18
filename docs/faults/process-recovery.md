# Process and thread recovery

The current process subsystem contains exactly one kernel process. Kernel
threads can terminate through the scheduler with an exit code, and their
terminated state is reaped later by explicit thread destruction. This is a
normal lifecycle operation, not recovery from a fault.

The kernel has no Ring 3 process creation, independent address spaces,
faultable user exception containment, process teardown, automatic capability
revocation on process exit, or IPC cleanup hook. A fault raised in current
kernel code is not treated as an isolated application failure: CPU exceptions
other than the explicitly resumable breakpoint enter the panic/halt path.

Future process recovery must stop all affected threads, revoke capabilities,
close endpoints, release address-space mappings only after ownership is
proven, and notify dependents through an authorized mechanism. None of those
steps is currently implemented as a process-recovery transaction.
