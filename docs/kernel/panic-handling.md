# Panic handling — Phase 6

`orion_panic_at(module, reason)` is the central fatal path. It disables
maskable interrupts, checks a static recursion guard, attempts one `ERROR`
record with the failing module as origin and the reason as message, then calls
the x86-64 fatal backend. The backend loops over `cli; hlt`; it does not return
and does not request an automatic reset. `orion_panic(reason)` uses `CORE` as
the origin.

If panic recurs, it skips logging and halts immediately. If serial logging
fails, the return value is intentionally ignored because the system cannot
safely resume; no diagnostic delivery is guaranteed. There is no panic stack,
register dump, crash persistence, watchdog, or recovery supervisor.

Host tests link the panic code against mocks rather than executing privileged
instructions. They verify normal origin/reason reporting, default values,
logging failure, recursion handling, interrupt-disable requests, and the
non-returning halt contract via a controlled `longjmp` from the halt mock.
A separate test kernel invokes the real panic backend in QEMU; the harness
requires its fatal record and the VM to remain alive through the observation
interval. This does not exercise every panic site or prove serial availability
on physical hardware.
