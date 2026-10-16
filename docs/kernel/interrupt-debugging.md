# GDB/QEMU exception debugging

The development ELF is built with `-g`. To start the existing QEMU launcher
with a GDB stub, add `-S -gdb tcp::1234` to its QEMU arguments (or launch the
same documented machine/OVMF/ESP arguments manually). In a second terminal:

```text
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break orion_x86_64_exception_dispatch
(gdb) continue
(gdb) info registers
(gdb) p/x $cr2
(gdb) x/16gx $rsp
```

GDB builds with architecture-specific support may expose descriptor-table
registers directly; otherwise use QEMU monitor `info registers` and inspect
GDTR/IDTR, then examine the table base and gate bytes with `x/` memory
commands. Inspect the `orion_x86_64_exception_frame` argument at the dispatcher
breakpoint. To avoid leaving a stopped VM, terminate only the dedicated test
instance after collecting the frame. These instructions are a debugging
procedure, not a separate test result.
