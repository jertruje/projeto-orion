# CPU exception handling

The x86-64 entry stubs normalize the vector and error-code slots before
building `orion_x86_64_exception_frame`. The structure is compile-time checked
against the assembly offsets and contains the saved general registers, vector,
error code, RIP, CS and RFLAGS. It does not claim to contain user RSP/SS for
the currently supported Ring 0 execution path.

Exception dispatch excludes timer vector 32 from fault reporting. For CPU
exceptions it stores a fixed-size report with vector, raw error code, RIP,
context derived from CS.RPL, and current kernel process/thread identifiers
when the current scheduler thread is known. Page fault vector 14 also records
CR2. Existing decoded page-fault logging remains for diagnosis.

Vector 3 is explicitly resumable and returns through `iretq`. All other CPU
exceptions take the panic path; the kernel does not attempt to repair page
faults or distinguish a recoverable user exception because Ring 3 is not
implemented. Double fault uses the configured IST stack; its handler still
halts through panic. The timer interrupt is a scheduler event, not a fault.

If structured storage cannot accept an event, the handler emits a best-effort
serial warning and follows the existing exception policy. The report API uses
static storage and single-core interrupt exclusion; this is not SMP-safe.
