# Kernel module contracts — Phase 6

| Module | Public interface / input | Result and invariants | Dependencies and failure behavior |
|---|---|---|---|
| Entry (`entry.S`) | `_start`, loader-provided first C argument | One entry path; interrupts disabled; `RSP` uses the reserved static stack before C call | Calls `kernel_main`; unexpected return halts. Relies on loader-provided x86-64 execution mode. |
| Boot contract (`boot_info.h`) | `orion_boot_info_is_valid(const orion_boot_info *)` | Boolean structural validation, including signature/version, non-null aligned map, descriptor sizing and map-size consistency | Does not prove arbitrary pointer accessibility or map semantic correctness. Invalid data causes core panic. |
| Init state (`init_state.h`) | `orion_init_advance(current, expected, next)` | Changes state only for a valid, exact next-stage transition; invalid operations leave state unchanged | Header-only; core treats a rejected transition as fatal. |
| Serial (`serial.c`) | Initialization, bounded text write, single character output | Polling COM1-compatible output; bounded transmit wait; returns failure when write cannot complete | No detection of physical UART presence; logger propagates errors, core/panic cannot guarantee delivery. |
| Logger (`log.c`) | Level, NUL-terminated module and message; optional `uint64_t` | Bounded synchronous record; `false` on invalid input, truncation or I/O failure | Serial; no allocation, lock, or reentrancy guarantee. Core treats required log failure as fatal. |
| Panic (`panic.c`) | Module and reason strings | Never returns; suppresses recursive log attempt and reaches halt | Logger and x86-64 fatal backend. Output is best effort only. |
| CPU entry validator (`cpu_state.S`) | Static stack bottom/top addresses | Boolean result for checked inherited CPU/stack state | Privileged CR/MSR reads; called only after boot-info validation and in QEMU/kernel context. Failure causes panic. |
| Fatal backend (`fatal.S`) | Disable-interrupts operation and non-returning halt | IF is cleared; halt remains in a deterministic loop | x86-64 privileged instructions; host tests provide mocks. |
| IF controls (`interrupt_state.S`) | Query current IF; disable or enable maskable interrupts | Explicit `bool` query and `cli`/`sti` operations; enabling is not used during bring-up | x86-64 privileged instructions; IF remains clear until a controller/handler return path is validated. |

All C interfaces require valid pointers to readable, NUL-terminated strings
where their signatures accept strings. There is no generic subsystem registry
or dynamic initialization framework.
