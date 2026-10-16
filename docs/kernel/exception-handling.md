# CPU exception handling — Phase 7

Stubs exist for vectors 0–31. The error-code stubs are vectors 8, 10–14, 17,
21, 29 and 30; the others push a synthetic zero so C receives a normalized
frame. Availability/meaning of newer architectural exceptions is CPU-feature
dependent; the code does not claim feature discovery.

The dispatcher logs vector and error code. Page Fault additionally reads CR2
and decodes the present/protection, write, user/supervisor, reserved-bit, and
instruction-fetch bits in its error code.
Breakpoint (#BP, vector 3) is logged and returns with `iretq`; other vectors
fail-stop through `orion_panic_at`. Double Fault (#DF, vector 8) uses TSS IST1.
The tests in `scripts/test-kernel-exceptions.ps1` isolate #DE, #UD, #GP, #PF
and #DF in separate QEMU runs and assert their serial evidence; a separate
test checks that #BP returns to the interrupted code.

There is no page-fault recovery, lazy mapping, fixup table, nested-fault
recovery, or complete register dump. A fatal exception does not return
normally.
