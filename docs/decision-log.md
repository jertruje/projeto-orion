# Registro de decisões

Estados: **proposta** = hipótese a revisar; **aceita** = decisão aprovada com
escopo; **substituída** = decisão histórica. Nenhuma entrada deste registro
substitui dados de teste.

| ID | Estado | Decisão | Razão / alternativas | Consequência e evidência pendente |
|---|---|---|---|---|
| ADR-0001 | Proposta | Primeiro alvo x86-64, UEFI, QEMU/EDK2. | Reflete escopo solicitado e permite boot automatizado; BIOS legado e hardware amplo ficam fora do bring-up inicial. | Boot testado em QEMU; validar hardware/firmware distintos futuramente. |
| ADR-0002 | Proposta | C freestanding e Assembly; avaliar Rust apenas por caso de uso. | Toolchain já consegue compilar C/ASM; Rust adiciona target/runtime/build questions. | Medir ABI, runtime, código gerado e custo de manutenção antes de adoção. |
| ADR-0003 | Proposta | Usar Clang `--target=x86_64-elf`/LLD para kernel e target Windows MSVC/LLD para UEFI EFI PE/COFF. | Compilador instalado e artefatos reais foram produzidos. | Reproduzir versões/builds e auditar ABI; dependência do caminho MSYS2 atual. |
| ADR-0004 | Proposta | Loader UEFI próprio mínimo, carregando kernel ELF independente. | Fronteira explícita e artefatos já exercitados em QEMU. | Parser, assinatura e hardening precisam revisão; sem Secure Boot configurado. |
| ADR-0005 | Substituída por ADR-0009 | Investigar microkernel mínimo com capabilities explícitas. | Hipótese inicial registrada na Fase 1. | Refinada pela decisão-base da Fase 3; métricas e propriedades permanecem não comprovadas. |
| ADR-0006 | Aceita para bring-up | Boot-info versionado, boot services encerrados antes da entrada do kernel. | Reduz dependência de firmware no kernel e permite um contrato observável. | Atualizar versão se mudar layout; documentar campos/reservas; validar parser e lifetime do mapa. |
| ADR-0007 | Aceita para bring-up | Build de desenvolvimento mantém mensagens seriais; teste QEMU observa eventos do boot. | Diagnóstico simples sem libc/GUI. | Mensagens não equivalem a métricas; saída serial não é concorrente-safe. |
| ADR-0008 | Proposta | Manter interrupts/preemption/SMP desativados até seus pré-requisitos serem testados. | Evita habilitar caminhos incompletos sem IDT/controlador/context switch. | Fases 7–10 precisam demonstrar cada transição antes de ativar. |
| ADR-0024 | Parcialmente aceita | Registrar falhas em ring estático bounded; falhas críticas continuam fail-stop. | Captura diagnóstico sem heap/locks, mas ausência de processos isolados impede contenção/restart segura. | Host e QEMU validam registro; supervisão, recuperação e SMP permanecem pendentes. |
| ADR-0025 | Parcialmente aceita | Registry limitado, PCI mechanism #1 somente leitura no bus 0 e driver platform COM1. | Plataforma q35 e UART existentes oferecem primeiro caminho de ponta a ponta sem habilitar DMA nem escrever BARs. | Host/QEMU validam lifecycle, scanner e UART; sem driver PCI, MMIO, IRQ routing, DMA, ECAM, isolamento ou cobertura geral de hardware. |
| ADR-0026 | Parcialmente aceita | Usar PIT IRQ0 tanto para a preempção testada quanto, em modo separado, como clocksource coarse; callbacks de deadlines ficam numa fila estática de 32 slots e são executados fora da IRQ. | Reutiliza o único timer que o kernel realmente inicializa; leitura PIT não exige pressupor invariância/frequência de TSC nem mapeamentos ACPI. Uma varredura bounded é suficiente para a capacidade atual. | Host tests cobrem conversão, monotonicidade, cancelamento, expiração, periodicidade e falhas; QEMU prova expiração real PIT. Sem sleep/wakeup do scheduler, timeouts bloqueantes, SMP, ACPI ou transição de energia. Revisar quando outra fonte/timer de dispositivo validado existir. |
| ADR-0027 | Parcialmente aceita | Primeiro armazenamento usa block layer bounded, ATA PIO primary-master e ORIONFS v1; imagem só é formatada por ferramenta explícita. | QEMU q35 oferece IDE legado testável agora; formato pequeno e sem partições delimita o primeiro gate. O boot não deve formatar nem montar/escrever automaticamente em mídia desconhecida. | Host tests, boot sem disco e persistência após novo processo QEMU passaram. Sem journaling, atomicidade a power loss, partições, cache, user descriptors, timeout de dispositivo ou SMP; suportar apenas a configuração efetivamente testada. |

| ADR-0009 | Aceita como arquitetura-base | Microkernel mínimo com capabilities para autoridade; serviços/drivers em user mode por padrão. | Alinha placement aos requisitos de contenção; trade-offs e lacunas de pesquisa explícitos. | Não prova segurança/desempenho; revisar com testes/medições de IPC, isolamento e recuperação. |
| ADR-0010 | Aceita como baseline | IPC síncrono request/reply, copiado, limitado e com quotas para controle. | Ownership simples para primeira implementação; shared memory adiada até necessidade medida. | Medir p50/p95/p99, throughput, jitter e CPU/op; limites definidos no gate de implementação. |
| ADR-0011 | Aceita como progressão | Bring-up cooperativo single-CPU; baseline user-mode preemptivo round-robin single-CPU; SMP separado. | Reduz concorrência no início e define progressão depois dos gates de IRQ. | Quantum depende de medição; sem prioridade user-configurable ou garantia real-time inicial. |
| ADR-0012 | Aceita como regra de placement | Mecanismos/invariantes no kernel; política e serviços fora por padrão. | Reduz privilégio implícito e mantém exceções revisáveis. | Driver/DMA/IRQ e overhead podem exigir exceção com ADR e testes. |
| ADR-0013 | Aceita como baseline de bring-up | Bitmap físico reservado por padrão. | Ownership e fail-closed simples de testar inicialmente. | Sem garantia de contiguidade ou velocidade; rever com dados de fragmentação/escala. |
| ADR-0014 | Aceita como arquitetura-base | Bundle de bootstrap pré-carregado para a primeira tarefa e supervisor mínimo. | Evita ciclo em que filesystem precisa carregar a si próprio. | Formato, assinatura, rollback e trust root são gates futuros. |
| ADR-0015 | Aceita para baseline inicial | C freestanding e Assembly restrito; Rust somente após avaliação por componente. | Compatível com toolchain disponível e mantém runtime explícito; Rust não foi encontrado no host. | C não é memory-safe; Rust exige avaliação de target/runtime/FFI/código gerado e manutenção antes da adoção. |
| ADR-0016 | Aceita para ambiente validado | MSYS2 UCRT64 no Windows, Clang/LLD, NASM e GNU Make. | Os targets PE/COFF UEFI e ELF x86-64 são suportados no ambiente observado; teste independente valida formato e dependências. | Versões não estão pinadas por lockfile; segundo host e CI ainda não validaram portabilidade. |
| ADR-0017 | Aceita para núcleo mínimo | Sequência determinística single-CPU, logs limitados e panic não retornável; validar apenas o estado de entrada necessário. | Testes host dos contratos e boot QEMU confirmaram sequência até idle; validação de entrada checou CPU/stack herdados do loader. | No fechamento original da Fase 6 ainda não havia GDT/IDT; Fase 7 adicionou tabelas e handlers de exceção, mas não memória própria, sincronização ou IRQ externo. |
| ADR-0018 | Aceita para exceções de CPU | Instalar GDT/TSS e IDT próprias, tratar vetores CPU 0–31, usar IST1 para #DF e manter IF fechado. | QEMU normal + testes isolados #BP/#DE/#UD/#GP/#PF/#DF; leituras SGDT/SIDT/CS/DS/TR validadas em runtime. | Sem PIC/APIC/IRQ externa, paginação própria, retorno de exceções fatais ou garantia de proteção de memória. |
| ADR-0019 | Aceita para bring-up; Fase 8 concluída no escopo de bootstrap | Mapa UEFI fail-closed, bitmap de frames reservado por padrão limitado a 4 GiB e page-table manager callback-based; adaptador QEMU clona o PML4 herdado e testa mapeamentos no CR3 clonado. | Testes host e QEMU para map/write/lookup/unmap, além de faults RO (#PF 3) e NX (#PF 17). | Sem address spaces de usuário, reclaim de firmware, NX setup pelo kernel, SMP shootdown ou garantia de portabilidade do alias físico/virtual. |
| ADR-0020 | Aceita para bring-up; Fase 9 concluída no escopo cooperativo | Processo kernel único, pool estático bounded e fila round-robin cooperativa single-CPU; não habilitar IF/preempção. | Host unit tests mais trace QEMU de duas threads alternadas, args/locals, término e invariantes. | Sem user mode/isolamento, scheduler preemptivo, general wait/wakeup ou SMP; slots/IDs não reutilizados. |
| ADR-0021 | Aceita para bring-up; Fase 10 concluída no escopo preemptivo single-core | PIT/PIC IRQ0 a 100 Hz nominal, vetor 32, FIFO round-robin e scheduler protegido por salvar/restaurar IF. | QEMU `-smp 1`: CPU-bound workers progrediram sem yield, IRQ0/EOI, preempções, contexto e caminho sem peers verificados; TSC virtual usado apenas como diagnóstico. | Sem AP startup/SMP, APIC, IRQ de dispositivos, FPU/SIMD switching, medição física ou garantias de latência. |
| ADR-0022 | Aceita para Fase 11 parcial | Tabelas bounded de capabilities no kernel; handles locais compostos por geração de tabela, geração de slot e índice; derivação/transferência apenas em subconjuntos; revogação invalida slots globais e leases existentes atrasam destruição. | Host negatives e autoteste do boot QEMU passam; sincronização salva/desabilita IF no kernel single-core. | Não há Ring 3, address space por processo, IPC, API de usuário nem segurança SMP; capabilities ainda não mediam operações reais de memória/dispositivo/processo. |
| ADR-0023 | Aceita para Fase 12 parcial | Canais full-duplex com duas capabilities de endpoint, filas FIFO estáticas, payload copiado até 256 bytes e operações não bloqueantes. Autoridade deriva das capabilities tipadas da Fase 11. | Testes host cobrem conteúdo, limites, fila, autoridade, fechamento, exaustão e invariantes; QEMU prova troca entre duas threads kernel. | Sem wait queues, user mode, entrega de capabilities em mensagens, process-exit cleanup ou sincronização SMP; não é IPC de aplicações. |

Alterações devem incluir a evidência que motivou a revisão, decisões rejeitadas
e efeito nos requisitos rastreados.

## Decisões que ainda exigem escolha humana

- Qual distribuição/versão Linux será a referência comparativa e em quais
  workloads ela será mantida.
- Quais dispositivos físicos, firmware e plataformas serão alvo após QEMU.
- Qual política de Secure Boot, origem de chaves e anti-rollback será aceitável
  antes de uma distribuição instalável.
- Se compatibilidade com APIs/aplicações existentes será requisito ou se a
  plataforma começará com API própria.
- Quais dispositivos e ambientes físicos comporão a primeira matriz de suporte.

A arquitetura-base ADR-0009 está selecionada. Os itens acima não bloqueiam
essa seleção documental, mas precisam ser resolvidos antes dos respectivos
gates de plataforma, trust root e release.

## ADRs formais das decisões-base

Estas decisões de plataforma/bring-up foram registradas inicialmente na Fase 1.
São formalizadas aqui para manter contexto, alternativas, evidência e revisão
rastreáveis. ADR-0005 foi substituída por ADR-0009.

### ADR-0001 — Alvo inicial x86-64, UEFI e QEMU/EDK2 (PROVISÓRIA)

- **Contexto/problema:** escolher alvo inicial testável sem alegar suporte amplo.
- **Alternativas:** outras ISAs, BIOS legado, firmware físico como único alvo.
- **Evidência:** requisitos da Fase 1 e boot UEFI observado em QEMU/EDK2. A
  documentação QEMU observa que opções variam por target/machine e que TCG é
  emulação; ver `research/phase-2-evidence.md`.
- **Decisão/justificativa:** x86-64 UEFI e QEMU/EDK2 para bring-up; o caminho
  atual compila e inicializa nesse ambiente.
- **Consequências positivas:** teste automatizável e inspeção da cadeia de boot.
- **Consequências negativas:** firmware/dispositivos virtuais não representam
  hardware real nem comprovam desempenho bare metal.
- **Riscos residuais:** cobertura de CPU/firmware limitada; hardware físico
  ainda não selecionado.
- **Revisão:** testar firmware e hardware reais identificados, mantendo QEMU
  como baseline automatizada.

### ADR-0002 — C freestanding e Assembly; Rust avaliado por caso (PROVISÓRIA)

- **Contexto/problema:** transições de baixo nível exigem ABI e runtime
  explícitos; não se deve presumir segurança por linguagem.
- **Alternativas:** C/Assembly; Rust integral; Rust seletivo com FFI.
- **Evidência:** toolchain atual compila o scaffold C/Assembly; não há
  comparação experimental de defeitos ou manutenção.
- **Decisão/justificativa:** usar C freestanding e Assembly imediatamente;
  avaliar Rust só para componentes com benefício demonstrável.
- **Consequências positivas:** alinha-se ao código atual e mantém runtime
  explícito.
- **Consequências negativas:** C exige disciplina contra UB; Rust/FFI adiciona
  integração, ABI e possível runtime.
- **Riscos residuais:** erros de memória, bug de compilador e falsa confiança em
  análise estática.
- **Revisão:** validar target, core/runtime, panic, allocator, ABI/FFI, código
  gerado e manutenção em CI antes de aceitar componente Rust.

### ADR-0003 — Clang/LLD e NASM no bring-up Windows (PROVISÓRIA)

- **Contexto/problema:** produzir PE/COFF UEFI e ELF freestanding x86-64 no
  ambiente disponível.
- **Alternativas:** cross-GCC dedicado; Clang com targets cross; ambiente
  Linux/WSL.
- **Evidência:** build atual gera loader EFI e kernel ELF; inspeção confirma
  arquitetura/entry e ausência de símbolos indefinidos reportados.
- **Decisão/justificativa:** Clang `x86_64-pc-windows-msvc` + `lld-link` para
  EFI, Clang `x86_64-elf` + LLD para kernel, NASM e GNU Make.
- **Consequências positivas:** funciona no host Windows sem cross-GCC separado.
- **Consequências negativas:** múltiplos targets/ABIs e instalação local
  dificultam portabilidade.
- **Riscos residuais:** versão/build não reproduzidos em host independente;
  ABI UEFI precisa revisão.
- **Revisão:** fixar versões/manifesto e reproduzir build em CI limpo e segundo
  host antes de release.

### ADR-0004 — Loader UEFI ORION mínimo para kernel ELF (PROVISÓRIA)

- **Contexto/problema:** manter uma fronteira observável entre firmware e
  kernel próprio.
- **Alternativas:** loader próprio; loader existente externo e documentado;
  imagem kernel EFI direta.
- **Evidência:** teste QEMU observou leitura/validação ELF, `ExitBootServices`
  e entry do kernel; parser e autenticidade ainda não foram endurecidos.
- **Decisão/justificativa:** manter loader próprio mínimo que carrega kernel
  ELF e entrega boot-info versionado, sem incorporar implementação de kernel
  alheia.
- **Consequências positivas:** contrato de handoff e erro pertencem ao projeto.
- **Consequências negativas:** parser ELF/ABI UEFI viram superfície de
  segurança mantida pelo ORION.
- **Riscos residuais:** overflow/segmentos malformados, imagem não assinada e
  firmware não testado.
- **Revisão:** fuzz parser, testar bounds/overlap/entry, definir assinatura e
  comparar custo com loader externo.

### ADR-0006 — Boot-info versionado e sem boot services pós-EBS (ACEITA PARA BRING-UP)

- **Contexto/problema:** kernel precisa receber mapa e estado sem chamar
  serviços UEFI após o handoff.
- **Alternativas:** continuar com boot services; reconstruir firmware state no
  kernel; passar estrutura de boot versionada.
- **Evidência:** contrato compartilhado e teste QEMU confirmam boot-info e
  `ExitBootServices`; mapa/reservas ainda não foram totalmente auditados.
- **Decisão/justificativa:** boot-info versionado, preservar mapa e
  não chamar boot services após EBS bem-sucedido.
- **Consequências positivas:** fronteira observável e campos validáveis.
- **Consequências negativas:** layout é ABI; novas reservas exigem evolução
  explícita.
- **Riscos residuais:** descriptor lifetime e memória ocupada não bastam ainda
  para allocator seguro.
- **Revisão:** Fase 8 valida descritores, limites e reservas antes de
  reutilizar qualquer página.

### ADR-0007 — UART serial para diagnóstico de bring-up (ACEITA PARA BRING-UP)

- **Contexto/problema:** não há GUI/console de kernel e o boot deve ser
  verificável automaticamente.
- **Alternativas:** UEFI console, UART, framebuffer, buffer de log.
- **Evidência:** serial real observada no QEMU; não existe garantia de UART em
  hardware arbitrário.
- **Decisão/justificativa:** manter COM1 como caminho diagnóstico inicial e
  harness de boot.
- **Consequências positivas:** saída simples e observável fora do guest.
- **Consequências negativas:** I/O pode falhar/expirar; logger não é
  concorrente-safe sem sincronização.
- **Riscos residuais:** panic pode não conseguir imprimir; custo de serial
  perturba medições.
- **Revisão:** medir overhead e definir locking/drop policy antes de SMP;
  substituir quando console/backend real existir.

### ADR-0008 — Não ativar IRQ/preempção/SMP antes de validar caminho seguro (PROVISÓRIA)

- **Contexto/problema:** kernel não possui IDT/controlador/context switch
  próprios e testados.
- **Alternativas:** habilitar cedo; manter desabilitado; ativar fontes
  gradualmente após validação.
- **Evidência:** `_start` executa `cli`; boot QEMU usa uma vCPU; sem IDT ORION,
  handler ou EOI.
- **Decisão/justificativa:** manter interrupts desabilitadas; ativar uma fonte
  só após IDT, stack, handler, controller/EOI e retorno testados; preempção e
  SMP têm gates próprios.
- **Consequências positivas:** não expõe caminhos IRQ incompletos durante boot.
- **Consequências negativas:** sem preempção, wakeups externos ou paralelismo.
- **Riscos residuais:** races podem surgir ao ativar timer/APs posteriormente.
- **Revisão:** satisfazer testes das Fases 7–10, incluindo critical sections,
  ownership e múltiplas vCPUs antes de habilitar.

## Decisões arquiteturais da Fase 3

As decisões abaixo fixam um baseline documental para a implementação, sem
afirmar que qualquer mecanismo já existe no código. As escolhas que dependem
de dados de hardware/workload permanecem parametrizadas e têm gates explícitos.

### ADR-0009 — Microkernel mínimo com autoridade por capabilities (ACEITA COMO ARQUITETURA-BASE)

- **Contexto/problema:** requisitos priorizam isolamento de falhas, autoridade
  mínima, possibilidade de recuperação de serviços e mensuração de desempenho.
  O scaffold atual somente inicializa e não possui processos/IPC.
- **Alternativas:** kernel monolítico; microkernel; híbrido; capabilities como
  mecanismo combinável com cada modelo.
- **Evidências consultadas:** requisitos da Fase 1; comparação de alternativas
  em `architecture-options.md`; registros EVD-001–EVD-010 em
  `research/evidence-register.md`; limitações em `research/research-gaps.md`.
  As fontes sustentam mecanismos e trade-offs, não superioridade global nem
  resultados de performance comparáveis.
- **Decisão:** selecionar um microkernel com mecanismos de CPU, memória,
  threads/escalonamento, IPC mínimo, objetos de capability, tempo e mediação
  de recursos em ring 0. Filesystem, rede, políticas, supervisor, drivers e UI
  executam em processos de usuário por padrão. Capabilities são o mecanismo de
  autoridade; não são uma afirmação de ausência de vulnerabilidades.
- **Justificativa:** esta composição alinha mais diretamente o placement de
  serviços e drivers aos requisitos de contenção e autoridade explícita. O
  custo de IPC e a cobertura de hardware ainda não foram medidos; portanto,
  não se afirma vantagem de desempenho ou segurança efetiva.
- **Consequências positivas:** fronteiras de privilégio auditáveis; falhas de
  serviço podem ser isoladas e potencialmente recuperadas quando lifecycle,
  dependências, DMA e estado persistente o permitirem.
- **Consequências negativas:** exige processos, VM, IPC, contracts, supervisor
  e ferramentas antes de alcançar funcionalidade rica; aumenta overhead e
  complexidade distribuída.
- **Riscos residuais:** kernel/capability bugs, confused deputy, revogação,
  shared-host drivers, DMA sem IOMMU, TCB de bootstrap, custo de IPC e suporte
  limitado de drivers.
- **Critérios de revisão:** rever a decisão se testes negativos mostrarem
  fronteiras inexequíveis, se o custo medido de IPC/driver impedir os workloads
  mínimos definidos, ou se a revisão de ameaça encontrar uma alternativa que
  satisfaça os mesmos requisitos com menor risco total. Até lá, manter
  comparação reproduzível e não afirmar superioridade sobre Linux.

### ADR-0010 — Cópia limitada para IPC inicial (ACEITA COMO BASELINE)

- **Contexto:** componentes futuros precisam de uma interface de comunicação
  com ownership claro; scheduler, processos e IPC ainda não existem.
- **Problema:** definir caminho inicial testável sem referências a memória com
  lifetime/TOCTOU ambígua, mas sem excluir otimizações futuras medidas.
- **Alternativas consideradas:** mensagens copiadas; buffers referenciados;
  shared memory; notificações/eventos; RPC síncrono ou assíncrono.
- **Evidências consultadas:** análise de contratos em `architecture/ipc-design.md`
  e documentação oficial de conceitos/tutoriais do seL4 e Fuchsia. Essas fontes
  informam modelos, não fornecem um resultado de desempenho transferível;
  ORION não tem IPC implementado.
- **Decisão:** baseline inicial request/reply síncrono, mensagem inline
  limitada, validação/cópia na fronteira e quotas por endpoint/processo. Bulk
  data não usará shared memory até haver necessidade medida e contrato de
  ownership/pinning/revogação.
- **Justificativa:** cópia torna o ownership inicial explícito e facilita
  testes de malformed input; não prova adequação de throughput.
- **Consequências positivas:** lifetime simples; mensagem não retém ponteiro do
  remetente; parsing limitado e testável.
- **Consequências negativas:** cópia, kernel entry, bloqueio e quotas podem
  elevar latência ou reduzir throughput.
- **Riscos residuais:** deadlock por dependência circular, denial-of-service
  por filas, timeout confundido com cancelamento, versões incompatíveis e
  starvation. Valores concretos de limites ainda não escolhidos.
- **Critérios de revisão:** medir round-trip p50/p95/p99, jitter, msgs/s,
  bytes/s, CPU/op e saturação em tamanhos diversos; comparar alternativa com
  semântica/validação equivalente; executar fuzz e teardown/race tests nas
  Fases 11–12.

### ADR-0011 — Bring-up cooperativo e baseline preemptivo single-CPU (ACEITA)

- **Contexto:** o teste atual usa QEMU com uma vCPU e interrupts desabilitadas;
  não há IDT, allocator, locks, scheduler ou frame de interrupção.
- **Problema:** limitar estados concorrentes no primeiro scheduler e não
  permitir preempção sobre contexto ainda não validado.
- **Alternativas consideradas:** habilitar preempção e APs no primeiro
  scheduler; começar single-CPU com yield cooperativo; começar single-CPU
  preemptivo após validar IRQ/context.
- **Evidências consultadas:** configuração/saída do teste ORION e gates das
  Fases 7–10. Documentações oficiais de Linux EEVDF/CFS e seL4 mostram
  políticas concretas, mas não prescrevem uma política transferível nem
  fornecem benchmark comparável ao ORION.
- **Decisão:** iniciar single-CPU com round-robin cooperativo para validar
  lifecycle; o baseline pretendido para user mode é round-robin preemptivo em
  uma CPU, sem prioridades configuráveis pelo usuário na primeira versão.
  Habilitar preempção somente após IDT/controller/EOI/frame/retorno e seções
  críticas testados. SMP é gate separado.
- **Justificativa:** reduz concorrência assíncrona inicial e permite testar
  ownership das filas antes de incluir timer e contexto interrompido.
- **Consequências positivas:** depuração e testes determinísticos mais
  simples; AP startup não bloqueia a primeira versão.
- **Consequências negativas:** a etapa cooperativa não limita responsividade
  enquanto uma thread não cede; preempção adiciona custo de timer/context
  switch; não há paralelismo até o gate SMP.
- **Riscos residuais:** starvation, latência não limitada, timer drift,
  preemption bugs e races durante evolução para SMP.
- **Critérios de revisão:** demonstrar alternância de duas threads, lifecycle e
  invariantes; medir latência/throughput; passar testes preemptivos single-vCPU;
  iniciar SMP apenas com per-CPU state, lock-order, TLB shootdown e testes
  multi-vCPU.

### ADR-0012 — Política em user mode, mecanismos privilegiados no kernel (ACEITA COMO REGRA DE PLACEMENT)

- **Contexto:** separar mecanismos privilegiados de política pode restringir
  impacto de falhas, mas ring 3, memória virtual e DMA/IRQ ainda não existem.
- **Problema:** definir placement sem presumir que toda função seja segura ou
  prática de mover para user mode antes de existir mediação.
- **Alternativas consideradas:** todos os subsistemas em ring 0; arquitetura
  híbrida com caminhos privilegiados específicos; mecanismos mínimos no kernel
  e policy/serviços em processos isolados.
- **Evidências consultadas:** matriz em `architecture/kernel-boundaries.md`;
  documentação oficial Fuchsia Driver Framework descreve driver manager,
  driver hosts como processos e possibilidade de co-localizar drivers; Linux
  documenta um device model hierárquico integrado. São modelos documentados,
  não prova de superioridade; ORION ainda não tem user mode/drivers.
- **Decisão:** manter no kernel operações privilegiadas e invariantes que
  exigem mediação (VM, context/thread, exceções/IRQ, IPC mínimo, capability
  checks e grants MMIO/IRQ/DMA). Filesystem, rede, device policy/manager,
  drivers, supervisor, compositor e aplicações ficam fora do kernel por
  padrão. Exceção exige ADR com autoridade, motivo, dados e testes.
- **Justificativa:** limita policy/protocolo no TCB sem ignorar as dependências
  físicas que podem impedir isolamento de um driver.
- **Consequências positivas:** blast radius potencialmente menor, serviços
  substituíveis e policy fora de ring 0.
- **Consequências negativas:** custo de IPC/bootstrap/debug distribuído;
  falha de driver ainda pode afetar dispositivo e sistema.
- **Riscos residuais:** driver host compartilhado amplia domínio de falha; sem
  IOMMU DMA pode atravessar fronteiras; confused-deputy e dependência de
  serviços críticos do supervisor.
- **Critérios de revisão:** testar processos/driver hosts user mode, tentativas
  de acesso não autorizado, DMA com/sem IOMMU, recuperação de falha e overhead
  de IRQ/IPC; revisar exceções por classe de dispositivo.

### ADR-0013 — Bitmap reservado por padrão para alocação inicial de frames (ACEITA COMO BASELINE DE BRING-UP)

- **Contexto/problema:** a memória física precisa de ownership explícito antes
  da criação de processos e mappings de usuário. A escala e fragmentação reais
  do mapa UEFI ainda não foram medidas.
- **Alternativas consideradas:** bitmap de frames; free list; buddy allocator;
  allocator híbrido.
- **Evidências consultadas:** contrato de boot-info e requisitos de ownership
  em `architecture/memory-and-concurrency.md`; evidência de desempenho do
  ORION ainda não existe.
- **Decisão:** iniciar com bitmap reservado por padrão para páginas físicas.
  A implementação atual aceita apenas descritores `EfiConventionalMemory`
  depois de validar mapa, stride, limites, overflow e sobreposição. Outras
  regiões permanecem reservadas por tipo UEFI. O range gerido está limitado a
  4 GiB, com metadados estáticos em BSS. Não prometer alocação contígua nem
  reclaim de memória de firmware.
- **Justificativa:** ownership e testes de double-free, reservation e
  exhaustion são diretos; não há evidência que justifique complexidade maior
  no primeiro allocator.
- **Consequências positivas:** estado observável e política fail-closed para
  memória não classificada.
- **Consequências negativas:** metadados proporcionais ao número de frames;
  busca e alocação contígua podem ser lentas ou indisponíveis.
- **Riscos residuais:** mapa de firmware malformado, overflow em cálculo do
  tamanho do bitmap e overhead em workloads grandes.
- **Critérios de revisão:** testar limites de mapa, exhaustion, fragmentação e
  tempo de busca; considerar free-list/buddy se medição mostrar que os custos
  violam requisitos concretos.

### ADR-0019 — Mapa UEFI e page-table manager inicial (ACEITA PARA BRING-UP; FASE 8 CONCLUÍDA NO ESCOPO DE BOOTSTRAP)

- **Contexto/problema:** a Fase 8 precisa contabilizar frames sem assumir que
  page tables ou identity mappings do firmware sejam portáveis entre máquinas.
- **Alternativas consideradas:** continuar sem PMM; trocar imediatamente para
  um root próprio; inicializar PMM e testar um gerenciador virtual por callbacks
  sem alterar o espaço ativo.
- **Evidência:** testes host cobrem validação do mapa/PMM e page-table
  map/lookup/unmap, flags e rollback OOM. QEMU/EDK2 validou o mapa real,
  invariantes do PMM, allocate/release, clonagem do PML4 herdado, troca para
  o root clonado e map/write/lookup/unmap temporário. Imagens QEMU separadas
  confirmaram #PF 3 ao escrever em página RO e #PF 17 ao executar em página
  NX. O root, pool e páginas de kernel permanecem em regiões reservadas.
- **Decisão:** manter a reserva fail-closed de todos os tipos UEFI exceto
  convencional, PMM limitado a 4 GiB e API de page table com callbacks
  explícitos. Para o escopo de bootstrap, copiar as entradas do PML4 herdado
  para root estático reservado e instalar essa cópia antes do teste real de
  mapeamento, preservando os mapeamentos ativos. O adaptador usa um pool
  estático limitado a 32 tabelas e acessa-o por endereço virtual numericamente
  igual ao físico.
- **Consequências:** map/write/lookup/unmap e permissões RO/NX são exercitados
  contra tabelas ativas em QEMU; o kernel continua sem address space de usuário
  ou proteção entre processos. A identidade física/virtual do pool foi
  observada apenas em QEMU/EDK2 e não é um contrato de portabilidade.
- **Riscos residuais:** teto físico, bitmap fixo, sem reclaim de boot-services,
  sem TLB shootdown SMP, sem NXE habilitado pelo kernel e sem validação em
  firmware/hardware distintos.
- **Critério de conclusão do escopo de bootstrap:** mapear, consultar e remover
  uma página no root ativo; observar faults de escrita RO e execução NX;
  confirmar preservação do boot, stack e mapeamentos herdados; cobrir rollback
  de alocação nos testes host. Esses critérios passaram no ambiente declarado.
  Address spaces, user isolation e suporte multiplataforma continuam gates
  futuros, não extensões implícitas desta decisão.

### ADR-0020 — Threads de kernel e escalonamento cooperativo bounded (ACEITA PARA BRING-UP)

- **Contexto/problema:** preparar lifecycle e troca de contexto testáveis sem
  habilitar IRQs, timer ou transições de privilégio ainda não validados.
- **Alternativas consideradas:** manter somente o contexto de boot; fazer
  troca preemptiva por IRQ; implementar threads cooperativas com pool e fila
  limitada.
- **Evidência:** host tests exercitam limites, estados e invariantes; QEMU
  11.1.2/EDK2 observou duas pilhas e threads distintas alternando em oito
  eventos ordenados, preservando argumento e local volatile, terminando e
  retornando ao contexto bootstrap. IF permaneceu desabilitado.
- **Decisão:** um processo kernel organizacional, oito TCBs estáticos contando
  o bootstrap, stacks de 16 KiB, FIFO round-robin cooperativo e contexto SysV
  AMD64 limitado ao stack pointer e registradores GPR preservados pelo ABI.
  Manter IF clear, sem timer, preempção, user mode ou APs. Destruir somente
  threads terminadas em outro stack; aposentar seus TCB slots para impedir
  aliasing de handles antigos.
- **Consequências:** multitarefa cooperativa de kernel real é verificável,
  mas recursos são limitados por boot e não existe contenção automática de
  tarefa que não cede. Processo não equivale a domínio isolado.
- **Riscos residuais:** sem guard pages, locks, bloqueio/despertar genérico,
  FPU/SIMD thread state, medição temporal ou validação de ABI sob outras
  toolchains; stacks e TCBs são memória BSS compartilhada.
- **Critério de revisão:** antes da Fase 10, validar salvamento de contexto de
  interrupção, timer/EOI, regiões críticas e retorno IDT; antes de user mode,
  definir root de processo, privilégio, syscalls e ownership de recursos.

### ADR-0021 — Preempção PIT/PIC single-core (ACEITA PARA BRING-UP)

- **Contexto/problema:** o round-robin cooperativo da Fase 9 não impede uma
  thread CPU-bound de monopolizar a CPU; o handler de exceção não reconhecia
  IRQs externas e o scheduler não possuía exclusão de interrupções.
- **Alternativas consideradas:** manter yield cooperativo; usar PIT/PIC legado
  para a primeira IRQ verificável; introduzir APIC timer e SMP desde o início.
- **Evidência:** QEMU 11.1.2/EDK2 em uma vCPU registrou IRQ0/preempções e duas
  workers CPU-bound executaram mais de 1.000.000 iterações sem yield. O teste
  verificou quatro ticks sem peers prontos, stacks, estados, contador de
  switches, e TSC virtual para dispatch/handoff.
- **Decisão:** programar PIT channel 0 com divisor 11.932, remapear PIC para
  0x20/0x28 e habilitar só IRQ0 durante o auto-teste de boot. Vector 32 usa
  stub de frame normalizado e `iretq`; o EOI precede qualquer context switch.
  A rotação FIFO ocorre no contexto da interrupção, mas reutiliza o context
  switch call-based sobre a pilha da thread interrompida. Scheduler mutations
  salvam IF, executam com IF clear e restauram o estado anterior.
- **Consequências positivas:** há preempção executável e evidência automática
  de que CPU-bound work não bloqueia peers no ambiente validado.
- **Consequências negativas:** dependência de PIC/PIT legacy e de uma única
  CPU; quantum nominal ~10 ms sujeito a jitter; a VM testada não valida
  firmware/hardware físicos.
- **Riscos residuais:** sem APIC/IOAPIC, AP startup, SMP locks/TLB shootdown,
  general blocking, user mode, per-thread FPU/SIMD state ou cobertura de
  latência de cauda. TSC reportado é virtual e não calibrado.
- **Critérios de revisão:** before enabling the timer in general operation,
  add full interrupt/device policy and longer stress tests; before SMP, require
  AP topology/startup, per-CPU state, unique ownership and multi-vCPU tests.

### ADR-0014 — Boot bundle pré-carregado para bootstrap de user mode (ACEITA COMO ARQUITETURA-BASE)

- **Contexto/problema:** o primeiro serviço não pode depender de filesystem
  user-mode que ainda não foi iniciado para carregar a si próprio.
- **Alternativas consideradas:** root task embutida no kernel; bundle carregado
  pelo loader; init image em dispositivo/raw partition; filesystem inicial
  privilegiado.
- **Evidências consultadas:** dependências e ciclos descritos em
  `architecture/component-model.md` e `architecture/boot-sequence.md`; loader
  atual somente carrega `kernel.elf` e não implementa esse mecanismo.
- **Decisão:** o loader futuro carrega e reserva um bundle de bootstrap com o
  primeiro executável e metadados mínimos; o kernel valida limites/versão e
  cria o primeiro address space depois de inicializar memória e privilege
  transition. O bundle não será lido por um filesystem que dependa dele.
- **Justificativa:** remove o ciclo de dependência e mantém serviço/policy fora
  do kernel. Autenticidade ainda depende da decisão separada de trust root,
  assinatura e anti-rollback.
- **Consequências positivas:** caminho de bootstrap finito e autoridade inicial
  explícita.
- **Consequências negativas:** loader e boot contract ficam responsáveis por
  carregar/reservar mais de um artefato; formato e espaço de memória precisam
  de versionamento e validação.
- **Riscos residuais:** bundle adulterado, parser de manifest, tamanho limitado
  de memória e falta de chave confiável.
- **Critérios de revisão:** especificar formato e autenticação na fase de boot
  verificado; testar bundle truncado, inválido, versão incompatível e
  assinatura/rollback rejeitados antes de executar sua primeira tarefa.

### ADR-0015 — C freestanding e Assembly restrito; Rust por caso (ACEITA PARA BASELINE INICIAL)

- **Contexto/problema:** o loader e o kernel precisam de controle explícito da
  ABI e do runtime, sem assumir biblioteca do sistema hospedeiro. A ferramenta
  Rust não foi encontrada no host avaliado.
- **Alternativas consideradas:** C/Assembly; Rust integral; adoção seletiva de
  Rust com fronteira FFI.
- **Evidências:** o probe da Fase 4 foi compilado como ELF64 x86-64 e ligado sem
  símbolos indefinidos; isso valida a cadeia C/LLD, não a segurança da
  linguagem nem o código do kernel.
- **Decisão:** manter C freestanding como linguagem principal imediata e
  restringir Assembly às transições/rotinas dependentes da arquitetura. Rust
  permanece elegível somente por caso de uso com benefício verificável.
- **Consequências positivas:** runtime e ABI ficam explícitos; o probe
  independente teve resultados bit a bit iguais em duas execuções no host
  validado.
- **Consequências negativas:** C mantém riscos de comportamento indefinido e
  segurança de memória; rotinas Assembly exigem revisão e testes próprios.
- **Riscos residuais:** erros de compilador, UB, FFI e falsa confiança em
  warnings.
- **Critérios de revisão:** qualquer proposta Rust deve validar target
  `no_std`, panic/runtime/allocator, ABI/FFI, código gerado, depuração,
  manutenção e testes automatizados antes de entrar na build principal.

### ADR-0016 — Toolchain Windows MSYS2 UCRT64 para bring-up (ACEITA PARA AMBIENTE VALIDADO)

- **Contexto/problema:** o repositório precisa de build x86-64 EFI PE/COFF e
  kernel ELF no host Windows detectado, sem exigir WSL ou instalação de
  dependências nesta fase.
- **Alternativas consideradas:** GNU cross-toolchain, Clang/LLD nativos,
  WSL/Linux.
- **Evidências:** ferramentas e versões observadas no host estão em
  `toolchain.md`; `tests/toolchain/` valida duas compilações ELF independentes,
  inspeção de arquitetura/formato/símbolo, ausência de símbolos indefinidos e
  hash repetido.
- **Decisão:** usar MSYS2 UCRT64 com Clang/LLD, NASM e GNU Make como ambiente
  de desenvolvimento validado. Manter os alvos EFI e ELF separados por
  target/ABI. O probe autônomo não depende de QEMU/EDK2.
- **Consequências positivas:** um host Windows pode gerar e inspecionar os
  formatos necessários, com comando de teste independente.
- **Consequências negativas:** instalação local/versionamento não é fixado e
  ainda não há matriz de CI ou segundo host.
- **Riscos residuais:** diferenças entre versões/hosts, caminhos MSYS2
  específicos e dependências adicionais introduzidas pelo build completo.
- **Critérios de revisão:** fixar/registrar versão antes de releases, validar
  build limpo em CI e segundo host, e rever target/flags caso mudem ABI,
  relocations ou modelo de carregamento.

### ADR-0017 — Núcleo mínimo, inicialização serial e panic fail-stop (ACEITA PARA FASE 6)

- **Contexto/problema:** o kernel inicializado pela Fase 5 precisa de uma
  sequência verificável antes de receber exceções, memória virtual ou
  concorrência.
- **Alternativas consideradas:** continuar com loop mínimo sem estágios;
  introduzir framework genérico de subsistemas; adicionar somente as
  verificações e interfaces exigidas pelo caminho atual.
- **Evidências:** `mingw32-make test-unit build inspect` passou; o ELF foi
  identificado como ELF64 x86-64 e linkado com `--no-undefined`; QEMU 11.1.2
  alcançou `IDLE` e permaneceu ativo por 15 s até parada do harness.
- **Decisão:** manter uma sequência monotônica single-CPU, logging síncrono com
  limites explícitos, verificação estreita do estado x86-64 herdado e panic
  fail-stop com diagnóstico best effort. Não introduzir GDT/IDT, allocator,
  scheduler ou reset automático nesta fase.
- **Consequências positivas:** eventos e pré-condições do boot ficam
  observáveis/testáveis; panic não retorna ao caminho normal.
- **Consequências negativas:** um erro fatal estaciona a máquina; diagnóstico
  depende da serial e não existe recuperação.
- **Riscos residuais:** ponteiros ainda dependem de mapeamentos herdados;
  validação CPU não verifica proteção das páginas; log/panic não são seguros
  para ISR/SMP.
- **Critérios de revisão:** fases 7–10 devem estabelecer exceções,
  ownership/mapeamento de memória e sincronização antes de ampliar o uso do
  logger/panic. QEMU/hardware precisa testar cada mecanismo quando habilitado.

### ADR-0018 — GDT/TSS/IDT para exceções de CPU, IRQs externas desabilitadas (ACEITA PARA FASE 7)

- **Contexto/problema:** o boot inicial dependia das tabelas herdadas do UEFI
  e não possuía um caminho controlado para registrar exceções.
- **Alternativas consideradas:** continuar com as tabelas firmware; instalar
  apenas uma IDT; instalar GDT/TSS e IDT próprias antes de qualquer IRQ.
- **Evidências:** o kernel leu de volta GDTR/IDTR/CS/DS/TR; QEMU atingiu idle
  com as tabelas próprias; instâncias isoladas confirmaram #BP retornável,
  #DE, #UD, #GP/error-code, #PF/CR2 e #DF/IST1.
- **Decisão:** instalar GDT ring-0 code/data, TSS com stack IST1, 32 stubs de
  exceção e IDT com gates somente para vetores CPU 0–31. Mascarar NMI durante
  a substituição de descritores pela porta PC CMOS/NMI, restaurando o estado
  anterior após validar as tabelas. Não reconfigurar PIC/APIC nem habilitar IF.
- **Consequências positivas:** frame/erro das exceções testadas são observáveis;
  #DF dispõe de uma stack IST separada; caminho fatal é determinístico.
- **Consequências negativas:** falhas fatais continuam fail-stop; a stack IST
  não tem guard page; a política de NMI depende do mecanismo PC-compatible.
- **Riscos residuais:** mapeamentos ainda são herdados; vectores 32–255 estão
  ausentes; sincronização, user mode, IRQ EOI e recuperação de page fault não
  existem.
- **Critérios de revisão:** habilitar IRQ somente após controlador, vetores,
  acknowledgement/EOI, pilhas, retorno e sincronização serem testados em fases
  posteriores; rever a dependência da porta 0x70 para plataformas distintas.

### ADR-0022 — Tabelas bounded de capabilities no kernel (ACEITA PARA FASE 11 PARCIAL)

- **Contexto/problema:** processos e threads existentes executam apenas em
  Ring 0; não há address spaces independentes, syscalls ou IPC. A fase precisa
  validar regras de autoridade sem alegar isolamento de um processo hostil.
- **Alternativas consideradas:** adiar qualquer implementação até Ring 3;
  criar handles globais por ponteiro; introduzir tabelas por proprietário,
  gerações, direitos tipados e leases sem expor ponteiros como handles.
- **Evidências:** testes host de negação/atenuação/revogação/lifecycle e
  autoteste de boot QEMU passaram; `mingw32-make build inspect` linkou o
  kernel freestanding sem símbolos indefinidos.
- **Decisão:** implementar tabelas estáticas limitadas e kernel-only, handles
  locais com geração de tabela/slot, direitos por tipo, derive/transfer como
  subconjuntos, revogação global e destruição adiada por leases. Salvar e
  desabilitar IF serializa operações somente no ambiente single-core atual.
- **Consequências positivas:** direitos e lifecycle são verificáveis sem
  ponteiros de kernel nos handles; falhas de autorização e esgotamento são
  explícitos.
- **Consequências negativas:** slots estáticos podem ser aposentados; não há
  escalabilidade, proteção SMP, política de concessão a usuário ou mediação de
  operações reais de memória/dispositivo.
- **Riscos residuais:** comprometimento do kernel contorna o mecanismo; leases
  preexistentes continuam até release após revogação; códigos ainda podem
  contornar a API se operações futuras não forem integradas.
- **Critérios de revisão:** integrar às operações reais, user mode, IPC,
  quotas, teardown de processos e sincronização apropriada antes de alegar
  isolamento; executar testes negativos em Ring 3 e stress/race tests quando
  SMP for introduzido.

### ADR-0024 — Ring estático de diagnóstico e política fail-stop (ACEITA PARA FASE 13 PARCIAL)

- **Contexto/problema:** exceções CPU e panic tinham somente mensagens seriais;
  o kernel não dispõe de processos de usuário ou serviços isolados que possam
  ser recuperados com segurança.
- **Alternativas consideradas:** alocar memória dinamicamente no handler;
  tentar retomar após qualquer exceção; registrar eventos em buffer estático e
  manter fail-stop para falhas críticas.
- **Evidências:** testes host cobrem campos, capacidade e overwrite; imagens
  QEMU testam #DE/#UD/#GP/#DF/#PF/RO/NX e o caminho de panic. A suíte não
  demonstra recuperação de serviços.
- **Decisão:** usar ring volátil de 32 registros, sem heap, com IDs
  monotônicos, validação de enumeração, overwrite explícito e serialização
  single-core por salvar/desabilitar/restaurar IF. #BP continua; as demais
  exceções CPU registradas seguem para panic; panic registra best-effort e
  nunca retorna.
- **Consequências positivas:** eventos estruturados consultáveis e limitados
  estão disponíveis sem alocação em contexto crítico.
- **Consequências negativas:** eventos são perdidos no reboot e os mais
  antigos são descartados na saturação; não existe contenção/restart de
  processos, nem sincronização SMP.
- **Riscos residuais:** falha da UART/stack/CPU pode impedir diagnóstico;
  registro não prova recuperação ou integridade; caminhos sem integração
  central podem não aparecer no ring.
- **Critérios de revisão:** adicionar lifecycle de processos, Ring 3,
  revogação/teardown IPC e supervisor independente antes de restart; testar
  contenção e loop de falha em QEMU antes de aprovar a fase.

### ADR-0025 — Registry bounded, PCI read-only e UART COM1 (ACEITA PARA FASE 14 PARCIAL)

- **Contexto/problema:** o kernel usava COM1 diretamente para logs, mas não
  possuía lifecycle de drivers nem inventário de funções PCI; recursos como
  MMIO, interrupções gerais e DMA ainda não têm mediação segura.
- **Alternativas consideradas:** criar interfaces sem um dispositivo testado;
  implementar controladores PCI com writes a BAR/command sem resource manager;
  reutilizar COM1 como primeiro driver concreto e limitar PCI a leituras
  verificáveis em QEMU.
- **Evidências:** host tests validam registry, startup/cleanup e scanner com
  configuração injetável. QEMU 11.1.2/EDK2 q35 executou serial COM1 e detectou
  seis funções em PCI bus 0. Nenhum BAR foi escrito.
- **Decisão:** adicionar registry estático de 8 drivers/32 devices, binding
  exclusivo e cleanup explícito; enumerar apenas bus 0 com PCI config
  mechanism #1 somente leitura; registrar/inicializar COM1 antes da atividade
  geral do scheduler.
- **Consequências positivas:** há um driver real exercitado e inventário de
  hardware com identificação verificável; falha de inicialização retorna
  explicitamente, limpa recursos e entra em FAILED.
- **Consequências negativas:** drivers permanecem no Ring 0; inventário é
  limitado e apenas bus 0; callback registry não é SMP-safe; driver COM1 é
  dependente de legado.
- **Riscos residuais:** não há contenção de driver comprometido; BARs são
  metadados não validados para uso; ausência de IRQ routing, MMIO e DMA impede
  operar os dispositivos PCI descobertos.
- **Critérios de revisão:** implementar resource manager e validação/reserva
  BAR antes de mapear MMIO; percorrer bridges com limites; adicionar driver PCI
  virtual sem bus mastering; só habilitar IRQ/DMA após handler, teardown,
  buffers e IOMMU/endereço DMA serem testados.

### ADR-0026 — PIT clocksource e timer queue limitada (ACEITA PARA FASE 15 PARCIAL)

- **Contexto/problema:** a Fase 10 fornecia interrupção de PIT para preempção,
  mas não expunha relógio monotônico nem deadlines reutilizáveis. TSC não foi
  validado para frequência/invariância ou sincronização; ACPI não é passado no
  boot-info.
- **Alternativas consideradas:** TSC sem calibração/CPUID verificados; HPET ou
  APIC timer sem discovery/driver; usar PIT IRQ0 já inicializado; não fornecer
  relógio geral até obter outra plataforma.
- **Evidências:** host tests exercitam conversão racional, overflow, clamp,
  ciclo de vida de timers, cancelamento durante firing e limites. QEMU
  11.1.2/OVMF q35 com uma vCPU demonstrou IRQ0 contínuo após preempção e
  expiração one-shot de 30 ms; quantum nominal 10.000.150 ns e lateness 453 ns
  em uma amostra.
- **Decisão:** expor ticks PIT como nanossegundos monotônicos coarse; manter
  pool estático de 32 deadlines com handles generacionais; somente executar
  callbacks no loop idle, fora da IRQ. Reusar PIT em modo timekeeping sem
  preempção depois do autoteste de scheduler.
- **Consequências positivas:** fonte e mecanismo de interrupção são
  explicitamente distintos; sem TSC presumido, heap/allocator ou callbacks de
  timer em IRQ; conversões/overflow e ciclo de vida têm testes.
- **Consequências negativas:** resolução limitada ao quantum PIT; callback
  dispatch depende do idle loop; pool e sincronização são single-core; uma
  amostra QEMU não demonstra latência determinística.
- **Riscos residuais:** no-sleep scheduler não pode ser despertado por
  deadlines; timeouts de IPC/drivers, civil time, ACPI/energia, SMP e hardware
  físico continuam ausentes. `orion_boot_info` não fornece RSDP/configuration
  tables, e nenhum estado ACPI foi inspecionado.
- **Critérios de revisão:** adicionar espera temporizada somente junto de
  estados de bloqueio/wakeup e teardown testados; selecionar outra fonte após
  detecção/validação de hardware; aceitar ACPI apenas com discovery e checagem
  de limites/checksum; não declarar jitter garantido sem distribuição medida.

### ADR-0027 — ATA PIO e ORIONFS com formatação explícita (ACEITA PARA FASE 16 PARCIAL)

- **Contexto/problema:** o kernel ainda não tinha I/O persistente, mas o
  scheduler não fornece wait/wakeup geral e o framework PCI só enumera
  configurações. Uma implementação integrada precisava permanecer síncrona,
  limitada e testável sem alterar discos desconhecidos.
- **Alternativas consideradas:** adiar armazenamento até haver VirtIO e
  sincronização bloqueante; usar dispositivo de bloco de QEMU com ATA legado
  presente no ambiente; importar um filesystem existente; formatar
  automaticamente durante o boot. Foi escolhido o dispositivo ATA disponível
  e um formato ORION próprio; automount/format automático foi rejeitado.
- **Decisão:** primeiro driver primary-master ATA PIO LBA28, setor de 512
  bytes, polling bounded e cache flush; block layer valida contratos; ORIONFS
  v1 usa superbloco CRC, bitmap, 64 inodes e seis blocos diretos por arquivo;
  VFS é apenas fachada interna de kernel. Formatador de imagem exige ação
  explícita, e substituir imagem requer `-Force`.
- **Evidência:** host tests cobrem limites, conteúdo, metadados, corrupção,
  erros I/O e fail-closed. QEMU 11.1.2/OVMF q35 identificou 32.768 setores
  numa imagem dedicada de 16 MiB; primeiro boot escreveu prova, processo QEMU
  novo recuperou e comparou os mesmos bytes. Boot normal sem disco passou.
- **Consequências/limites:** filesystem suporta no máximo 4096 blocos (2 MiB),
  não há tabela de partições, journal, cache, atomicidade, locking SMP,
  isolamento, descritores de usuário, timeouts pelo clocksource ou recuperação
  garantida após power loss. A prova entre boots valida somente a imagem raw
  em QEMU/OVMF e não constitui garantia de durabilidade para hardware físico.
- **Critérios de revisão:** antes de ampliar suporte, comparar VirtIO/PCI com
  o caminho ATA e criar testes de erro/recuperação. Antes de alegar integridade
  contra falha abrupta, adicionar journaling ou protocolo transacional e
  injeção de power-cut/reboot com verificação do filesystem. Só declarar
  dispositivos/formatos efetivamente exercitados.

### ADR-0028 — NE2000 PIO e pilha de rede limitada (ACEITA PARA FASE 17 PARCIAL)

- **Contexto/problema:** a Fase 14 descobre PCI, mas o kernel ainda não tem
  mapeamento MMIO genérico, DMA para memória de sistema, roteamento de IRQ de
  dispositivo, filas assíncronas, user mode ou sincronização SMP. A Fase 15
  fornece clock monotônico PIT; as fases 11–12 ainda são internas ao kernel.
- **Alternativas consideradas:** E1000/virtio-net com anéis e DMA, adiamento
  da rede até haver DMA/IOMMU, ou NE2000 PCI com acesso programado por I/O. Foi
  escolhido `ne2k_pci`/RTL8029 no QEMU porque oferece NIC utilizável por meio
  de I/O PIO e memória interna da placa, evitando presumir que endereço virtual
  seja endereço DMA ou habilitar IRQ de dispositivo não validada.
- **Decisão:** adicionar a interface de rede genérica e uma pilha estática e
  bounded para Ethernet II, ARP, IPv4 sem fragmentação, ICMPv4 Echo e UDP.
  Endereço estático é configurado apenas na imagem de teste. A integração é
  síncrona no idle context; chamadas de usuário, sockets, DHCP, TCP, IPv6,
  firewall e autenticação permanecem fora do escopo.
- **Evidência:** host tests verificam protocolos, checksums, dados malformados,
  limites, endpoints, timeouts finitos e falha de transmissão. QEMU 11.1.2 /
  OVMF q35 detecta PCI `10ec:8029`; script com rede user/slirp recebe 22 bytes
  do host em `127.0.0.1:40170` e devolve resposta idêntica do guest
  `10.0.2.15:40171`. Ping QEMU ao gateway respondeu 1/1; RTT foi quantizado
  pelo PIT coarse e não sustenta alegação de latência. Boot normal sem NIC não
  é fatal.
- **Consequências/limites:** máximo 1518 bytes por frame, MTU 1500, payload
  UDP 1472, cache ARP 16 entradas/60 s, oito endpoints UDP e sem filas
  ilimitadas. O driver faz polling PIO e usa memória no dispositivo; o stack
  não é SMP-safe. ARP não é autenticado. Uma configuração QEMU não comprova
  funcionamento em hardware físico, desempenho, segurança geral ou
  compatibilidade ampla. TCP não é exposto parcialmente.
- **Critérios de revisão:** exigir DMA mapper/IOMMU e ownership verificado
  antes de driver que use DMA em memória de sistema; exigir IRQ routing e
  processamento seguro antes de ativar interrupções de NIC; somente adicionar
  TCP após implementação confiável de estados, ACK, retransmissão, fluxo,
  timers e testes de perda/reordenação; não habilitar sockets até existir
  isolamento e API de usuário validada.
