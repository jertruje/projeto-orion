# Boot sequence and chain of trust

## Target sequence

```mermaid
sequenceDiagram
  participant FW as UEFI firmware
  participant L as ORION loader
  participant K as Kernel
  participant R as Bootstrap task
  participant S as Supervisor / device manager
  participant U as Other services and applications
  FW->>L: Load PE/COFF image
  L->>L: Verify manifest/signature and kernel image (future)
  L->>L: Load ELF and bootstrap bundle, collect memory map and boot data
  L->>FW: ExitBootServices with current map key
  L->>K: Enter documented x86-64 ABI
  K->>K: Validate boot data; initialize diagnostics, CPU, memory and interrupts
  K->>R: Create first user address space from reserved, validated boot bundle (future)
  R->>S: Start supervisor and device manager with attenuated capabilities
  S->>U: Start essential services and applications by dependency order
```

The events through kernel entry are the only portion exercised by the current
QEMU test. The selected design carries the initial task and minimum supervisor
in a loader-provided boot bundle, avoiding a dependency cycle in which a
filesystem service must load itself. Bundle format, signature policy and
rollback handling remain future trust-policy work. Creating the first user
task requires memory management, privilege transition and bundle validation.

## Stage contracts

| Stage | Input/output | Failure behavior | Integrity requirement |
|---|---|---|---|
| Firmware → loader | UEFI image handle, system/boot services | Report status on serial if possible, return error or halt | Firmware and Secure Boot policy are platform assumptions; current signature state unverified |
| Loader → kernel image and boot bundle | ELF, optional authenticated manifest, allocated load segments and reserved bundle pages | Reject malformed/unsupported artifact; do not transfer control | Bounds/overflow/overlap/entry checks; signature and rollback are future trust-policy requirements |
| ExitBootServices | Final memory map and current key | Retry only on stale key, bounded; otherwise stop | No boot service use after success |
| Kernel entry | Versioned boot info, stack, CPU execution state | Validate before dereference; fatal failure halts | ABI and reserved regions explicitly versioned |
| Kernel initialization | CPU state/stack, reserved memory, serial | Stage-specific panic; never claim failed module ready | Validate boot info; initialize safe exception path and allocator before page reuse; keep external interrupts disabled until handlers/controller/return path are verified |
| Root task | Validated executable from reserved boot bundle + minimal bootstrap caps | Halt/degraded mode; no untrusted fallback | Authentication/measurement and anti-rollback policy pending; current loader does not load this bundle |
| Service graph | Dependency manifest + capability grants | Fail closed; report degraded boot; bounded retry | Each service image/version and authority is auditable |

## Current limitations

Loader is own code and currently reads `kernel.elf` from a UEFI Simple File
System, validates a limited ELF header/segments, gets UEFI memory map and
calls `ExitBootServices`; the kernel reports boot info and idles. It does not
authenticate the ELF, enforce rollback, or establish an independent secure
root. Firmware, CPU features, runtime mappings and real hardware have not been
validated broadly.
