# Interrupt state policy — Phase 7

The loader/core enter with IF clear. GDT/IDT installation does not execute
`sti`; the kernel idle loop continues to use `cli; hlt`. Exception gates cover
CPU faults, but this does not make external interrupts safe.

Because NMI is not masked by IF, GDT installation temporarily masks NMI using
the PC-compatible CMOS/NMI control at port `0x70`, preserves the previous port
value, installs the GDT/TSS, then loads and verifies the IDT before restoring
that value. If descriptor setup fails, the kernel fail-stops with NMI still
masked rather than continue with a possibly inconsistent table pair.

`interrupt_state.h` provides explicit query, disable, and enable operations.
The enable operation is intentionally not called anywhere in this phase. IF
must remain clear until an external controller is configured, every enabled
vector has a valid handler and acknowledgement path, stack/frame/return
behavior is audited, and the system has a safe policy for interrupt-time
logging and synchronization.
NMI and synchronous CPU exceptions are distinct from maskable IRQs and are not
blocked by IF.
