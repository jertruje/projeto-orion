# Alternativas arquiteturais e decisão-base

**Estado:** arquitetura-base selecionada pela Fase 3; custos e propriedades
continuam sujeitos a validação em implementação. Não há benchmark ORION nem
protótipo IPC para resolver compromissos de desempenho.

| Critério | Monolítico | Microkernel | Híbrido | Componentes/capacidades |
|---|---|---|---|---|
| Superfície de ataque | Muitos serviços/driver podem compartilhar ring 0; superfície privilegiada tende a ser maior, dependendo da implementação. | Kernel pode ser pequeno, mas IPC, drivers e serviços ainda expõem interfaces atacáveis. | Varia conforme o que permanece privilegiado; não há definição universal de híbrido. | Autoridade explícita pode reduzir acesso implícito; implementação de capacidades também pode conter falhas. |
| Isolamento de falhas | Falha em subsistema privilegiado pode comprometer kernel inteiro. | Serviços isolados podem falhar sem derrubar kernel, se dependências e estado forem isolados. | Parcial e dependente de fronteiras concretas. | Componentes com autoridade limitada podem restringir blast radius; requer granularidade e revogação corretas. |
| Comunicação | Chamadas internas/barreiras; baixa sobrecarga comum, custo de fronteiras no kernel. | IPC, troca de contexto e cópias/mapeamentos podem custar em caminhos frequentes. | Mistura de chamadas internas e IPC. | Pode ser sobreposto a qualquer opção; operações mediadas podem custar mais que autoridade implícita. |
| Complexidade inicial | Maior acoplamento possível, mas início funcional pode ser simples. | Contratos, IPC, scheduler e serviços separados exigem infraestrutura precoce. | Semântica híbrida pode aumentar número de caminhos e acoplamento. | Modelo de objetos/direitos, lifecycle, transferência e revogação é complexo. |
| Depuração | Caminhos diretos; corrupção compartilhada dificulta localizar origem. | Fronteiras ajudam atribuição, mas bugs de protocolo, corrida e timeout distribuem o diagnóstico. | Diagnóstico atravessa fronteiras de privilégio. | Auditoria de autoridade é explícita; bugs no kernel de capacidades têm alto impacto. |
| Portabilidade | Camadas específicas ainda podem contaminar APIs do kernel. | Núcleo pequeno potencialmente facilita portar, mas drivers/serviços continuam dependentes. | Depende do que foi acoplado à arquitetura. | Conceitos portáveis; primitivas de hardware e memória não deixam de ser específicas. |
| Drivers | Modelo mais comum em OS general-purpose; driver defeituoso tem privilégio elevado. | Drivers em user mode podem ser reiniciados; DMA e caminhos críticos precisam de mediação. | Alguns drivers dentro e outros fora. | Autoridade de device/DMA pode ser concedida por capability, se IOMMU e lifecycle forem corretos. |
| Desempenho potencial | Caminhos internos podem reduzir overhead; contenção e grande TCB podem limitar previsibilidade. | IPC e troca de contexto podem adicionar overhead; batching, shared memory e desenho do caminho podem reduzir custo. | Pode otimizar hot path; risco de fronteiras inconsistentes. | Verificação de capability e indireção têm custo mensurável; não há resultado presumido. |
| Manutenção | Interfaces internas acopladas podem tornar mudanças arriscadas. | Interfaces menores podem permitir substituição, com custo de versionar protocolos. | Pode combinar flexibilidade e complexidade. | Direitos explícitos podem melhorar auditabilidade, mas aumentam trabalho de API e lifecycle. |
| Verificação formal | Grande TCB eleva esforço, embora partes possam ser verificadas. | Núcleo menor pode ser mais tratável; sistema inteiro e drivers continuam fora se não especificados. | Superfície privilegiada variável. | Propriedades de autoridade são formalizáveis, mas prova depende de modelo/implementação. |

| Critério adicional | Monolítico | Microkernel | Híbrido | Componentes/capacidades |
|---|---|---|---|---|
| Isolamento de serviços | Serviços no kernel compartilham estado privilegiado; serviços em user mode ainda podem ser separados. | Serviços em processos distintos podem ter domínios separados, sujeitos ao modelo IPC. | Varia conforme placement concreto. | Grants hierárquicos podem limitar comunicação/recursos; bootstrap e policy continuam necessários. |
| Gerenciamento de memória | Kernel centraliza VM e memória física; drivers privilegiados podem ter acesso amplo. | Kernel controla objetos/mapeamentos; processos recebem espaços isolados por MMU. | Placement de memória e policy mistos. | Rights podem mediar frames e mappings, mas não substituem MMU, ownership ou TLB discipline. |
| Escalonamento multicore | Kernel escalona e sincroniza threads SMP; escala depende da implementação. | Kernel também escalona e sincroniza; serviços adicionam estados concorrentes e IPC. | Depende das fronteiras privilegiadas. | Budget/authority pode ser explícito, mas não elimina races e migração. |
| Custo de desenvolvimento | Bring-up pode ser direto; integração privilegiada aumenta custo conforme cresce. | Exige processos, IPC, lifecycle e bootstrap cedo. | Precisa manter vários placements e exceções. | Objects, rights, transfer and revocation require design and tests. |
| Custo de manutenção | Interfaces internas podem acoplar mudanças amplas. | Contratos explícitos favorecem substituição, mas exigem versionamento. | Atualizações atravessam boundaries heterogêneas. | Rights explícitos auxiliam auditoria, mas mudanças de API/lifecycle afetam dependências. |
| Complexidade de segurança | Mais código privilegiado exige hardening e revisão. | Menor core potencial, com superfícies adicionais em IPC e serviços. | Cada exceção requer justificativa própria. | Least authority é possível; confused deputy, delegação e revogação continuam riscos. |

As avaliações são tendências de desenho, não propriedades inevitáveis nem
resultados medidos. “Híbrido” não é uma especificação suficientemente precisa
para pontuar sem definir componentes privilegiados.

### Combinações de capabilities e arquitetura de kernel

Capabilities e componentes descrevem autoridade/decomposição e podem ser
combinados com mais de um placement de kernel; não são uma quarta classe
mutuamente exclusiva de kernel.

| Composição | Aplicação possível | Compromisso principal | Resultado para ORION |
|---|---|---|---|
| Monolítico + capabilities | Syscalls e objetos podem exigir rights explícitos, embora drivers/serviços ainda compartilhem ring 0. | Controle de autoridade não contém por si só falhas de código privilegiado. | Não selecionada: menor alinhamento com contenção de falhas de drivers/serviços. |
| Microkernel + capabilities | Kernel mínimo medeia objetos/recursos; serviços e drivers separados recebem rights atenuados. | IPC, lifecycle e authority model adicionam complexidade; DMA exige proteção própria. | Selecionada por alinhamento aos requisitos de contenção e autoridade mínima. |
| Híbrido + capabilities | Componentes privilegiados e isolados recebem authority explícita. | Exceções e caminhos mistos ampliam a superfície de revisão. | Permitida somente por ADR com necessidade, custo e testes especificados. |

Esta escolha não significa que capabilities sejam invioláveis nem que seu
custo seja aceitável para qualquer workload.

## Evidência contextualizada consultada

- A documentação Linux de [kernel self-protection](https://docs.kernel.org/security/self-protection.html)
  descreve mitigação por permissões de memória, redução de superfícies e
  separação kernel/user; a de [LSM](https://docs.kernel.org/admin-guide/LSM/index.html)
  documenta hooks de política no kernel. Isso demonstra que mecanismos de
  hardening/policy podem coexistir com modelo monolítico; não prova equivalência
  de isolamento de falhas com serviços em processos.
- A documentação Linux do [device model](https://docs.kernel.org/driver-api/driver-model/overview.html)
  descreve uma hierarquia uniforme de dispositivos/buses e callbacks comuns.
  A página consultada informa redação em 2002 e atualização em 2006; é evidência
  conceitual/histórica, não inventário completo do estado atual dos drivers.
- A documentação Linux de [EEVDF](https://docs.kernel.org/scheduler/sched-eevdf.html)
  informa transição desde Linux 6.6 e descreve elegibilidade/virtual deadline.
  A página [CFS](https://docs.kernel.org/scheduler/sched-design-CFS.html) é
  histórica; portanto a proposta ORION não trata CFS como estado atual nem
  importa números ou política do Linux.
- O [tutorial de threads seL4](https://docs.sel4.systems/Tutorials/threads)
  descreve TCBs, prioridade/MCP e round-robin entre threads elegíveis de mesma
  prioridade. É documentação de uma API/modelo específico, não comparação de
  desempenho.
- A documentação Fuchsia do [driver framework](https://fuchsia.dev/fuchsia-src/concepts/drivers/driver_framework)
  descreve driver manager/index/hosts e nota que drivers co-localizados no
  mesmo host compartilham address space. Assim, “driver em user mode” sozinho
  não define o domínio de falha; host e co-location importam. A mesma página
  informa que a ABI ainda não atingiu estabilidade.
- A documentação de [testes do kernel Linux](https://docs.kernel.org/dev-tools/testing-overview.html)
  distingue teste unitário white-box de teste de sistema por interface; isso
  informa a estratégia de evidência, sem adoção de código.

Essas fontes foram acessadas em 2026-10-15 e estão catalogadas com seus limites
em `research/evidence-register.md` e `research/phase-2-evidence.md`. A cobertura
não é exaustiva: MINIX 3 tem evidência oficial resumida e não há benchmark comum
entre os seis sistemas. As lacunas constam em `research/research-gaps.md`. A
seleção abaixo decorre dos requisitos do ORION, não de uma conclusão empírica
de superioridade.

## Arquitetura-base selecionada

Adotar um **microkernel de mecanismos mínimos com capabilities como modelo de
autoridade**, mantendo filesystem, rede, política de dispositivos, drivers,
supervisor e GUI em user mode por padrão. Esta seleção orienta interfaces e
implementação futura. Não significa que as fronteiras já existam, nem que
ORION seja mais seguro ou rápido que Linux.

### Justificativa e incerteza

- Corresponde aos objetivos de isolamento e recuperação, sem garantir que os
  atinjam.
- Capabilities podem tornar concessões de autoridade explícitas, mas adicionam
  complexidade e precisam de testes contra confusão de deputy, revogação e
  corrida.
- Boot bring-up atual ainda não tem IPC, modo usuário ou memória própria; logo
  não informa o custo real.
- A pesquisa da Fase 2 é documental e parcial; faltam benchmarks homogêneos e
  cobertura aprofundada de alguns projetos.
- O scaffold não tem processos, VM própria, scheduler ou IPC; não permite
  validar isolamento, recuperação ou custo.

### Critérios de revisão da decisão

1. Implementar user mode, capabilities e IPC e executar testes negativos de
   autoridade, isolamento e teardown.
2. Medir round-trip, throughput e jitter em
   configuração fixa.
3. Comparar serviço/driver isolado com implementação privilegiada
   equivalente, incluindo falha injetada e custo de recuperação.
4. Medir TCB privilegiado por método consistente, não apenas por linhas de código
   isoladas.
5. Revisar ameaça e contratos após os resultados; alterar ADR-0009 se evidência
   contradisser as premissas ou mostrar custo/risco inaceitável.

Esses critérios permitem revisão com dados, mas não impedem a seleção
documental do baseline. Até que os testes existam, as propriedades de segurança
e desempenho continuam metas não demonstradas.
