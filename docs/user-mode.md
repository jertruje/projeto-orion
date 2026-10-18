# User-mode execution and system-call ABI

**Status:** narrow x86-64 implementation, tested in QEMU/OVMF only. This is
not a POSIX ABI, a stable compatibility promise, or evidence that the broader
microkernel/service architecture is complete.

## Current implementation

- The UEFI loader optionally reads `application.elf` from the EFI volume root.
  The file is bounded by `ORION_BOOT_APPLICATION_MAX_BYTES` (256 KiB) and is
  handed to the kernel through boot-info version 2. Missing application files
  do not affect ordinary boots; the dedicated Phase 19 test kernel rejects a
  missing or invalid image.
- The application is an ELF64 little-endian x86-64 `ET_EXEC` image. The kernel
  accepts up to eight non-overlapping `PT_LOAD` segments, at most 31 image
  pages plus one stack page, and a fixed application virtual window of 1 MiB.
  `PT_DYNAMIC`, `PT_INTERP`, `PT_TLS`, writable+executable segments, invalid
  bounds/alignment, and entries outside executable segments are rejected.
  Dynamic linking, relocations, TLS, ASLR and executable stack are unsupported.
- User mappings use the fixed base `0x0000010000000000` (PML4 slot 2), with
  the stack at the top of a separate 4 KiB page 2 MiB above the base. The
  address-space root clones the active kernel PML4 and requires slot 2 to be
  empty. If the inherited firmware/kernel mapping occupies that slot, process
  creation fails closed. Segment mappings enforce user permissions, NX and
  W^X; BSS and stack memory are zero-filled. The app test uses the linker
  script in `apps/orion/test_app.ld`.
- The image has no authentication or signature. Boot-info protects sizes and
  pointer/size consistency, not provenance. Only the named test image is
  loaded; there is no general filesystem-backed executable lookup.
- There is one synchronous test process (ID 2), no user thread scheduling,
  no concurrent address-space mutation, no SMP, and no user-facing capability
  or handle table. Its cloned root preserves inherited supervisor mappings;
  process isolation and safe direct-map assumptions require further review.

## ABI version 1

The syscall mechanism is x86 `int 0x80`, installed as a DPL3 interrupt gate.
The application cannot select a kernel callback address. `RAX` contains the
call number and return value; arguments are `RDI`, `RSI`, and `RDX`. Return
values are signed 64-bit integers. Negative error values are sign-extended.
The entry path saves the interrupt frame and runs with maskable interrupts
disabled. Except for the result in `RAX`, the interrupt frame is restored for
ordinary syscall returns.

| Number | Name | Arguments | Result and validation |
|---:|---|---|---|
| 1 | `console_write` | `RDI`: user buffer; `RSI`: byte count | Returns bytes written; maximum 256. Zero bytes succeeds without dereferencing the pointer. Nonempty buffers must be fully readable user memory. |
| 2 | `system_info` | `RDI`: pointer to output structure | Returns 0 after copying the full writable 40-byte structure; rejects any unmapped/non-writable byte. |
| 3 | `process_exit` | `RDI`: exit code | Does not return to user mode; restores the saved kernel CR3 and stack and returns the exit code to the single process runner. |

Error values:

| Value | Name | Meaning |
|---:|---|---|
| -1 | `INVALID_ARGUMENT` | Invalid length or argument |
| -2 | `BAD_ADDRESS` | User range is not mapped with required permissions |
| -3 | `NOT_SUPPORTED` | Unknown syscall or unsupported operation |
| -4 | `DEVICE` | Console device write failed |
| -5 | `PERMISSION` | Call came from an invalid privilege/context |
| -6 | `NO_MEMORY` | Reserved ABI value; current calls do not allocate user memory |

`system_info` contains two 32-bit fields (`abi_version`, `structure_size`)
followed by four 64-bit fields (`process_id`, `page_size`,
`maximum_write_bytes`, `feature_flags`), for a total of 40 bytes. Version 1
does not promise compatibility beyond the explicitly checked ABI version and
structure size. The matching application wrappers live in `lib/orion/syscall.h`
and `.c`.

All syscall buffers are range-checked page by page before access. The initial
implementation runs on a single CPU and does not alter mappings concurrently;
it does not yet provide a general fault-isolated copy primitive, pinning,
capability-checked object handles, or a syscall for files, IPC, networking,
memory allocation or yielding.

## Faults, process exit and cleanup

An exception delivered from CPL3 is recorded as a user CPU fault and the
current test process is marked terminated. The interrupt return sentinel
restores the saved kernel CR3 and stack. The process runner restores TSS RSP0,
unmaps user pages, releases their physical frames and page-table descendants,
then checks allocator invariants. Kernel-mode exceptions retain the existing
kernel fault/panic policy.

The ordinary process-exit path and mapping cleanup are verified by the QEMU
test below. CPL3 exception containment is implemented but not yet separately
fault-injected in QEMU. Cleanup for future file/IPC/device handles is absent
because those resources are not exposed through this ABI.

## Reproducible validation

```text
mingw32-make test-unit
mingw32-make test-user-mode-qemu
```

The host ELF tests cover valid images, bad identity/machine, truncated program
tables, file and virtual bounds, W+X, unreadable segments, overlapping load
segments, unsupported interpreter segments and an entry outside executable
memory. The QEMU test independently links `application.elf`, boots it through
UEFI, checks console output, system information, invalid pointers, an unknown
syscall, process exit status and physical-memory invariants after teardown.

Passing this gate demonstrates one user application and these three calls in
the named virtual machine configuration only. It does not demonstrate
multi-process isolation, user IPC/files/network access, service restart,
firmware-independent mappings, authenticated execution, POSIX/Linux
compatibility, SMP safety, or superiority over Linux.
