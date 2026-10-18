# Component model and lifecycle

## Component identity and state

Each future process/service has an immutable process identity during its
lifetime and a generation number for restart. Handles belong to a process
namespace and reference typed kernel objects; raw pointers and numeric device
IDs are never authority. A capability carries object type, rights, optional
resource/size limits and lifecycle state. A process cannot mint additional
rights; derivation may only reduce rights. Revocation invalidates all derived
references according to an explicitly tested object-generation scheme.

Initial lifecycle proposal:

`CREATED -> LOADING -> READY -> RUNNING -> STOPPING -> EXITED`

Failure can transition a process from `LOADING`, `READY`, or `RUNNING` to
`FAILED`, then cleanup revokes handles, unmaps address spaces and reclaims
threads only after they cannot execute. Restart creates a new generation;
old handles must fail rather than resolve to the replacement.

## Components targeted

- **Kernel mechanism:** CPU state, memory ownership/mapping, thread scheduling,
  bounded IPC, capability validation, low-level interrupt/time/device
  mediation, panic diagnostics.
- **Root bootstrap:** trusted first user task; exact source/signature and safe
  bootstrap authority remain open. It only creates the minimum supervisor and
  device-manager capability set.
- **Service supervisor:** starts, monitors and can restart only named
  non-kernel services; restart uses bounded backoff and health contracts.
- **Device manager:** enumerates platform description, matches signed/approved
  driver manifests, grants MMIO/IRQ/DMA subsets; cannot silently expand a
  driver's authority.
- **Drivers:** separate process per failure domain where viable. Requests are
  typed, bounded, cancellable and versioned.
- **Storage, network, UI and applications:** clients of documented service
  protocols. No user application receives raw device access by default.

## Bootstrapping and cycles

The kernel cannot require filesystem service to load the filesystem service.
Therefore an initial boot bundle or equivalent trusted bootstrap source must
contain the root task and minimum initial services. The loader currently loads
only `kernel.elf`; artifact packaging, signature verification, manifest format
and root-task creation are unresolved. These are explicit blockers, not implied
features.

Proposed ordering:

1. Kernel validates boot information and initializes serial/panic.
2. Kernel establishes CPU exception path and physical/virtual memory.
3. Kernel creates the first address space and root task from a verified bundle.
4. Root task starts supervisor and a minimal device manager using pre-created
   bootstrap capabilities.
5. Device manager starts needed drivers; storage service starts only after its
   block driver is healthy.
6. Network and UI start after their device/service dependencies.
7. Applications start with their declared, reduced capability set.

Steps 2–7 are target behavior and must be revised against actual code.

## Storage, networking, applications and presentation

- **Storage:** a user-mode storage/filesystem service owns filesystem policy
  and recovery metadata; a block driver receives only its assigned device
  capability. Writes that claim durability must report success only after the
  selected device/cache contract says data is persistent. Journal, COW or
  another format is not selected here; power-loss semantics and tests choose
  it in the storage phase.
- **Network:** parsing untrusted frames/packets and protocol policy stay outside
  the kernel by default. Device buffers, receive queues, retries, sockets and
  per-client quotas are bounded. No transport protocol or name service is
  selected without implementation requirements and evidence.
- **Applications:** each application receives a documented, versioned API and
  only declared file/network/device capabilities. No compatibility with Linux
  binaries is assumed. Stable ABI is not promised until API versioning,
  lifecycle and compatibility tests exist.
- **Presentation:** compositor, window management and user interaction are
  user-mode policy/services over mediated display/input resources. A GUI is not
  a kernel subsystem and cannot be an early boot dependency.

These rules establish trust ownership, not implementation status. The Phase 19
baseline now has a deliberately narrow CPL3 syscall ABI and one independently
linked QEMU test application; it does not yet expose filesystem, network,
capability or IPC objects to user programs and is not the service lifecycle
described above. A GUI is not implemented.
