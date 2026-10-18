# ORION OS — estado verificável das fases 1–11

**Escopo deste registro:** evidência registrada para fases 1–11.
**Estado geral:** INCOMPLETO. Fases 1–10 passaram somente nos escopos
limitados descritos abaixo. A Fase 11 tem uma base interna de capabilities
implementada e testada, mas não fornece isolamento de usuário e não atende
integralmente ao gate de segurança.

## Evidência observada no workspace

- Na inspeção inicial, o workspace não continha `.git`; continha o prompt
  `projeto orion.txt`, que foi preservado. O repositório está agora na branch
  `master`, com `HEAD` em `17e1093`. Esta execução não cria commit; as
  atualizações documentais finais permanecem no worktree.
- Foi instalado MSYS2 portátil em
  `%LOCALAPPDATA%\orion-msys64\msys64`. O ambiente possui Clang/LLVM/LLD,
  binutils, NASM, GDB, QEMU/EDK2, GNU Make e GNU-EFI. A versão gravada
  e confirmada nesta fase: Clang/LLVM/LLD 23.1.3, NASM 3.02, GDB 18.1,
  QEMU 11.1.2 e GNU Make 4.4.1. O target do Clang é
  `x86_64-unknown-unknown-elf`. Python funcional e Rust não foram detectados;
  nenhum é dependência crítica da build/teste selecionado.
- `mingw32-make build` compilou o loader PE/COFF e kernel ELF64.
- `mingw32-make test-boot` foi executado após corrigir a descoberta de
  Simple File System. Resultado observado: `UEFI-to-kernel boot
  verification: PASS`. A serial mostrou kernel ELF validado, segmentos
  reservados, saída dos serviços UEFI, entrada no kernel e início do loop
  mínimo. A observação registrada na última repetição foi 15 s; o script encerra
  QEMU deliberadamente, portanto isso não é teste de estabilidade prolongada.
- Na auditoria pré-alteração da Fase 6, faltavam testes unitários isolados de
  logger/panic, verificação explícita do estado CPU/stack e validação robusta
  dos estágios de inicialização. A fase implementou esses itens dentro dos
  limites documentados.
- O núcleo atual valida boot-info e o estado de entrada x86-64 (pilha,
  long-mode/paging, PAE, IF), usa logger serial limitado, panic fail-stop e
  loop `cli; hlt`. A Fase 7 agora carrega GDT/TSS/IDT próprios, mantém IF
  desabilitado e passou por testes QEMU isolados das exceções suportadas.
- A Fase 8 foi validada no escopo de memória de bootstrap: mapa UEFI, alocador
  físico de páginas de 4 KiB e operações de page-table em quatro níveis,
  exercitadas em um PML4 clonado instalado no CR3 e em falhas QEMU de páginas
  RO/NX. Isso não implementa isolamento de processos, user mode ou um layout
  virtual independente; a identidade física/virtual do pool estático só foi
  observada em QEMU/EDK2.
- A Fase 9 implementa um processo kernel organizacional e threads kernel com
  troca cooperativa round-robin single-CPU, pilhas separadas e teste de
  alternância real em QEMU. Não há processos de usuário, timer, preempção,
  IRQs externos ou suporte SMP.
- A execução de `git status` e `git rev-parse` antes da inicialização do
  repositório retornou “not a git repository”. A ferramenta Git está
  disponível. Nenhum commit ou histórico anterior foi observado.

## Matriz de fases

| Fase | Estado | Evidência/pendência objetiva |
|---|---|---|
| 1 — Especificação | CONCLUÍDA, baseline inicial | Requisitos, alternativas, ameaça, métricas, recuperação, comparação Linux, riscos, decisões e rastreabilidade registrados. `scripts/validate-docs.ps1` passou com 13 documentos obrigatórios, 16 IDs rastreados e fases 1–10 presentes. Decisões são iniciais e algumas provisórias. |
| 2 — Pesquisa técnica | APROVADA como pesquisa documental com limites explícitos | Seis sistemas comparados em 20 eixos com estados de evidência; fontes primárias/indisponibilidades registradas; Linux classificado por origem; segurança/prova formal analisadas; benchmark público seL4 documentado com condições/limites; plano futuro; 27 entradas EVD. Sem benchmark ORION ou comparação cruzada homogênea. |
| 3 — Arquitetura | CONCLUÍDA como baseline arquitetural documental | Arquitetura-base microkernel/capabilities selecionada; contratos de kernel/serviços, autoridade, IPC, memória, escalonamento, drivers, recuperação, boot, interfaces e dependências documentados. ADR-0009–0014 fixam o baseline, com gates de implementação e revisão. Nenhum subsistema arquitetural foi implementado nesta fase. |
| 4 — Toolchain e repositório | CONCLUÍDA no gate do probe independente | Ferramentas/targets/flags documentados; probe C freestanding linkado como ELF64 x86-64; inspeção LLVM confirmou formato/arquitetura/símbolo, `llvm-nm` não encontrou símbolos indefinidos, e duas saídas produziram SHA-256 idênticos. Build existente, unit tests e validação documental passaram. Sem segundo host/CI; nenhum kernel/bootloader foi implementado nesta fase. |
| 5 — Boot UEFI | CONCLUÍDA no alvo QEMU mínimo | Teste serial QEMU confirmou a cadeia loader→ELF kernel, `ExitBootServices` e execução do kernel. Robustez do parser ELF, hardening e hardware físico continuam limitados. |
| 6 — Núcleo do kernel | CONCLUÍDA no escopo mínimo documentado | Módulos e contratos documentados; sequência monotônica, validação boot-info/CPU/stack; testes host de estado, logger e panic; ELF64 x86-64 sem símbolos indefinidos; QEMU normal chega a `IDLE`; panic real testado em imagem separada. Limites em `docs/kernel/testing.md`. |
| 7 — GDT, IDT e exceções | CONCLUÍDA no escopo CPU-exception, IRQs externas desabilitadas | GDT/TSS e IDT próprias carregadas e lidas de volta; IST1 de double fault; stubs 0–31 com normalização/código de erro; interfaces IF query/cli/sti presentes (sti não chamado); QEMU isolado confirmou #BP com retorno, #DE, #UD, #GP + erro 1034, #PF + CR2, e #DF por falha encadeada de entrega; IF continua desabilitado. Sem PIC/APIC, IRQ externa, user mode ou proteção por paginação. |
| 8 — Memória física e virtual | CONCLUÍDA NO ESCOPO DE BOOTSTRAP | Mapa UEFI validado; PMM com bitmap reservado por padrão (4 GiB); testes host de alloc/free/exhaustion/invalid-free e page tables; smoke QEMU de map/write/lookup/unmap após clonar o PML4 e instalar o CR3; faults QEMU RO (#PF 3) e NX (#PF 17). Sem user isolation, demanda, SMP shootdown ou validação fora de QEMU/EDK2; detalhes em `docs/memory/`. |
| 9 — Processos e threads | CONCLUÍDA NO ESCOPO COOPERATIVO DE KERNEL | Processo kernel único como contêiner organizacional; pool bounded de TCBs/stacks, estados validados, fila FIFO round-robin, troca SysV AMD64 cooperativa, trampoline/retorno controlado e destruição pós-switch. Host tests e QEMU provam alternância real, argumentos, stack-local preservada, término, fila vazia e invariantes. Sem user mode/isolamento, criação de processos, timer/preempção, general block/wakeup ou SMP. |
| 10 — Timer, preempção e multicore | CONCLUÍDA NO ESCOPO DE PREEMPÇÃO SINGLE-CORE; MULTICORE BLOQUEADO | PIT channel 0 + PIC IRQ0, vector IDT 32, EOI antes de scheduling, IF-safe queue updates e FIFO preemptivo; QEMU `-smp 1` prova workers CPU-bound sem yield, contexto/stack, fila vazia e métricas TSC/ticks. Sem AP startup, SMP synchronization, APIC/IOAPIC ou hardware físico testado. |
| 11 — Capabilities e isolamento | PARCIAL, TESTADA NO ESCOPO INTERNO DO KERNEL | Tabelas bounded por processo/objeto/slot/lease; handles locais com gerações; type/right checks; derivação e transferência em subset; revogação global e destruição adiada por lease. Host tests e self-test QEMU passam. Sem Ring 3, address spaces independentes, syscalls, device/memory mediation ou SMP; isolamento efetivo pendente. |
| 12 — IPC | PARCIAL, TESTADO PARA THREADS DO KERNEL | Canais full-duplex limitados, filas FIFO copiadas e capabilities SEND/RECEIVE/QUERY/CLOSE; host negatives e troca real entre duas kernel threads em QEMU. Não há bloqueio/despertar, IPC Ring 3, transferência de capabilities em mensagens, cleanup de processo ou SMP. |

## Comandos de build e teste

Executados no workspace Windows com o toolchain portátil:

```text
mingw32-make test-unit build inspect
  Resultado: exit code 0; testes boot-info/init-state, logger e panic passaram;
  kernel ELF64 x86-64 linkado com --no-undefined; inspeção LLVM realizada;
  llvm-nm --undefined-only não reportou símbolos; sem warnings de compilação.

mingw32-make test-boot
  Resultado: exit code 0; QEMU 11.1.2 atingiu CPU_STATE_VALID, CORE_READY,
  IDLE e a mensagem de entrada no idle loop; permaneceu em execução por 15 s
  antes de o harness encerrar a instância; UEFI-to-kernel boot verification: PASS.

mingw32-make test-panic-qemu
  Resultado: exit code 0; kernel de teste separado carregado pelo loader,
  panic registrou "[ORION][TEST][ERROR] QEMU panic path reached", QEMU
  permaneceu em execução por 15 s em cli;hlt e foi encerrado pelo harness;
  QEMU panic-path verification: PASS.

mingw32-make test-exceptions-qemu
  Resultado: exit code 0; instâncias QEMU separadas registraram #GP (vetor 13,
  erro 1034), #UD (6, erro 0), #DF (8, erro 0 via IST1), #PF (14, CR2
  140737488351232) e #DE (0, erro 0); todas entraram em panic fail-stop e
  permaneceram ativas por 15 s até parada deliberada pelo harness.

mingw32-make test-unit build inspect test-boot test-panic-qemu test-exceptions-qemu validate-docs
  Resultado: exit code 0; testes unitários passaram; kernel ELF64 x86-64 e
  loader EFI inspecionados; llvm-nm --undefined-only não reportou símbolos;
  boot normal, breakpoint, panic e exceções QEMU passaram; cada instância de
  teste foi observada por 15 s; validação documental PASS (69 arquivos,
  17 requisitos, 17 ADRs, 27 evidências, 20 linhas de comparação,
  64 links locais, 4 diagramas Mermaid).

mingw32-make check-tools
  Resultado: exit code 0; Clang/LLD 23.1.3, NASM 3.02 e QEMU 11.1.2
  encontrados pelo verificador do projeto.

mingw32-make toolchain-test
  Resultado: exit code 0; Clang 23.1.3 compilou e LLD ligou dois ELF64
  x86-64 independentes; llvm-readobj confirmou formato, arquitetura,
  endereçamento de 64 bits, tipo executável e símbolo orion_toolchain_probe;
  llvm-nm não encontrou símbolos indefinidos.
  SHA-256 de ambos: ce479f365f40c4a673ebabb3a84e8f04011bf115f8922b31e4bf83667faa115d
  Toolchain probe: PASS.

mingw32-make build inspect test-unit validate-docs
  Resultado: exit code 0; build e inspeção existentes passaram;
  Boot-info unit tests: PASS;
  Document validation: PASS (46 files, 17 requirements, 15 ADR definitions,
  56 local links, 4 Mermaid diagrams, phases 1-26 present).

mingw32-make build
  Resultado: exit code 0; loader EFI e kernel ELF foram compilados/linkados.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\test-boot.ps1
  -ToolchainHome %LOCALAPPDATA%\orion-msys64\msys64 -ObservationSeconds 15
  Resultado: exit code 0; UEFI-to-kernel boot verification: PASS.

mingw32-make test-unit build inspect test-boot test-panic-qemu test-exceptions-qemu validate-docs
  Resultado: exit code 0; testes host de mapa/PMM/page tables passaram;
  kernel ELF64 x86-64 sem símbolos indefinidos; QEMU validou 99 descritores
  UEFI, reportou 51.972 páginas livres abaixo de 4 GiB no ambiente de teste,
  passou smoke test de allocate/release e permaneceu no idle pelo período
  configurado; o teste #PF registrou CR2, error code 0 e cinco bits
  diagnósticos. Panic QEMU e validação documental também passaram (78
  documentos, 18 ADRs, 65 links locais). Isso não demonstra page-table walks
  do CR3 ativo.

mingw32-make test-unit build inspect test-tasking-qemu
  Resultado: exit code 0; testes host incluíram limite de threads, stacks,
  transições inválidas e invariantes; ELF64 x86-64 sem símbolos indefinidos;
  QEMU 11.1.2 permaneceu 15 s no teste. A trace serial confirmou as oito
  execuções `2,3,2,3,2,3,2,3`, com iterações 0–3 por thread; ambas terminaram
  com exit code 0, a pilha bootstrap retomou e as invariantes passaram.
  QEMU round-robin alternation trace: PASS. Sem preempção ou benchmark.

git init
  Resultado na inspeção inicial: repositório criado em
  C:\Users\danim\Downloads\projeto orion\.git; branch inicial master, sem
  commits; arquivos existentes apareciam como untracked. Estado posterior:
  HEAD 17e1093, sem commit criado nesta execução.
```

As verificações acima refletem o estado pós-alteração. Build e testes devem
continuar sendo executados após alterações futuras.

Durante o desenvolvimento dos testes de exceção, um primeiro arranjo de #DF
gerou #NP (vetor 11) e a primeira página escolhida para #PF estava mapeada pelo
ambiente UEFI. Esses resultados não foram contados como testes aprovados; os
casos foram ajustados para provocar/observar #DF de verdade e usar um endereço
canônico não mapeado. O panic test também teve uma tentativa inicial com ESP
isolado que não iniciou o loader; o harness final usa o ESP validado, exige
serial não vazia, restaura o kernel normal mesmo em falha e observa cada VM
por 15 s. Os alvos finais passaram.

**Medições até o fechamento da Fase 9:** nenhum benchmark de desempenho foi
executado nessa etapa. Os testes de exceção são checks negativos de
confiabilidade/baixo nível, não uma auditoria de segurança abrangente. Não há
isolamento de usuário, capabilities ou paginação própria para testar. Os
diagnósticos temporais da Fase 10, registrados abaixo, não são benchmarks de
desempenho nem comparação com Linux.

## Relatório da Fase 2

**Estado: APROVADA como pesquisa documental com limites explícitos.** A matriz
compara Linux, seL4, Fuchsia, Genode, MINIX 3 e SerenityOS em 20 eixos e marca
células com evidência insuficiente/não verificada em vez de inventar
conclusões. `evidence-register.md` contém 27 entradas rastreáveis. A pesquisa
registra resultados formais do seL4 por configuração, mecanismo MINIX de
heartbeat/restart e um microbenchmark IPC seL4 com plataforma, payload,
unidade, estatística e limites; não há benchmark ORION nem comparação
cross-OS homogênea.

Fontes sem conteúdo técnico utilizável ou acesso denegado foram registradas em
`docs/research/phase-2-evidence.md` e no registro. A conclusão não reivindica
superioridade, não decide arquitetura e não transforma mecanismos
documentados em resultados de confiabilidade. Nenhum código de kernel ou de
terceiros foi incorporado nesta fase.

Documentos de pesquisa revisados: `phase-2-evidence.md`,
`evidence-register.md`, `linux-limitations.md`, `architecture-comparison.md`,
`security-analysis.md`, `performance-analysis.md`,
`driver-and-hardware-compatibility.md`, `failure-recovery.md`,
`benchmark-plan.md`, `research-gaps.md` e `phase2-conclusions.md`.

```text
PowerShell -NoProfile -ExecutionPolicy Bypass -File scripts\validate-docs.ps1
  Resultado: exit code 0.
  Document validation: PASS (46 files, 17 requirements, 15 ADR definitions,
  27 evidence entries, 20 comparison rows, 56 local links, 4 Mermaid diagrams,
  phases 1-26 present).
```

## Relatório da Fase 3

**Estado: CONCLUÍDA como arquitetura-base documental.** Os 13 documentos
arquiteturais, a matriz de alternativas, a rastreabilidade e os gates foram
revisados e harmonizados. ADR-0009 seleciona microkernel mínimo com
capabilities; ADR-0010–0012 fixam IPC controlado, progressão do escalonador e
placement de mecanismos/política. Essas decisões orientam implementação, mas
não alegam propriedades de segurança ou desempenho já demonstradas. A pesquisa
documental da Fase 2 foi posteriormente aprovada com limites e lacunas
registrados; isso não representa evidência de implementação.
Nenhum código de kernel foi implementado na Fase 3.

### Arquivos arquiteturais produzidos

- `docs/architecture/system-overview.md`
- `docs/architecture/kernel-boundaries.md`
- `docs/architecture/component-model.md`
- `docs/architecture/security-architecture.md`
- `docs/architecture/ipc-design.md`
- `docs/architecture/memory-and-concurrency.md`
- `docs/architecture/scheduling-design.md`
- `docs/architecture/driver-model.md`
- `docs/architecture/failure-recovery.md`
- `docs/architecture/boot-sequence.md`
- `docs/architecture/interfaces-and-contracts.md`
- `docs/architecture/architecture-risks.md`
- `docs/architecture/phase-dependencies.md`
- `docs/phase-gates.md`

Também foram atualizados `docs/decision-log.md`, `docs/traceability.md`,
`docs/architecture-options.md`, `docs/research/phase-2-evidence.md`,
`docs/phase-status.md`, `README.md` e `scripts/validate-docs.ps1`.

### Decisões, alternativas e riscos

- ADR-0001–0004, 0006–0008 formalizam decisões de plataforma, linguagem,
  toolchain, loader, boot-info, diagnóstico e gates de interrupção; ADR-0005
  fica substituída pela ADR-0009.
- ADR-0009 seleciona microkernel/capabilities como baseline; monolítico,
  híbrido e capabilities como mecanismo combinável foram comparados sem
  pontuação arbitrária.
- ADR-0010 seleciona IPC de controle síncrono, copiado e limitado como baseline.
- ADR-0011 seleciona bring-up cooperativo em single-CPU e baseline futuro
  preemptivo single-CPU; SMP permanece um gate separado.
- ADR-0012 mantém mecanismos no kernel e policy/services em user mode por padrão.
- ADR-0013 seleciona bitmap reservado por padrão para o allocator físico inicial.
- ADR-0014 seleciona bundle pré-carregado para inicializar a primeira tarefa sem
  ciclo de dependência com o filesystem.
- Custos de IPC, revogação de capabilities, IOMMU/DMA, bootstrap de user task,
  confiança de boot, allocator e compatibilidade/plataformas continuam riscos
  abertos com revisões/experimentos registrados em
  `docs/architecture/architecture-risks.md`.

### Validação executada

```text
PowerShell -NoProfile -ExecutionPolicy Bypass -File scripts\validate-docs.ps1
  Resultado: exit code 0.
  Document validation: PASS (39 files, 16 requirements, 13 ADR definitions,
  49 local Markdown links, 4 Mermaid diagrams, phases 1-26 present).
```

A validação conferiu documentos obrigatórios, IDs de requisitos na
rastreabilidade, linhas de estado das fases 1–26, unicidade das definições de
ADR, presença dessas ADRs na rastreabilidade, fechamento de code fences e
existência dos links locais. Não foi feito build do kernel para uma alteração
exclusivamente documental, nem implementado código nesta fase. O comando foi
executado em 2026-10-16 nesta retomada da Fase 3.

### Critérios de aceitação

| Critério | Estado | Observação |
|---|---|---|
| Alternativas arquiteturais avaliadas | Atendido documentalmente | Comparação qualitativa expandida sem pontuação numérica; limites de evidência anotados. |
| Modelo recomendado justificado | Atendido documentalmente | ADR-0009 seleciona microkernel/capabilities por alinhamento aos requisitos, sem alegação de superioridade. |
| Limites de privilégio e confiança | Atendido documentalmente | `security-architecture.md` define modelo de autoridade e limites; implementação futura. |
| Subsistemas de kernel/serviços separados | Atendido documentalmente | Placement e regra para exceções em `kernel-boundaries.md`. |
| IPC, memória e escalonamento definidos | Atendido como baseline | Cópia síncrona de controle; ownership/VM; cooperação inicial e preempção planejada; ainda não implementados. |
| Drivers, recuperação e boot definidos | Atendido documentalmente | Lifecycle, DMA, falhas e bundle inicial descritos; trust policy permanece aberta. |
| Interfaces e dependências coerentes | Atendido documentalmente | Contratos e grafo registrados; bundle/formato permanecem gates de implementação. |
| Riscos registrados | Atendido documentalmente | Riscos residuais, revisões e gates em `architecture-risks.md`. |
| Decisões rastreáveis a requisitos/evidências | Atendido documentalmente | ADRs conectadas a requisitos, fontes, riscos e fases de validação. |
| Validações documentais executadas | Atendido | `PowerShell -NoProfile -ExecutionPolicy Bypass -File scripts\validate-docs.ps1` passou nesta retomada; 39 arquivos, 16 requisitos, 13 ADRs, 49 links locais e 4 diagramas Mermaid. |
| Contradições críticas | Atendido com limitações registradas | Baseline arquitetural separado da implementação e dos resultados ainda não medidos. |

A implementação futura deve seguir os gates e revisar decisões somente com
evidência de testes ou medições.

## Restrições e riscos ainda abertos

1. O parser ELF e o carregador UEFI são código inicial próprio e precisam de
   revisão de limites/overflow, validação de segmentos e fuzzing antes de
   serem tratados como seguros.
2. O kernel preserva um mapa UEFI, mas não possui ainda alocador nem gestão
   própria de page tables; não inferir que a memória esteja protegida.
3. O QEMU usa TCG/EDK2 e um dispositivo FAT virtual; isso não prova
   compatibilidade com firmware ou hardware físico.
4. A serial e o loop `hlt` não exercitam concorrência, exceções, IPC ou falhas
   de serviços.
5. Benchmark do ORION OS, comparação válida com Linux e métricas de segurança
   continuam “não medidos”.

## Relatório da Fase 4

**Estado: CONCLUÍDA para o gate de toolchain definido em
`docs/phase-gates.md`.** A fase valida ferramentas e a cadeia de compilação
com um probe independente; não implementa nem declara validado qualquer
subsistema de kernel/bootloader.

O hash idêntico comprova igualdade bit a bit somente entre essas duas
compilações, neste host, com a toolchain e os arquivos observados. Não
comprova builds do kernel reproduzíveis entre hosts/versões, segurança,
desempenho ou boot do probe. O teste não exigiu QEMU. O teste de boot já
registrado pertence à validação anterior da Fase 5.

Decisões de linguagem e toolchain estão registradas nas ADR-0015 e ADR-0016.
Requisitos, ADRs e comandos estão rastreados em `docs/requirements.md`,
`docs/decision-log.md`, `docs/traceability.md`, `docs/toolchain.md` e
`docs/development-environment.md`. A ferramenta Python resolve para um alias
zerado da Microsoft Store e não foi confirmada como interpretador funcional;
nenhum validador Python foi necessário. `rustc`/`cargo` não foram encontrados
e não são necessários.

Não há segundo host, CI ou pinagem imutável de versões; são pendências de
portabilidade/release, não bloqueios do gate local da Fase 4. Nenhum arquivo de
implementação do kernel ou bootloader foi alterado.

## Próximos passos necessários para encerrar o lote 1–10

- A Fase 2 está aprovada como pesquisa documental; lacunas de implementação,
  benchmark cruzado e hardware permanecem gates posteriores.
- A Fase 4 está concluída no escopo do gate local; manter segundo host/CI e
  pinagem de versões como pendências de portabilidade/release.
- Fortalecer o contrato de boot/parser e adicionar testes de parser e boot-info.
- Fases 6–9 e a preempção single-core da Fase 10 têm evidência nos escopos
  limitados registrados acima.
- Antes de declarar multicore concluído, implementar AP topology/startup,
  per-CPU state, local interrupt controllers, SMP-safe ownership/TLB policy e
  repetir os testes com múltiplas vCPUs.

## Relatório da Fase 10

**Estado:** CONCLUÍDA NO ESCOPO DE PREEMPÇÃO SINGLE-CORE; MULTICORE
BLOQUEADO, NÃO IMPLEMENTADO.

- **Política:** FIFO round-robin cooperativa e preemptiva, uma fila por boot
  single-CPU. Um PIT IRQ0 rota a thread corrente quando existe outro ready.
- **Timer:** PIT channel 0, divisor 11.932; PIC 8259 remapeado para 0x20/0x28,
  somente IRQ0 não mascarado durante o teste; vector IDT 32 e EOI antes de
  preempção. Frequência nominal ~99,9985 Hz (~10,00015 ms), não medida em
  hardware físico.
- **Preempção observada:** dois workers CPU-bound terminaram >=1.000.000
  iterações cada, sem `yield`; QEMU verificou timer ticks, preempções, stack,
  término e o retorno do timer sem threads prontas. O serial verifier exige
  os valores não-zero e os limites.
- **Multicore:** uma vCPU testada. AP startup/topology, per-CPU scheduler e
  sincronização inter-CPU faltam; `-smp 2` não foi habilitado e não se declara
  suporte funcional.
- **Medições:** número de IRQs/preempções e runtime ticks por thread; TSC
  virtual para amostras do dispatcher PIT sem reschedule e handoff médio do
  scheduler. O TSC não tem frequência calibrada, inclui overhead de medição e
  não fornece latência física nem comparação de desempenho.
- **Uma execução da suíte final:** 7 PIT ticks, 3 timer-driven preemptions, 4
  amostras sem troca (média 104.778 e máximo 262.663 TSC virtual cycles),
  2.003 dispatches no benchmark de handoff (média observada 6.633 TSC cycles
  por dispatch), e progresso de 1.191.960/1.000.000 iterações nos dois
  workers. Esses números são evidência de funcionamento nessa execução QEMU,
  não limites, distribuição estatística ou benchmark comparativo.
- **Comando dedicado:** `mingw32-make test-preemption-qemu`.
- **Pendências:** APs/SMP, APIC/IOAPIC e IRQs de dispositivo, estados FPU/SIMD,
  cobertura longa e hardware físico.
- **Fase 11:** implementação interna parcial; critérios dependentes de Ring 3,
  address spaces e isolamento efetivo permanecem pendentes.

## Relatório da Fase 11

**Estado:** PARCIALMENTE IMPLEMENTADA E TESTADA; NÃO APROVADA COMO ISOLAMENTO
DE PROCESSOS OU SEGURANÇA COMPLETA.

- Foram adicionadas tabelas estáticas bounded para 8 processos, 32 slots por
  processo, 64 objetos e 64 leases.
- Handles opacos validam tabela/proprietário, geração da tabela, slot, geração
  do slot, geração/estado do objeto, tipo e direitos. Slots são aposentados
  antes de wrap.
- Derivação e transferência exigem direitos explícitos e só permitem
  subconjuntos. Revogação remove todos os slots do objeto; leases já obtidos
  permanecem válidos até release e adiam o destrutor.
- Inicialização/invariantes estão integradas ao processo kernel. Criação
  inicial de objetos é kernel-only.
- Testes host e autoteste de inicialização QEMU cobrem concessão válida,
  negativas de tipo/direitos/handle, falsificação/reuso, derivação,
  transferência, limites, leases, revogação, destrutor diferido e invariantes.
- `mingw32-make test-unit build inspect` passou. A execução combinada de
  `mingw32-make test-boot test-tasking-qemu test-preemption-qemu
  test-panic-qemu test-exceptions-qemu` passou; a serial QEMU incluiu
  `Capability checks and revocation passed`. O harness encerra cada VM após
  sua janela observacional.
- Nenhuma medição de desempenho foi feita. Não houve teste de concorrência SMP,
  acesso entre processos, CPL3 ou páginas protegidas a partir de usuário.
- Dependências seguintes: terminar isolamento de address spaces e Ring 3 antes
  de declarar capabilities como fronteira contra processos hostis; Fase 12
  deve construir IPC sobre autoridade validada, com quotas, validação de
  mensagens e teardown.

## Relatório da Fase 12

**Estado:** PARCIALMENTE IMPLEMENTADA E TESTADA; NÃO É IPC DE APLICAÇÕES.

- O kernel possui canais full-duplex com endpoint capabilities, filas FIFO
  estáticas e cópia bounded de mensagens opacas.
- Operações não bloqueantes fornecem resultados explícitos para fila vazia/
  cheia, peer fechado, endpoint encerrado, buffer curto, argumento inválido,
  autorização negada, handle inválido e exaustão.
- Cada payload aceita 0–256 bytes; cada endpoint mantém até 8 mensagens.
  Existem 8 canais globais (32 KiB de payload máximo no conjunto); a criação
  não usa heap e reporta esgotamento atomicamente.
- `SEND`, `RECEIVE`, `QUERY` e `CLOSE` são direitos explícitos de endpoint.
  Fechar invalida todas as cópias daquele endpoint; mensagens já destinadas
  ao peer aberto podem ser drenadas antes de `PEER_CLOSED`.
- A sincronização salva/desabilita/restaura IF em seções curtas, válida apenas
  para o kernel single-core. Não há wait queues ou bloqueio/despertar; as
  operações nunca esperam ocupadamente.
- `mingw32-make test-unit build inspect` passou, incluindo testes de direitos,
  payloads, fila, lifecycle, exaustão e isolamento entre canais.
- `mingw32-make test-boot` passou com duas threads reais do kernel alternando
  quatro envios/recebimentos e validando os bytes em runtime QEMU.
- Não foram testados user buffers, processos independentes, bloqueio/
  despertamento, transferência de capabilities dentro de mensagens,
  concorrência SMP ou desempenho.
- Fase 13 depende de lifecycle de processo/serviço e supervisor; canal fechado
  gera erro explícito, mas não reinicia componentes nem garante recuperação.
