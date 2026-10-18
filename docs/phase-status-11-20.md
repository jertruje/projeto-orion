# ORION OS — auditoria e estado do lote 2 (fases 11–20)

**Interpretação do lote:** “2” corresponde às fases 11–20, conforme o esquema
combinado.  
**Resultado atual:** Fases 11–17 parciais; Fase 12 implementada somente para
threads do kernel; Fase 13 parcial em diagnóstico de falhas, sem supervisor ou
recuperação de serviços; Fase 18 bloqueada, Fase 19 parcial e Fase 20 bloqueada.
Nenhum subsistema futuro foi simulado nem declarado funcional sem evidência.

## Estado atual após revalidação

O estado abaixo reúne evidência dos gates já exercitados. A Fase 17 avançou
para um caminho de rede parcial e a Fase 19 agora executa uma aplicação de
teste CPL3; as limitações de cada gate continuam explícitas:

| Área necessária | Estado observado |
|---|---|
| UEFI e kernel x86-64 | Boot passa no QEMU/EDK2. O kernel valida boot-info, inicializa subsistemas testados até o scheduler/capability self-test e permanece em idle. |
| GDT, IDT e exceções | Fase 7 validou #BP/#DE/#UD/#GP/#PF/#DF; a Fase 10 acrescentou apenas vector 32/PIT PIC IRQ0 e o testou em QEMU single-vCPU. Outros IRQs continuam mascarados. |
| Memória física e virtual | Fase 8 aceita no escopo de bootstrap. Mapa UEFI validado, PMM bitmap limitado a 4 GiB, testes host, mapeamento temporário no CR3 clonado e faults QEMU RO/NX; sem address spaces de processo ou isolamento de usuário. |
| Processos, threads e scheduler | Fase 9 aceita apenas para processo kernel único e threads cooperativas em ring 0; troca de contexto/round-robin passam teste QEMU. Não existe execução em modo usuário ou isolamento entre processos. |
| Capacidades e permissões | Parcial: gerenciador interno testado, integrado ao processo kernel e com handles/types/rights/revocation; sem user mode, isolamento ou mediação de operações reais de dispositivo/memória. |
| IPC | Parcial: canais limitados com mensagens copiadas, capabilities por endpoint e operações não bloqueantes; host tests e troca real entre kernel threads em QEMU. Sem Ring 3, bloqueio/despertar, transferência de capabilities em mensagens ou SMP. |
| Timer, relógio e sincronização | Fase 15 fornece clocksource coarse baseado em PIT IRQ0 e timer queue bounded com callbacks no idle context, host/QEMU testados. Sem sono/wakeup geral, locks/atomics SMP ou sincronização inter-CPU. |
| Drivers e descoberta PCI | Parcial: registry bounded, UART COM1, enumeração PCI no bus 0 e driver NE2000 PCI PIO de escopo único exercitados. Não há suporte geral MMIO/IRQ/DMA, nem vários modelos de NIC. |
| Armazenamento/VFS e persistência ORION | Parcial: block layer, ATA PIO primary-master LBA28, ORIONFS v1 e VFS interno. Host tests, ausência opcional em boot normal e persistência em dois processos QEMU passaram. Sem partições, journal, cache, atomicidade contra power loss, user descriptors ou SMP. |
| Rede | Parcial: driver PCI NE2000 em QEMU, Ethernet/ARP/IPv4/ICMP Echo/UDP; host provou round trip UDP real via slirp. Sem TCP, DHCP, IPv6, sockets de usuário, SMP ou NIC física testada. |
| Criptografia, assinatura e raiz de confiança | Ausentes. O loader não autentica criptograficamente o kernel; Secure Boot não foi configurado/testado. |
| Syscalls, ABI de usuário e loader de aplicações | Parcial: `int 0x80` x86-64, três syscalls, loader ET_EXEC limitado e uma aplicação independente QEMU. Sem handles, IPC/files/rede user-facing, múltiplos processos ou fault injection. |
| GOP/framebuffer e entrada gráfica | Ausentes no contrato atual; sem teclado/mouse geridos pelo kernel. |

## Estado por fase

| Fase | Estado | Bloqueio específico e gate de saída |
|---|---|---|
| 11 — Capacidades e isolamento | PARCIAL | Host negatives e QEMU startup self-test validam API interna e lifecycle básico. Gate de isolamento efetivo permanece aberto: Ring 3, roots independentes, syscalls, teardown integrado de processos, APIs reais de objetos e sincronização SMP ausentes. |
| 12 — IPC | PARCIAL | Canais FIFO full-duplex, limitados e capability-protected; host negativos e tráfego entre duas kernel threads no QEMU. Sem wait/wakeup, caller user buffers, IPC de aplicações, cleanup de process exit ou SMP. |
| 13 — Supervisor de falhas | PARCIAL: REGISTRO E POLÍTICA FAIL-STOP | Ring bounded e estático registra exceções CPU e panic; #BP retoma e as demais exceções testadas seguem para halt. Não há processos/serviços isolados, contenção de aplicação, restart, teardown IPC ou recuperação do kernel. Supervisor permanece bloqueado até lifecycle de processo/serviço e IPC seguro. |
| 14 — Framework de drivers | PARCIAL: REGISTRO, PCI E UART | Registry bounded com lifecycle, enumeração PCI mechanism #1 no bus 0 e UART COM1. A Fase 17 acrescenta somente o binding específico NE2000/PIO; sem bridge traversal/ECAM, BAR sizing/alocação, MMIO genérico, IRQ de dispositivo, DMA/IOMMU, hot-plug, user-mode driver ou SMP. |
| 15 — Temporizadores e energia | PARCIAL: PIT CLOCKSOURCE E DEADLINE QUEUE | PIT IRQ0 fornece nanosegundos monotônicos coarse; pool estático de 32 timers com cancelamento generacional, one-shot/periódico e callback fora da IRQ; host tests e expiração real QEMU passaram. Sem scheduler wait/wakeup, timeouts de IPC/drivers, ACPI, horário civil, eventos ou transições de energia. |
| 16 — Armazenamento e filesystem | PARCIAL: ATA PIO, ORIONFS E VFS INTERNO | Block layer bounded, ATA PIO LBA28 primary-master, filesystem ORIONFS v1 e fachada VFS; host tests e prova de persistência em dois boots QEMU com disco dedicado passaram. Sem partições, journal, cache, atomicidade contra interrupção de energia, user mode/FDs, wait/wakeup, timeout de dispositivo ou SMP. |
| 17 — Rede | PARCIAL: NE2000/PIO, IPv4, ICMP E UDP | Host tests cobrem checksums, ARP, IPv4, Echo, UDP, limites, malformed frames, ARP expiry, timeouts e falhas; QEMU 11.1.2/OVMF q35 detectou NE2000 `10ec:8029` e comprovou request/reply UDP byte-idêntico de 22 bytes entre host e guest. Sem TCP, DHCP, IPv6, user sockets, fila assíncrona, DMA de sistema, IRQ de dispositivo, SMP ou teste físico; ver `docs/network/networking.md`. |
| 18 — Boot seguro e atualização | BLOQUEADA | Sem raiz de confiança, verificação de assinatura, criptografia, persistência ou mecanismo transacional de atualização. Hash isolado não atenderia aos critérios de autenticidade/rollback. Gate: escolher política de chaves e storage resiliente; validar firmware/secure-boot disponível. |
| 19 — Syscalls e aplicações | PARCIAL | `int 0x80` DPL3 v1, `console_write`, `system_info`, `process_exit`, validação integral de buffers, ELF64 ET_EXEC limitado, processo de teste único, teardown de páginas e aplicação independente passam QEMU/OVMF. Sem handles/capabilities user-facing, IPC/files/rede, scheduler de processos, ASLR, assinatura, fault injection CPL3 ou portabilidade de firmware. Ver `user-mode.md`; gate restante inclui lifecycle/authority e testes de falhas. |
| 20 — Interface gráfica | BLOQUEADA | Sem GOP/framebuffer no boot-info, page mappings, scheduler/processos, drivers de display e entrada ou ambiente de aplicações. Gate: fases 8–9, 14, 19 e backend gráfico testado no QEMU. |

O estado “BLOQUEADA” nas fases 18 e 20 não significa que os objetivos sejam
impossíveis; significa que as precondições listadas ainda não existem. A Fase
19 está parcial e não deve ser confundida com um ambiente de aplicações
completo. A Fase 11 está parcial:
documentar uma interface, adicionar tipos isolados ou produzir uma demo no host
não satisfaz o gate de isolamento efetivo. A Fase 17 também é parcial: a prova
QEMU de um caminho de rede não representa suporte geral a dispositivos.

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
de recuperação ampla, assinaturas, syscalls ou GUI. As Fases 16 e 17 têm
evidência específica e escopo parcial descritos abaixo.

## Resultado da Fase 15

- `mingw32-make test-unit build inspect test-boot` passou após adicionar o
  clocksource PIT e a fila de timers. A inspeção manteve o kernel ELF64
  freestanding sem símbolos indefinidos.
- Os testes host `time/timers` passaram: source indisponível, conversões e
  overflow, monotonicidade/clamp, one-shot, periodic fixed-delay, cancelamento,
  stale handles, firing BUSY, callback failure, exaustão de slots e cleanup.
- QEMU realçou IRQ0 em timekeeping-only depois da etapa de preempção. A callback
  one-shot de 30 ms expirou; quantum nominal calculado 10.000.150 ns; lateness
  observada 453 ns; PIT ticks observados passaram de 7 para 10 no segmento
  timekeeping. Um sample não caracteriza jitter/precisão garantida.
- ACPI não foi lido: o boot-info não leva configuração UEFI/RSDP. Nenhum estado
  ou evento de energia é suportado; não há desligamento, retomada ou sleep de
  threads.
- Estado final da Fase 15: **PARCIAL**. Naquele gate não existia driver de
  bloco/filesystem. A Fase 16 avançou depois para o baseline síncrono e bounded
  descrito em seu resultado; wait/wakeup/timeouts continuam indisponíveis.

## Evidência adicionada para a Fase 11

- `kernel/security/capability.h` e `capability.c`: implementação interna.
- `tests/kernel/task/task_test.c` e `kernel/core/main.c`: testes host e
  autoteste de inicialização QEMU.
- `docs/security/`: threat model, tipos/direitos, ciclo de vida, fronteiras,
  isolamento de memória e protocolo/limites de testes.

Os gates das Fases 11 e 12 permanecem parciais. A Fase 13 adicionou registro
estruturado e política fail-stop, mas não contenção ou recuperação de serviços;
a Fase 16 permanece parcial, e a Fase 17 avançou depois para uma implementação
parcial de rede. A Fase 18 e a Fase 20 continuam bloqueadas por dependências
próprias; a Fase 19 está parcial. As auditorias
anteriores documentadas abaixo são históricas e não representam o estado atual
das capabilities.

## Resultado da Fase 16

- `kernel/storage/`: contrato bounded de dispositivo de bloco com validação de
  faixa, tamanho, alinhamento, disponibilidade e read-only.
- `kernel/drivers/storage/ata_pio.*`: primary-master ATA PIO, IDENTIFY, LBA28,
  polling com limite, setores de 512 bytes, leitura/escrita e cache flush.
- `kernel/fs/orionfs.*` e `kernel/vfs/`: ORIONFS v1 em bytes little-endian,
  superbloco com CRC32, bitmap, 64 inodes, diretórios, arquivos de até 3 KiB,
  handles generacionais e VFS interno síncrono.
- `mingw32-make test-unit build build/storage-test-kernel.elf` passou; testes
  host exercitam limites, corrupção, remount, I/O, fail-closed e operações de
  arquivo/diretório.
- `mingw32-make test-storage-qemu` passou em QEMU 11.1.2/OVMF q35: com disco
  dedicado de 16 MiB, o primeiro boot identificou 32.768 setores e gravou a
  prova; após iniciar novo processo QEMU com a mesma imagem, o segundo boot
  leu e comparou o conteúdo persistido. `mingw32-make test-boot` também passou
  sem disco, confirmando que storage ausente não impede o boot normal.
- A imagem de teste é criada e formatada apenas por ação explícita:
  `mingw32-make prepare-storage-tool` seguido de
  `scripts/prepare-storage-image.ps1 -Format`; substituir imagem existente
  exige `-Force`. O kernel não formata discos no boot.
- Limites: dispositivo legacy IDE primary-master apenas; sem MBR/GPT,
  descoberta geral de dispositivos, cache/journal, transações, garantia contra
  perda de energia, user mode/descriptors, concorrência SMP, timeout integrado
  ao clocksource ou integração com processos. A parada forçada do processo
  QEMU ocorre após flush/unmount no kernel e não é teste de power-cut.
- Estado final da fase: **PARCIAL**; suporte de armazenamento além do caminho
  QEMU testado e durabilidade sob falha abrupta continuam sem evidência.
  Detalhes e comandos: [storage/filesystem.md](storage/filesystem.md).

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

As threads kernel e o IPC interno não desbloqueiam por si as fases 18 e 20:
faltam raiz de confiança e subsistema gráfico. A Fase 19 tem apenas a base
single-process descrita em `user-mode.md`; as Fases 13–17 são parciais e
nenhuma delas foi marcada como concluída.

## Evidência da Fase 13

- `kernel/fault/report.*` guarda até 32 eventos em memória estática. Exceções
  CPU registram metadados estruturados; panic registra a ação fail-stop.
- `mingw32-make test-unit build inspect` passou, incluindo testes do ring,
  argumentos inválidos, overwrite e classificação do panic.
- `mingw32-make test-panic-qemu test-exceptions-qemu test-boot
  test-tasking-qemu test-preemption-qemu` passou. As imagens QEMU de exceção
  confirmaram IDs estruturados e o panic foi observado antes do halt.
- `mingw32-make validate-docs` e `git diff --check` passaram.
- Não foi demonstrado isolamento ou recuperação de processo/serviço; a Fase 13
  permanece parcial.

## Evidência da Fase 14

- `kernel/drivers/core/` contém registry bounded de 8 drivers/32 devices com
  matching exclusivo, estados e cleanup na falha. Testes host cobrem entrada
  inválida, dispositivo sem driver, init falha, cleanup, stop e invariantes.
- `kernel/drivers/bus/pci.*` enumera funções de bus 0 via PCI mechanism #1;
  testes host cobrem funções multifunção, IDs, classes, BAR metadata e
  argumentos fora dos limites. Na Fase 14 não havia escrita de configuração;
  a Fase 17 acrescenta apenas escrita do bit I/O decode no command register
  para o driver NE2000, sem sizing/alocação de BAR.
- `kernel/drivers/platform/uart16550.*` registra e inicia COM1 no boot real;
  reaproveita a UART serial do kernel e usa polling com timeout finito.
- Em QEMU 11.1.2/EDK2 `-machine q35 -m 256M -smp 1`, o log serial confirmou
  o driver COM1 e seis funções PCI em bus 0: 8086:29c0, 1234:1111,
  8086:10d3, 8086:2918, 8086:2922 e 8086:2930. A VM chegou a IDLE.
- `mingw32-make test-unit build inspect test-boot` passou.
- PCI discovery não é suporte funcional aos dispositivos enumerados. Apenas
  o NE2000/PIO foi ligado depois, na Fase 17. BARs continuam sem sizing ou
  alocação geral; faltam MMIO, IRQ routing, DMA, IOMMU, ECAM, bridge traversal,
  hot-plug e execução isolada de drivers.

## Próximo caminho seguro

1. Fechar o gate de isolamento de processo e privilégio antes de expor
   capabilities ou IPC a código hostil.
2. Reauditar os gates de 13–20 antes de cada subsistema, sem pular dependências.

O lote 2 não está concluído. As fases 11–17 são parciais; a Fase 18 e a Fase
20 permanecem bloqueadas e a Fase 19 é parcial. A implementação da Fase 19
fornece um caminho CPL3 de teste, mas não supervisor de serviços nem
isolamento de múltiplos processos.

## Resultado da Fase 19

- `kernel/syscall/` e `lib/orion/` definem ABI v1 com entrada real `int 0x80`,
  três serviços pequenos, erros assinados e validação de toda a faixa de cada
  buffer antes de cópia.
- `kernel/exec/elf64.*` aceita somente ELF64 ET_EXEC com limites, páginas
  não sobrepostas, W^X, entry executável e sem interpretador/dynamic/TLS. O
  loader cria páginas RX/RO/RW/NX e stack RW/NX em espaço de usuário clonado;
  falha explicitamente se PML4 slot 2 estiver ocupado.
- `apps/orion/` compila e liga separadamente uma aplicação independente que
  consulta system-info, escreve no console, exercita buffers inválidos e
  syscall desconhecida, e termina com código zero. O runner restaura CR3/RSP0,
  remove mappings, libera frames/tabelas e verifica invariantes do PMM.
- `mingw32-make build`, `mingw32-make build/kernel-elf64-test.exe` e
  `build/kernel-elf64-test.exe` passaram; o host parser test cobre formatos
  válidos/negativos. `mingw32-make test-user-mode-qemu` passou em QEMU
  11.1.2/OVMF q35, single-vCPU, com o log confirmando as duas mensagens da
  aplicação e o retorno após teardown.
- Esse teste não inclui exceção CPL3 injetada, capability/handle namespace de
  usuário, threads/process scheduling, IPC, arquivos, sockets, ASLR, assinatura,
  multi-vCPU, segundo firmware nem auditoria de isolamento. A Fase 19 permanece
  **PARCIAL**, não concluída; detalhes em [user-mode.md](user-mode.md).

## Resultado da Fase 17

- `kernel/drivers/net/ne2k_pci.*` implementa o binding PCI `10ec:8029` para
  NE2000/RTL8029. O dispositivo usa acesso PIO e memória interna da placa; não
  há DMA para RAM do kernel nem IRQ de dispositivo.
- `kernel/net/network.*` fornece interface genérica e buffers/limites fixos
  para Ethernet II, ARP, IPv4, ICMPv4 Echo e UDP. Endereço estático é
  configurado somente na imagem de teste. A ausência de NIC continua não fatal
  no boot normal.
- `tests/kernel/net/network_test.c` verifica vetores de checksum, ARP,
  validação IPv4/ICMP/UDP, checksum/length UDP inválidos, quadro truncado,
  limite e lifecycle dos endpoints, expiração ARP, timeouts finitos e erros de
  transmissão.
- `mingw32-make test-unit build inspect test-boot test-preemption-qemu` passou;
  `mingw32-make test-network-qemu` passou; `mingw32-make validate-docs` e
  `git diff --check` passaram. Diagnósticos do editor não reportaram erros nos
  arquivos C alterados.
- A integração QEMU 11.1.2/OVMF q35 detectou a NE2000 com MAC
  `52:54:00:12:34:56`, MTU 1500 e IP de teste `10.0.2.15/24`. O host enviou
  `ORION_PHASE17_UDP_ECHO` (22 bytes) a `127.0.0.1:40170`; o payload retornou
  idêntico do guest `10.0.2.15:40171`. O ICMP ping ao gateway `10.0.2.2`
  recebeu 1/1 resposta e perdeu zero pacotes. Amostras de RTT foram 0 e
  10.000.151 ns com PIT nominal de 10.000.150 ns; a resolução coarse não
  permite inferir latência sub-tick. Um snapshot após UDP registrou 3 TX, 3
  RX, zero descartes e zero erros.
- A regressão de armazenamento, ao usar a imagem padrão já persistida,
  encontrou a prova antiga na primeira inicialização e não satisfez a
  expectativa de uma imagem recém-formatada. O arquivo padrão foi preservado;
  o teste foi repetido com imagem descartável nova
  `build\orionfs-phase17-regression.raw`, preparada explicitamente com
  `-Format`, e as duas inicializações passaram.
- A fase permanece **PARCIAL**: TCP confiável, DHCP, IPv6, sockets de usuário,
  múltiplos dispositivos/interfaces, estado de link, SMP, suporte físico e
  outros NICs não foram implementados ou testados. Ver
  [network/networking.md](network/networking.md) e ADR-0028.
