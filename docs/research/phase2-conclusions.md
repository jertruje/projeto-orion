# Conclusões da Fase 2

**Estado: APROVADA como pesquisa documental com limites explícitos.**
Critérios de aceitação e validação registrados ao final. Aprovação não
significa que as lacunas de implementação, benchmark, auditoria ou hardware
tenham sido resolvidas, nem que alguma arquitetura seja superior.

## 1. Escopo e fontes

A Fase 1 forneceu requisitos, métricas iniciais, ameaças e critérios para
comparação. Nesta pesquisa foram consultadas fontes oficiais de Linux, seL4,
Fuchsia, Genode, MINIX 3 e SerenityOS, além de documentação do syzkaller,
artigos publicados sobre prova formal/recovery e uma série pública de
microbenchmark seL4. A data da retomada é 2026-10-16.

O registro `phase-2-evidence.md` lista URLs efetivamente consultadas e
indisponibilidades; `evidence-register.md` contém 27 entradas estáveis com
alegação, fonte, tipo, data, limite, confiança, implicação e investigação
adicional. Páginas mutáveis, antigas, com 403 ou extração sem texto técnico
foram tratadas como tais. Não houve incorporação de código de terceiros.

## 2. Respostas às questões da fase

### 2.1 Limitações Linux demonstráveis

- Linux documenta hardening de memória, redução de superfície, restrição de
  entradas e trade-offs explícitos da autoproteção (EVD-001).
- Linux documenta LSM/capabilities como mecanismos de política e controle,
  configuráveis segundo build/boot (EVD-002).
- Linux documenta várias camadas complementares de testes, coverage, fuzzing
  e análise; seus guias não provam ausência de defeitos ou taxa residual
  (EVD-003, EVD-023).
- A documentação de EEVDF descreve política de scheduler e evolução por versão;
  não é comparativo de fairness nem benchmark universal (EVD-011).
- A evidência de driver examinada é limitada por página histórica de 2002/2006;
  UIO existe para certos dispositivos, mas pode exigir handler de kernel para
  trabalho que não pode aguardar userspace (EVD-012, EVD-022).

Não foi demonstrada frequência geral de crashes, bugs de driver ou regressões
em todas as distribuições. Reclamações de usuário sem configuração, hardware,
logs e reprodução ficam classificadas como não verificadas.

### 2.2 Trade-offs de microkernel

Documentação seL4 e Fuchsia mostra protocolos IPC, limites de mensagem,
capability transfer e opção de driver runtime local; Genode descreve
composição e TCB por aplicação (EVD-014, EVD-015, EVD-017). Isto sustenta que
fronteiras e composição são mecanismos relevantes. Não fornece medidas
universais de overhead.

Custos a verificar em implementação: IPC/cópia, troca de contexto,
provisionamento e revogação de authority, configurações de driver host,
memória compartilhada e DMA/IOMMU. Microkernel não é sinônimo de segurança ou
performance superiores.

### 2.3 Verificação formal e segurança

Há resultados formais publicados para propriedades e configurações específicas
do seL4, com cobertura variável por arquitetura e premissas (EVD-024). A
hipótese de DMA/MMU e os itens não cobertos impedem propagar a prova a firmware,
drivers, serviços, políticas ou OS completo. Kernel pequeno, menos código
privilegiado, process isolation, capability authority e formal verification
são propriedades diferentes; ver `security-analysis.md`.

### 2.4 Recuperação plausível

MINIX 3 documenta Reincarnation Server com heartbeat/restart/backoff; o
mecanismo é evidência de um desenho de recovery, não medida de taxa, tempo ou
integridade (EVD-026). Fuchsia documenta driver hosts e lifecycle, mas isso não
prova restart seguro de todos os dispositivos (EVD-015). Atualização requer
verificação, estado de ativação/fallback e storage power-fail behavior; hashes
isolados não provam atomicidade (EVD-016).

Recuperação de aplicação/serviço/driver é plausível se houver isolamento,
protocolos de lifecycle, reentrância/idempotência, contenção DMA e state
validation. Falha de kernel, firmware/hardware ou mídia pode exigir reboot ou
perda de dados. Não prometer recuperação sem mecanismo e teste correspondentes.

### 2.5 Desempenho público

Um benchmark público seL4 com metadados recuperáveis reporta
cross-address-space zero-word `seL4_Call`: 782 cycles de média, SD 2.9, n=16,
run 2026-01-27, plataforma rotulada `PC99_64_haswell3` (EVD-025). SKU/clock
exatos não foram estabelecidos neste registro. É microbenchmark do próprio
projeto, não comparação independente nem resultado de workload completo.

Não há comparação válida recuperada entre os seis sistemas com condições
equivalentes. Os papers clássicos de Liedtke foram identificados, mas ACM
retornou 403 para o texto integral; nenhum valor ou condição foi inventado
(EVD-027). Benchmark plan define protocolo futuro; ORION não foi medido.

## 3. Recomendações candidatas e grau de confiança

| Recomendação de pesquisa | Evidência | Confiança e limite |
|---|---|---|
| Testar limites de privilégio e autoridade explicitamente; não concluir segurança pelo rótulo da arquitetura. | Linux self-protection/LSM; capabilities seL4; driver hosts Fuchsia (EVD-001, EVD-002, EVD-014–015). | Alta para a necessidade de propriedades explícitas; ganho ORION não medido. |
| Especificar IPC com tamanho, bloqueio, lifetime, authority transfer e quotas antes de otimizar. | Tutorial oficial seL4 descreve send/call/reply, payload limitado e capability transfer (EVD-014). | Alta para contrato descrito; custo do protocolo ORION ainda não medido. |
| Definir isolamento de driver pelo domínio real (host/address space), IRQ/DMA e restart protocol. | Fuchsia permite drivers co-localizados compartilharem espaço; UIO demonstra limite de handler user-mode (EVD-015, EVD-022). | Alta para os fatos documentados; configuração ótima depende de classe de hardware. |
| Separar testes unitários, integração, fuzzing, verificação formal e auditoria. | Linux test guide/syzkaller e provas seL4 têm escopos diferentes (EVD-003, EVD-023–024). | Alta para diferença de método; a efetividade no ORION precisa ser validada. |
| Exigir metodologia e dados brutos antes de alegação de performance ou recovery. | Benchmark seL4 e limites de sample/platform; mecanismo MINIX sem taxa publicada nesta pesquisa (EVD-025–026). | Alta como regra de evidência; thresholds precisam de requisito/workload. |
| Projetar update com autenticidade, rollback e testes de interrupção/durabilidade. | Fuchsia documenta TUF e fluxo OTA, sem transferir guarantee ao ORION (EVD-016). | Média-alta como requisitos de projeto; trust/storage policy ORION ainda em aberto. |

## 4. Abordagens rejeitadas por falta de evidência/custo conhecido

- Rejeita-se declarar microkernel, capabilities ou formal verification como
  garantia automática de segurança, performance ou recovery.
- Rejeita-se escolher vencedor por tabela qualitativa ou por um benchmark
  histórico sem hardware/workload/método compatível.
- Rejeita-se tratar reclamações isoladas de Linux como defeitos generalizados
  de kernel.
- Rejeita-se prometer driver recovery sem quiescence de hardware/DMA, validar
  estado e tratar requisições em voo.
- Rejeita-se afirmar compatibilidade de hardware/aplicações com base em
  objetivos de projeto ou autodescrições dos projetos comparados.

## 5. Questões abertas para arquitetura e fases futuras

1. Quais requisitos mensuráveis justificam fronteiras de serviço e que custo
   IPC é aceitável no workload escolhido?
2. Qual mecanismo de capability, delegação, revocation e quota é implementável
   e testável?
3. Que drivers podem ser isolados por processo, e quais dependem de kernel
   handling, IOMMU ou trusted hardware?
4. Qual root-of-trust, update transaction e rollback policy atende ao target?
5. Quais workloads Linux e configurações são funcionalmente equivalentes aos
   primeiros workloads ORION?
6. Quais propriedades formais são viáveis para o código ORION e qual TCB não
   estará incluída na prova?
7. Qual matriz física/firmware deve ser suportada após QEMU?

Estas são perguntas de design/implementação; a pesquisa não as resolve por
preferência nem altera uma decisão arquitetural.

## 6. Critérios de aceitação e estado

| Critério da Fase 2 | Estado | Evidência |
|---|---|---|
| Fontes principais examinadas ou indisponibilidades registradas | Atendido com limites | `phase-2-evidence.md`; rotas inacessíveis/antigas e extração insuficiente explicitadas |
| Matriz de seis sistemas e 20 eixos | Atendido como matriz de evidência, sem ranking | `architecture-comparison.md`; lacunas permanecem rotuladas |
| Problemas Linux classificados por origem | Atendido | `linux-limitations.md`; distinção entre kernel, driver, firmware, distro, desktop/app, docs e relato subjetivo |
| Segurança distingue isolamento, privilégio, capability, prova e OS inteiro | Atendido | `security-analysis.md`, EVD-024 |
| Benchmarks públicos documentam condições e limites | Atendido dentro do material verificável | `performance-analysis.md`, EVD-025; papers inacessíveis EVD-027; sem comparação cruzada |
| Plano de benchmarks futuros | Atendido | `benchmark-plan.md` com método e definição operacional |
| Registro de evidência rastreável | Atendido | 27 entradas EVD, fontes, data, categoria, confiança/limites |
| Lacunas e hipóteses explicitadas | Atendido | `research-gaps.md` e esta síntese |
| Sem alegação de superioridade sem prova | Atendido | recomendações e evidências delimitadas; nenhum ranking |
| Validação documental executada | Atendido | `scripts/validate-docs.ps1`; resultado atualizado no `phase-status.md` |

A fase está aprovada **como pesquisa documental**, com evidência suficiente
para sustentar comparação e perguntas de projeto sem reivindicar solução
definitiva. Pesquisa de hardware, benchmark cruzado, análise formal para ORION,
auditoria e fault-injection pertencem a gates posteriores e permanecem
pendentes.

## 7. Integridade e não implementação

Nenhum código de kernel/bootloader ou código de outros kernels foi
implementado ou copiado nesta fase. Esta atualização não inicia nem altera a
arquitetura da Fase 3; decisões de arquitetura são registradas separadamente e
devem ser revisitadas apenas com evidência técnica adequada.
