# ORION OS — auditoria e estado do lote 2 (fases 11–20)

**Interpretação do lote:** “2” corresponde às fases 11–20, conforme o esquema
combinado.  
**Resultado atual:** Fase 11 parcial; Fase 12 parcialmente implementada para
threads do kernel; fases 13–20 continuam bloqueadas por pré-requisitos
técnicos. Nenhum subsistema futuro foi simulado nem declarado funcional sem
evidência.

## Estado atual após revalidação

O repositório e os gates posteriores foram verificados antes de concluir que
as fases não podem avançar com segurança:

| Área necessária | Estado observado |
|---|---|
| UEFI e kernel x86-64 | Boot passa no QEMU/EDK2. O kernel valida boot-info, inicializa subsistemas testados até o scheduler/capability self-test e permanece em idle. |
| GDT, IDT e exceções | Fase 7 validou #BP/#DE/#UD/#GP/#PF/#DF; a Fase 10 acrescentou apenas vector 32/PIT PIC IRQ0 e o testou em QEMU single-vCPU. Outros IRQs continuam mascarados. |
| Memória física e virtual | Fase 8 aceita no escopo de bootstrap. Mapa UEFI validado, PMM bitmap limitado a 4 GiB, testes host, mapeamento temporário no CR3 clonado e faults QEMU RO/NX; sem address spaces de processo ou isolamento de usuário. |
| Processos, threads e scheduler | Fase 9 aceita apenas para processo kernel único e threads cooperativas em ring 0; troca de contexto/round-robin passam teste QEMU. Não existe execução em modo usuário ou isolamento entre processos. |
| Capacidades e permissões | Parcial: gerenciador interno testado, integrado ao processo kernel e com handles/types/rights/revocation; sem user mode, isolamento ou mediação de operações reais de dispositivo/memória. |
| IPC | Parcial: canais limitados com mensagens copiadas, capabilities por endpoint e operações não bloqueantes; host tests e troca real entre kernel threads em QEMU. Sem Ring 3, bloqueio/despertar, transferência de capabilities em mensagens ou SMP. |
| Timer, relógio e sincronização | PIT/PIC IRQ0 e exclusão local do scheduler passam no teste da Fase 10; continuam ausentes relógio geral, timer queues, locks/atomics SMP e sincronização inter-CPU. |
| Drivers e descoberta PCI | Ausentes. O dispositivo FAT usado no boot é apresentado pelo QEMU/firmware; isso não é um driver ORION. |
| Armazenamento/VFS e persistência ORION | Ausentes. Não existe driver de bloco, filesystem ou updater do kernel. |
| Rede | Ausente. |
| Criptografia, assinatura e raiz de confiança | Ausentes. O loader não autentica criptograficamente o kernel; Secure Boot não foi configurado/testado. |
| Syscalls, ABI de usuário e loader de aplicações | Ausentes. Não há transição ring 3, endereço de processo ou executáveis de usuário. |
| GOP/framebuffer e entrada gráfica | Ausentes no contrato atual; sem teclado/mouse geridos pelo kernel. |

## Estado por fase

| Fase | Estado | Bloqueio específico e gate de saída |
|---|---|---|
| 11 — Capacidades e isolamento | PARCIAL | Host negatives e QEMU startup self-test validam API interna e lifecycle básico. Gate de isolamento efetivo permanece aberto: Ring 3, roots independentes, syscalls, teardown integrado de processos, APIs reais de objetos e sincronização SMP ausentes. |
| 12 — IPC | PARCIAL | Canais FIFO full-duplex, limitados e capability-protected; host negativos e tráfego entre duas kernel threads no QEMU. Sem wait/wakeup, caller user buffers, IPC de aplicações, cleanup de process exit ou SMP. |
| 13 — Supervisor de falhas | BLOQUEADA | Threads podem terminar cooperativamente, mas não há processos/serviços isolados, IPC ou supervisor. Panic do kernel não é contenção nem recuperação de serviço. Gate: fases 9, 11 e 12, além de supervisão de serviços. |
| 14 — Framework de drivers | BLOQUEADA | Há PMM, mapeamento ativo e timer legado de scheduler; faltam roteamento genérico de IRQ, sincronização, isolamento e recuperação necessários para drivers. Gate: fases 7–10 e infraestrutura de autoridade; iniciar com dispositivo virtual cuidadosamente delimitado. |
| 15 — Temporizadores e energia | BLOQUEADA | O PIT é usado apenas pela preempção single-core da Fase 10; faltam serviço de relógio geral, ACPI, deadlines e abstração de drivers. Gate: interfaces de timer e driver da Fase 14. |
| 16 — Armazenamento e filesystem | BLOQUEADA | Sem allocator, driver de dispositivo, timeout, sincronização ou framework de drivers. Não há dispositivo de bloco acessado pelo kernel. Gate: fases 8, 14 e 15. |
| 17 — Rede | BLOQUEADA | Sem buffers/alocador, drivers de rede, interrupções, timer, sincronização ou sockets/API. Gate: fases 8, 14–16 conforme a pilha definida, com testes de protocolo isolados. |
| 18 — Boot seguro e atualização | BLOQUEADA | Sem raiz de confiança, verificação de assinatura, criptografia, persistência ou mecanismo transacional de atualização. Hash isolado não atenderia aos critérios de autenticidade/rollback. Gate: escolher política de chaves e storage resiliente; validar firmware/secure-boot disponível. |
| 19 — Syscalls e aplicações | BLOQUEADA | Embora GDT/TSS/IDT de ring 0 e scheduler single-CPU existam, não há memória de usuário, CPL3, processos isolados, capabilities, IPC, loader de user ELF ou ABI. Gate: fases 8–12 e contrato de processo/address space testado. |
| 20 — Interface gráfica | BLOQUEADA | Sem GOP/framebuffer no boot-info, page mappings, scheduler/processos, drivers de display e entrada ou ambiente de aplicações. Gate: fases 8–9, 14, 19 e backend gráfico testado no QEMU. |

O estado “BLOQUEADA” nas fases 12–20 não significa que os objetivos sejam
impossíveis; significa que as precondições listadas ainda não existem e que
declarar implementação integrada seria incorreto. A Fase 11 está parcial:
documentar uma interface, adicionar tipos isolados ou produzir uma demo no host
não satisfaz o gate de isolamento efetivo.

## Testes executados nesta auditoria e revalidação posterior

Comandos executados no workspace Windows:

```text
mingw32-make test-unit build inspect test-boot test-panic-qemu test-exceptions-qemu validate-docs
```

Resultados reais:

- Unit tests boot-info/init/log/panic: PASS
- Build/inspect: ELF64 x86-64; nenhum símbolo indefinido reportado.
- QEMU/EDK2 normal: `UEFI-to-kernel boot verification: PASS`.
- QEMU panic e exceções: #BP, #DE, #UD, #GP, #PF e #DF foram observados com os
  vetores/erros documentados e caminhos fatais não retornáveis.
- `Document validation: PASS` após atualizar o estado da Fase 7.

Na revalidação anterior ao smoke test de page tables ativas, os testes host de
mapa, alocador e operações sintéticas passaram; `test-boot` validou o mapa
UEFI e o QEMU #PF registrou CR2 e os campos de erro decodificados. Na
revalidação final desta execução, `mingw32-make test-boot` passou com o smoke
test de map/write/lookup/unmap no CR3 clonado. `mingw32-make
test-exceptions-qemu` passou #GP, #UD, #DF, #PF não presente, #DE, escrita RO
(#PF 3) e fetch NX (#PF 17), em imagens isoladas observadas por 15 s cada.

Na execução da Fase 9, `mingw32-make test-unit build inspect
test-tasking-qemu` passou. O teste host cobriu capacidade/estado/stack e
invariantes; QEMU registrou oito eventos alternados dos IDs 2 e 3 (iterações
0–3), preservação de argumento/local, término e retorno ao bootstrap. A VM
ficou ativa por 15 s antes de ser encerrada pelo harness.

Os testes de Fase 11 validam a API interna de capabilities, incluindo testes
host negativos e um autoteste de boot QEMU. Os testes de Fase 12 validam
mensagens e tráfego entre kernel threads, mas não IPC Ring 3. Não há evidência
de recuperação, drivers, storage, rede, assinaturas, syscalls ou GUI.

## Evidência adicionada para a Fase 11

- `kernel/security/capability.h` e `capability.c`: implementação interna.
- `tests/kernel/task/task_test.c` e `kernel/core/main.c`: testes host e
  autoteste de inicialização QEMU.
- `docs/security/`: threat model, tipos/direitos, ciclo de vida, fronteiras,
  isolamento de memória e protocolo/limites de testes.

O gate da Fase 11 permanece parcial; fases 12–20 continuam bloqueadas pelas
dependências acima. As auditorias anteriores documentadas abaixo são
históricas e não representam o estado atual das capabilities.

## Evidência adicionada para a Fase 12

- `kernel/ipc/channel.h` e `channel.c`: endpoints de canal como capabilities,
  filas limitadas, cópia de payload e estados explícitos.
- `tests/kernel/ipc/ipc_test.c`: testes host de mensagens, direitos, filas,
  fechamento, isolamento entre canais e exaustão dos pools.
- `kernel/core/main.c`: autoteste QEMU em duas threads kernel com quatro
  mensagens sequenciadas.
- `docs/ipc/`: arquitetura, formato, lifecycle, autoridade, sincronização,
  limites e evidências.

A Fase 12 também é parcial: APIs bloqueantes, IPC entre processos de usuário,
transferência de capabilities como parte da mensagem, cancelamento e SMP
permanecem pendentes.

## Arquivos alterados nas auditorias anteriores

- `docs/phase-status-11-20.md` — este diagnóstico com bloqueios e gates.
- `README.md` — link para o estado do lote.
- `scripts/validate-docs.ps1` — valida presença dos estados das fases 11–20.

As threads kernel e o IPC interno ainda não desbloqueiam sozinhos as fases
13–20: faltam Ring 3, address spaces independentes, supervisor, drivers e
outros pré-requisitos. Nenhuma fase 13–20 foi marcada como concluída.

## Próximo caminho seguro

1. Fechar o gate de isolamento de processo e privilégio antes de expor
   capabilities ou IPC a código hostil.
2. Reauditar os gates de 13–20 antes de cada subsistema, sem pular dependências.

O lote 2 não está concluído. As fases 11 e 12 são parciais; fases 13–20
permanecem pendentes/bloqueadas. A implementação da Fase 7 remove apenas o
bloqueio específico de GDT/IDT para exceções CPU; não fornece user mode,
supervisor de serviços ou isolamento entre processos.
