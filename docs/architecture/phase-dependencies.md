# Architecture dependency graph

```mermaid
flowchart TD
  F1[Fase 1 requirements] --> F2[Fase 2 evidence review]
  F2 --> F3[Fase 3 architecture ADR]
  F3 --> F4[Toolchain and reproducible build]
  F4 --> F5[UEFI to kernel]
  F5 --> F6[Kernel core]
  F6 --> F7[GDT IDT exceptions]
  F7 --> F8[Physical and virtual memory]
  F8 --> F9[Threads and cooperative scheduler]
  F9 --> F10[Timer preemption; SMP separately gated]
  F8 --> F11[Capabilities and isolation]
  F9 --> F12[IPC]
  F11 --> F12
  F7 --> F13[Failure supervisor]
  F9 --> F13
  F12 --> F13
  F10 --> F14[Driver framework]
  F11 --> F14
  F14 --> F15[Time and power services]
  F14 --> F16[Storage]
  F15 --> F16
  F14 --> F17[Network]
  F16 --> F17
  F18[Verified boot and updates] --> F21[Package manager]
  F16 --> F21
  F19[Syscalls and user applications]
  F11 --> F19
  F12 --> F19
  F16 --> F19
  F19 --> F20[GUI]
  F14 --> F20
  F11 --> F19
  F21 --> F24[Release candidate]
  F22[Security/reliability audit] --> F24
  F23[Comparable benchmarks] --> F24
  F20 --> F24
```

Fase 18 (verified boot/update) can proceed as policy/specification in parallel,
but package installation requires its integrity mechanism and actual storage.
Fase 22 is an audit of implemented code and should repeat at milestones. Fase 23
must benchmark only subsystems that exist. The graph indicates dependencies,
not status or guaranteed sequencing; detailed status is in
[`../phase-status.md`](../phase-status.md) and
[`../phase-status-11-20.md`](../phase-status-11-20.md).
