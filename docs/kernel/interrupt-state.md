# Interrupt state policy — Phases 7 and 10

The loader/core enter with IF clear. GDT/IDT installation does not execute
`sti`. The normal idle loop continues to use `cli; hlt`; maskable interrupts
are temporarily enabled only inside the bounded PIT-preemption test, after the
IDT timer vector, PIC/PIT programming, acknowledgement and scheduler path are
ready. The PIC is masked and scheduler preemption disabled before idle.

Because NMI is not masked by IF, GDT installation temporarily masks NMI using
the PC-compatible CMOS/NMI control at port `0x70`, preserves the previous port
value, installs the GDT/TSS, then loads and verifies the IDT before restoring
that value. If descriptor setup fails, the kernel fail-stops with NMI still
masked rather than continue with a possibly inconsistent table pair.

`interrupt_state.h` provides query, disable, atomic save-disable and
conditional restore operations. Scheduler APIs protect queue mutations by
saving IF, executing with IF clear, then restoring the caller's state. The IRQ
handler performs no logging or allocation and sends PIC EOI before scheduling.
This is local interrupt exclusion, not a lock and not SMP synchronization.
NMI and synchronous CPU exceptions are distinct from maskable IRQs and are not
blocked by IF.
