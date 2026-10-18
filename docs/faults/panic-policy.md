# Kernel panic policy

`orion_panic_at` disables interrupts, prevents recursive panic handling,
attempts to append a `KERNEL_PANIC` record, writes the module/reason through
the bounded serial logger, and enters the architecture halt loop. The
structured event marks `HALT_KERNEL` and `NOT_ATTEMPTED`; it does not claim
recovery.

The panic path does not allocate memory, acquire scheduler locks, free
resources, restart services, or resume execution. If structured recording
fails, the panic still proceeds to the existing serial diagnostic and halt.
Nested panic skips further logging and halts immediately.

The report ring is only best-effort volatile evidence. A wedged UART, damaged
kernel stack, CPU/hardware failure, or double fault may prevent complete
diagnostics. No controlled reboot path is implemented.
