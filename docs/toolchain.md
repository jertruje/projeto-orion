# ORION OS toolchain

**Validated configuration:** Windows x64 host, PowerShell 5.1, portable MSYS2
UCRT64 tools under `%LOCALAPPDATA%\orion-msys64\msys64`, using Clang/LLVM,
NASM, GNU Make and QEMU/EDK2. Versions listed here were observed on
2026-10-16; they are environment observations, not a pinned dependency
manifest.

## Selected tools

| Purpose | Selected tool | Observed version/status | Rationale |
|---|---|---|---|
| C compiler | Clang | 23.1.3 | Cross-target support for the UEFI PE/COFF target and `x86_64-elf`; one compiler family supports both ABIs. |
| Kernel linker | LLD `ld.lld` | 23.1.3 | Explicit ELF x86-64 linking with an ORION linker script and no host CRT. |
| UEFI linker | `lld-link` | Installed; version is checked by `check-tools` | Emits EFI PE/COFF for the current Windows-hosted toolchain. |
| Assembly | NASM | 3.02 | Explicit syntax for existing low-level UEFI assembly. Clang integrated assembler is also used for `.S`. |
| Artifact inspection | `llvm-readobj`, `llvm-nm` | LLVM 23.1.3 | Inspect file format, architecture, sections, program headers, symbols and undefined symbols. |
| Build orchestration | GNU Make | 4.4.1 | Existing project build graph and repeatable named targets. |
| Emulator/firmware | QEMU + EDK2 | QEMU 11.1.2; firmware files present | Existing boot-test environment; not required by the standalone Phase 4 toolchain probe. |
| Debugger | GDB | 18.1 | Available for future target debugging; no GDB session is claimed in this phase. |
| Rust | `rustc`, `cargo` | Not found | Optional only; absence is not a blocker because Rust is not selected for initial kernel code. |

## Targets and ABI boundaries

- Kernel C: `--target=x86_64-elf`, ELF64, x86-64, freestanding, statically
  linked by `ld.lld` with `kernel/linker.ld`.
- UEFI loader C: `--target=x86_64-pc-windows-msvc`, PE/COFF x64 and the
  Windows x64/UEFI calling convention, linked with `lld-link`.
- NASM UEFI assembly: `-f win64`; `.S` kernel assembly is passed through
  Clang's integrated assembler with the ELF target.
- The target does not imply that an ELF file is automatically bare-metal.
  Freestanding compilation controls compiler assumptions and startup/runtime
  expectations; the linker, ABI, symbols, memory layout and actual runtime
  dependencies must also be verified.
- A host-target executable uses the host ABI and OS services. A freestanding
  object can still be host-targeted. Bare-metal kernel code targets a declared
  machine ABI and must provide or avoid startup/runtime services. User
  applications that depend on ORION syscalls/runtime are a separate target
  and are not built by the current toolchain test.

## Kernel compiler flags

The current build uses:

```text
--target=x86_64-elf -std=c11 -O2 -Wall -Wextra -Werror
-ffreestanding -fno-stack-protector -fno-pic -fno-builtin
-mno-red-zone -mcmodel=small -g
```

- `-ffreestanding` removes hosted-environment assumptions; it does not provide
  startup code or prove the artifact has no runtime dependencies.
- `-fno-stack-protector` is used until a compatible kernel stack-check runtime
  is deliberately implemented.
- `-fno-pic` matches the current fixed-address static kernel link; revisit if
  relocation or KASLR is added.
- `-fno-builtin` avoids compiler-emitted assumptions about hosted library
  routines while no kernel runtime is established.
- `-mno-red-zone` prevents use of the x86-64 SysV red zone, which is unsafe
  across kernel interrupt/exception entry unless explicitly preserved.
- `-mcmodel=small` is consistent with the current low physical link address;
  it must be revised if the virtual layout exceeds its addressing assumptions.
- `-Wall -Wextra -Werror` makes warnings fail the build; `-g` retains
  development debug information and does not imply runtime support.
- The test fixture uses the same ABI-sensitive flags except `-g`, since
  omitting debug paths permits a meaningful repeated-artifact hash check.

No hosted standard library, CRT, or operating-system syscall library is linked
by the standalone test. The fixture-specific linker script defines only an
inspection image and is not the kernel linker script.

## Reproduction

On the validated Windows setup:

```powershell
mingw32-make check-tools
mingw32-make toolchain-test
mingw32-make inspect
mingw32-make validate-docs
```

Set `ORION_TOOLCHAIN_HOME` or pass `TOOLCHAIN_HOME=...` to Make to use another
MSYS2 installation with the required files. Build/test outputs are under
`build/`. `mingw32-make clean` removes that generated build directory only.

The toolchain test is standalone and does not boot the fixture. Its two
same-environment outputs are SHA-256 compared; that is not evidence of
cross-host or cross-version reproducibility, nor does it establish that the
kernel build is bit-for-bit reproducible.
