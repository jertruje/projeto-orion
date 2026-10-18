# Fault diagnostics

`orion_fault_record` stores an event identifier, category, execution context,
vector, CPU error code, instruction pointer, fault address, detail, process
and thread identifiers, chosen action, and recovery result. Unknown values
are zero. Text is not copied into the ring; the existing panic/logger path
retains the human-readable module and reason.

The ring has 32 statically allocated entries and does not use a heap. It
overwrites its oldest event at capacity and exposes a monotonically increasing
event identifier plus an overwritten-entry counter. Invalid records, missing
output pointers, unavailable event IDs and overwritten/not-found events have
explicit status results. Event IDs are never reused.

Writes and reads save/disable/restore interrupt state, appropriate only for
the current single-CPU kernel. The API detects reentrant access and reports a
corruption status rather than modifying the ring recursively. This mechanism
is neither durable nor an SMP synchronization primitive. Existing exception
and panic paths are integrated; other subsystems must opt in explicitly.
