# Freestanding toolchain probe

This is a host-independent compiler/linker validation fixture. It is **not**
the ORION kernel, a bootloader, or a runnable operating system. The C function
and linker script exist only to verify the x86-64 ELF toolchain without CRT,
libc, operating-system syscalls, or QEMU.

From the repository root on the supported Windows setup:

```powershell
mingw32-make toolchain-test
```

The harness builds the source twice in separate directories using
`--target=x86_64-elf`, links with `ld.lld` and the fixture linker script,
inspects ELF headers/program headers/symbols with `llvm-readobj`, confirms the
probe symbol exists, rejects undefined symbols with `llvm-nm`, and compares
SHA-256 hashes of both executables. Artifacts are written below
`build/toolchain-test/`; the repository's `clean` target removes `build/`.

The harness requires Clang, LLD, `llvm-readobj`, and `llvm-nm`. It does not
require QEMU, firmware, GDB, or Rust. `ORION_TOOLCHAIN_HOME` may point to the
MSYS2 installation; otherwise the script uses
`%LOCALAPPDATA%\orion-msys64\msys64`.

A matching hash establishes byte-for-byte equality for these two builds in
the observed toolchain and environment only. It does not establish
reproducibility across hosts, compiler versions, or the actual kernel build.
