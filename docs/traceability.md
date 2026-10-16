# Rastreabilidade requisito → fase → teste

| Requisito | Fase | Documento/implementação atual | Evidência necessária / teste |
|---|---:|---|---|
| OR-PLAT-01 | 4–6 | `Makefile`, loader/kernel freestanding | `mingw32-make build`, symbols/undefined checks; repetir em ambiente limpo. |
| OR-BOOT-01 | 5 | `boot/uefi/loader.c`, `kernel/arch/x86_64/entry.S`, `kernel/core/main.c` | `mingw32-make test-boot`; serial espera ELF validado, ExitBootServices e kernel entry. |
| OR-BOOT-02 | 5, 8 | [boot-contract.md](boot-contract.md) | Static/layout checks e runtime validação de boot-info/mapa. |
| OR-CORE-01 | 6 | `kernel/core/main.c`, `include/orion/init_state.h`; sequência determinística com validação de boot-info e estado herdado de CPU/stack | `mingw32-make test-unit` cobre sequência/transições inválidas; `mingw32-make test-boot` observa sequência até idle em QEMU. |
| OR-CORE-02 | 6 | `kernel/console/serial.c`, `kernel/log/log.c`, `kernel/panic/panic.c`, `kernel/arch/x86_64/fatal.S`; logger limitado e panic fail-stop | `mingw32-make test-unit` cobre limites/saída simulada e panic normal/default/recursivo/log failure; `mingw32-make test-boot` e `test-panic-qemu` cobrem boot normal e panic fatal em QEMU. |
| OR-CPU-01 | 7 | `kernel/arch/x86_64/gdt.c`, `idt.c`, `interrupts.S`, `exceptions.c`; GDT/TSS/IDT próprias e IRQs externas desabilitadas | `mingw32-make test-exceptions-qemu` verifica #DE/#UD/#GP/#PF/#DF e código de erro; `test-panic-qemu` verifica #BP retornável; boot normal confirma sequência. |
| OR-MEM-01 | 8 | `kernel/mm/memory_map.*`, `physical.*`; somente descritores UEFI convencionais abaixo de 4 GiB tornam-se livres; todo o restante reservado | `mingw32-make test-unit` cobre mapa/overlap/alinhamento/reserva/alloc/free/exhaustion/invalid-free; `test-boot` valida mapa real, invariantes e allocate/release. |
| OR-MEM-02 | 8 | `kernel/mm/page_tables.*`; 4 níveis, 4 KiB, endereços canônicos 48-bit, W^X, callbacks de TLB/ownership; não ligado ao CR3 atual | `mingw32-make test-unit` cobre canonicality, map/lookup/unmap/flags/OOM rollback em pool sintético; falta teste de page-walk/mapping no CR3 ativo em QEMU. |
| OR-THR-01 | 9 | Sem thread structures ou context switch | Deterministic create/yield/return/termination test QEMU. |
| OR-SCHED-01 | 9 | Kernel idle loop somente | Teste round-robin, blocked/terminated e fila vazia. |
| OR-TIME-01 | 10 | Sem timer e controlador configurados | Teste interrupt/EOI/context preservation/preemption single-vCPU. |
| OR-SMP-01 | 10+ | `-smp 1` no teste atual; sem AP boot | `-smp N` QEMU, unique ownership, race/invariant checks. |
| OR-SEC-01 | 11+ | Não implementado; contratos candidatos em `architecture/security-architecture.md` | Negative tests de capability, memória e device authority. |
| OR-REL-01 | 13+ | Não há supervisor de serviço | Falha injetada, serviço saudável pós-recuperação, dados/invariantes checados. |
| OR-COMP-01 | 14+ | Nenhum driver ORION; boot usa bloco FAT emulado do QEMU | Inventário modelo/revisão e testes específicos por classe. |
| OR-BUILD-01 | 4+ | Makefile e toolchain local | Build repetido/independente, manifest de versões e hashes quando aplicável. |
| OR-TOOL-01 | 4 | `tests/toolchain/hello.c`, `tests/toolchain/linker.ld`, `scripts/test-toolchain.ps1` | `mingw32-make toolchain-test`: ELF64 x86-64, símbolo probe, nenhum símbolo indefinido e hashes repetidos iguais. |

O teste de boot pode passar sem satisfazer requisitos de isolamento,
interrupções, memória, scheduler ou compatibilidade. Uma linha desta tabela
não indica conclusão até que a evidência necessária esteja registrada.

## Rastreabilidade arquitetural da Fase 3

| Decisão/contrato | Requisitos e riscos | Documento | Fase / evidência que fecha |
|---|---|---|---|
| ADR-0001: alvo x86-64/UEFI/QEMU | OR-BOOT-01, OR-BOOT-02; R-08 | `architecture/system-overview.md`, `architecture/boot-sequence.md` | 5+: firmware físico e matriz de plataforma |
| ADR-0002: C/Assembly, Rust por caso | OR-PLAT-01; R-07, R-13 | `architecture/interfaces-and-contracts.md` | 4+: build/ABI, codegen/runtime e manutenção |
| ADR-0003: Clang/LLD/NASM | OR-PLAT-01, OR-BUILD-01; R-07 | `architecture/interfaces-and-contracts.md` | 4+: manifesto de toolchain e reprodução em host limpo |
| ADR-0004: loader UEFI próprio | OR-BOOT-01, OR-BOOT-02; R-01, R-08 | `architecture/boot-sequence.md`, `../boot-contract.md` | 5, 18: parser, autenticação, rollback e firmware |
| ADR-0006: boot-info/EBS boundary | OR-BOOT-02; R-02 | `architecture/boot-sequence.md`, `../boot-contract.md` | 8: mapa/reservas/lifetime auditados |
| ADR-0007: UART diagnostics | OR-CORE-02; R-03 | `architecture/kernel-boundaries.md`, `../kernel-core.md` | 6–7: UART failure, panic, concurrency and overhead tests |
| ADR-0008: gated IRQ/preemption/SMP | OR-CPU-01, OR-TIME-01, OR-SMP-01; R-03, R-04 | `architecture/scheduling-design.md`, `phase-gates.md` | 7–10: IDT/EOI/frame/context/preemption/multi-vCPU tests |
| ADR-0009: microkernel + capability-mediated authority baseline | OR-SEC-01, OR-REL-01; AR-01–04 | `architecture/system-overview.md`, `architecture/security-architecture.md`, `architecture/architecture-risks.md`, `research/architecture-comparison.md` | 7–14: implement/test boundaries; measure cost and containment; no superiority claim before evidence |
| ADR-0010: bounded copied control IPC baseline | OR-SEC-01, OR-REL-01; resource-exhaustion risk | `architecture/ipc-design.md` | 11–12: fuzz, quota/teardown negatives, latency/throughput percentiles; reconsider bulk data path after measurement |
| ADR-0011: cooperative bring-up, then preemptive single-CPU baseline | OR-THR-01, OR-SCHED-01, OR-TIME-01, OR-SMP-01 | `architecture/scheduling-design.md`, `architecture/memory-and-concurrency.md` | 7–10: exceptions, memory, context switch, preemption; SMP separately |
| ADR-0012: mechanisms in kernel, policy/services outside by default | OR-PLAT-01, OR-SEC-01, OR-COMP-01 | `architecture/kernel-boundaries.md`, `architecture/driver-model.md` | 11, 14: negative authority and resource mediation tests |
| ADR-0013: reserved-by-default physical frame bitmap baseline | OR-MEM-01, OR-MEM-02; AR-07 | `architecture/memory-and-concurrency.md`, `architecture/architecture-risks.md` | 8: map bounds, reservation, exhaustion, fragmentation and allocation-cost tests |
| ADR-0014: preloaded bootstrap bundle | OR-BOOT-01, OR-BOOT-02; AR-04–05 | `architecture/boot-sequence.md`, `architecture/component-model.md` | 5, 18–19: bundle bounds/authentication, rollback and first-task failure tests |
| ADR-0015: C/Assembly baseline; Rust case-by-case | OR-PLAT-01, OR-TOOL-01; R-07, R-13 | `language-decisions.md` | 4: ABI/target test passed; Rust remains unselected pending a justified component evaluation |
| ADR-0016: Windows MSYS2 Clang/LLD + GNU Make baseline | OR-PLAT-01, OR-BUILD-01, OR-TOOL-01; R-07 | `toolchain.md`, `development-environment.md`, `tests/toolchain/README.md` | 4: probe, ELF inspection, undefined-symbol check, repeated hash; second host/CI still open |
| ADR-0017: núcleo mínimo single-CPU e fail-stop | OR-CORE-01, OR-CORE-02; R-03 | `kernel-core.md`, `kernel/*.md` | 6: host tests, freestanding ELF inspection, normal and fatal QEMU boot; broaden only after interrupt/memory/synchronization gates |
| ADR-0018: GDT/TSS/IDT de exceções com IF fechado | OR-CPU-01; R-03, R-04 | `kernel/gdt.md`, `kernel/idt.md`, `kernel/exception-handling.md`, `kernel/interrupt-controller.md` | 7: QEMU tests of #BP/#DE/#UD/#GP/#PF/#DF; controller/IRQ delivery remains gated |
| ADR-0019: UEFI memory map + 4-GiB PMM; page tables sem CR3 migration | OR-MEM-01, OR-MEM-02; AR-07 | `../memory/memory-model.md`, `../memory/testing.md` | 8: live page walks and permissions in QEMU, safe table-frame resolution, then CR3 integration |
| Boot/service trust chain | OR-BOOT-01, OR-BOOT-02; R-01, R-02, R-08 | `architecture/boot-sequence.md`, `architecture/component-model.md` | 5, 18–19: signed root bundle, rollback and failure path tests |
| Memory ownership and address-space invariants | OR-MEM-01, OR-MEM-02; R-02, R-04 | `architecture/memory-and-concurrency.md` | 8: allocator/map/fault tests and race-safe reclamation |
| Recovery contracts | OR-REL-01; R-03, R-09 | `architecture/failure-recovery.md` | 13, 16, 18: crash injection, recovery health and data integrity |
| Phase dependency graph and gates | All phase requirements; all architecture risks | `architecture/phase-dependencies.md`, `phase-gates.md` | Review each gate before implementation; no inferred completion |
