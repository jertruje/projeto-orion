# Power states and transitions

No ACPI system power state is implemented. ORION does not request S3 suspend,
S4 hibernation, S5 shutdown, reboot, or resume. It has no ACPI event handler,
thermal policy, battery service, device quiescence protocol or restart path.

The idle loop uses the x86 `hlt` instruction while interrupts are enabled.
IRQ0 can wake that CPU execution context. This is an idle instruction in the
current single-vCPU kernel; it is not evidence of ACPI state discovery,
system suspend, device power management or a recoverable suspend/resume
transition.

No power transition should be added until ACPI data is safely discovered,
device lifecycle and interrupt ownership are defined, and the transition has
a platform-specific failure and resume procedure tested in QEMU and on
supported hardware.
