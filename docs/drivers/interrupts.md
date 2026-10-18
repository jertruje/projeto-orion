# Driver interrupt contract

The current driver registry does not register per-device interrupt handlers.
Only the existing PIT/PIC timer path owns vector 32; its work is scheduler
preemption, not a generic device-driver callback. Other external IRQs remain
masked. The UART driver uses bounded polling with a finite transmit-ready
timeout and does not enable UART interrupts.

MSI/MSI-X, PCI INTx routing, shared IRQs, handler removal, deferred work and
device interrupt acknowledgement are not implemented. Future drivers must
keep hard interrupt work bounded and defer blocking/protocol work to a safe
thread context.
