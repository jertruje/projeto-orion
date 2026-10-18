# Process model — Phase 9

The implementation contains one static kernel process, ID 1. It owns the
linked list of kernel threads and a lifecycle state. Its `address_space` and
`resources` fields are reserved interfaces and remain null.

Processes are not independently schedulable; threads are. All current threads
run in ring 0 and share the inherited kernel mappings. There is no user-mode
transition, per-process page-table root, handle namespace, resource isolation,
process creation, or process termination. The process record is organization
only, not a security boundary.
