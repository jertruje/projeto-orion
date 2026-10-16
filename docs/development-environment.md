# Development environment

## Validated host

The environment inspected on 2026-10-16 is Windows x64 (PowerShell 5.1) with
portable MSYS2 installed at `%LOCALAPPDATA%\orion-msys64\msys64`. GNU Make
invokes Windows PowerShell scripts for tool checks and QEMU harnesses; Clang,
LLD, NASM and QEMU are native UCRT64 executables. The build is not running in
WSL and does not require Linux commands.

The Git repository exists on branch `master` but has no commits; existing
project content is untracked. Do not assume a clean checkout or a recorded
history. `.gitignore` ignores generated `build/` outputs only.

## Available tools observed

- Clang/LLVM/LLD 23.1.3
- NASM 3.02
- GNU Make 4.4.1
- QEMU 11.1.2
- GDB 18.1
- `python.exe` resolves to the zero-byte Microsoft Store app-execution alias;
  a functional Python interpreter was not confirmed and no Python-based Phase
  4 validator is required.
- `rustc` and `cargo` were not found; Rust remains optional and is not needed
  for the selected C/Assembly baseline.
- QEMU UEFI firmware files were checked by the existing toolchain checker.

## Commands

Run from the repository root in PowerShell:

```powershell
mingw32-make check-tools
mingw32-make toolchain-test
mingw32-make build
mingw32-make inspect
mingw32-make test-unit
mingw32-make validate-docs
```

`toolchain-test` uses `scripts/test-toolchain.ps1` and only requires Clang,
`ld.lld`, `llvm-readobj` and `llvm-nm`. `build`, `inspect` and `test-boot` use
the existing project toolchain and QEMU/EDK2 files. No package installation is
performed by these commands.

## Known environment limitations

- The current toolchain was observed on one host; no second-host build has
  been performed.
- The worktree has no Git commit, so builds cannot yet be tied to an immutable
  source revision.
- Rust is unavailable and therefore has not been evaluated in the environment.
- QEMU/EDK2 validates the emulator setup only; it does not certify physical
  hardware support.
