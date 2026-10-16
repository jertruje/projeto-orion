# Comparação arquitetural: Linux, seL4, Fuchsia, Genode, MINIX 3 e SerenityOS

**Estado:** comparação documental revisada em 2026-10-16. Não é ranking nem
decisão de arquitetura ORION. Os marcadores descrevem o nível de evidência
obtido nesta pesquisa, não qualidade do sistema:

- **Documentado**: fonte primária consultada descreve explicitamente o item.
- **Parcialmente documentado**: apenas uma parte do item foi examinada.
- **Não verificado**: evidência da propriedade não foi conferida nesta revisão.
- **Evidência insuficiente**: fonte consultada não permite concluir o item.
- **Não aplicável**: somente quando o eixo não se aplica ao modelo.

“Implementado” é reservado para mecanismos vistos em documentação do projeto,
não inferido a partir de metas ou marketing. Não foi auditado código nem
executado benchmark de nenhum dos seis sistemas nesta fase.

## Matriz de evidência

| Eixo | Linux | seL4 | Fuchsia | Genode | MINIX 3 | SerenityOS |
|---|---|---|---|---|---|---|
| 1. Arquitetura do kernel | Documentado: kernel Linux e seus mecanismos de autoproteção (EVD-001) | Documentado: microkernel; prova do kernel tem escopo específico (EVD-009) | Documentado: Zircon kernel mais pequeno conjunto de serviços/driver/lib userland necessários ao boot (EVD-021) | Documentado: framework componentizado com múltiplas base platforms possíveis (EVD-017) | Parcialmente documentado: site o descreve como microkernel, resto em processos protegidos de user mode (EVD-007) | Parcialmente documentado: autodescrição de OS gráfico Unix-like; detalhes internos não auditados (EVD-018) |
| 2. Separação kernel/serviços | Parcialmente documentado: kernel/user e pontos de entrada protegidos; componentes completos não comparados (EVD-001) | Documentado: IPC/endpoints e capacidades explícitas; sistema userland não abrangido pela prova do kernel (EVD-009, EVD-014) | Documentado: kernel fornece processos/threads/VM/IPC; serviços necessários ficam em user space (EVD-021) | Documentado: componentes isolados e TCB específico à aplicação como princípio (EVD-017) | Parcialmente documentado: o site oficial descreve processos protegidos em user mode (EVD-007) | Evidência insuficiente: README não permite mapear privilégio/subsistemas (EVD-018) |
| 3. Modelo de permissões/autoridade | Documentado: Linux capabilities sempre presentes e LSM para checks/políticas configuráveis (EVD-002) | Documentado: capability objects e transferência por IPC aparecem nos tutoriais (EVD-004, EVD-014) | Parcialmente documentado: driver manager roteia FIDL capabilities; modelo completo de componentes não verificado (EVD-015, EVD-019) | Documentado: atribuição de recursos e comunicação controlada no modelo de componentes (EVD-017) | Evidência insuficiente: página principal não especifica autoridade granular (EVD-007) | Evidência insuficiente: README não documenta política de autoridade em detalhe (EVD-018) |
| 4. Isolamento de drivers | Parcialmente documentado: página do device model é histórica e não mede containment atual; UIO é limitado e requer módulo kernel para ações críticas (EVD-012, EVD-022) | Não verificado: a prova do kernel não basta para afirmar isolamento de drivers de um sistema (EVD-024) | Documentado: driver host é processo; drivers co-localizados compartilham address space; runtime local evita kernel IPC (EVD-015) | Documentado como arquitetura: drivers podem ser componentes; eficácia depende da plataforma/configuração (EVD-017) | Parcialmente documentado: sistema descrito como processos protegidos; domínio e recuperação por driver não confirmados nesta fonte (EVD-007) | Evidência insuficiente: não foi verificada configuração de drivers ou seus domínios (EVD-018) |
| 5. IPC | Parcialmente documentado: há interfaces Linux, mas esta pesquisa não compara latência (EVD-003) | Documentado: endpoints, send/receive/call síncronos, mensagens limitadas e capability transfer (EVD-014) | Parcialmente documentado: Zircon inclui IPC; protocolos FIDL/driver runtime documentados na fonte de drivers (EVD-015, EVD-021) | Documentado em princípio: comunicação entre componentes; protocolo/custo específico não estudados (EVD-017) | Evidência insuficiente: protocolo não estudado nesta revisão | Evidência insuficiente: protocolo não estudado nesta revisão |
| 6. Gerenciamento de memória | Documentado conceitualmente: Linux descreve VM/page tables/TLB e self-protection (EVD-001, EVD-020) | Parcialmente documentado: prova trata operações do kernel; política de sistema/memória do produto não avaliada (EVD-009) | Documentado em alto nível: syscalls de processo e virtual memory do Zircon (EVD-021) | Parcialmente documentado: recursos físicos atribuídos a componentes; detalhes de VM não examinados (EVD-017) | Evidência insuficiente nesta revisão | Evidência insuficiente nesta revisão |
| 7. Escalonamento | Documentado: documentação descreve EEVDF e transição em versões recentes; algoritmo/configuração varia por versão (EVD-011) | Parcialmente documentado: IPC tutorial não basta; material anterior do projeto descreve prioridade e round-robin, sem medir aqui | Parcialmente documentado: kernel concepts lista threads/locking; política e fairness não examinadas (EVD-021) | Evidência insuficiente nesta revisão | Evidência insuficiente nesta revisão | Evidência insuficiente nesta revisão |
| 8. Multicore | Não verificado nesta pesquisa de fontes; não se infere escala/performance | Não verificado para releases/configuração alvo ORION nesta pesquisa | Não verificado nesta pesquisa | Não verificado; depende da base platform declarada pelo projeto | Não verificado nesta pesquisa | Não verificado nesta pesquisa |
| 9. Recuperação de falhas | Parcialmente documentado: análise aqui não prova recovery automático de kernel/driver | Evidência insuficiente: microkernel/prova não implica restart de serviços por si | Parcialmente documentado: isolamento por driver host dá process boundary; políticas/resultados de restart não verificados (EVD-015) | Parcialmente documentado: componentes isolados são mecanismo, recovery efetivo não auditado (EVD-017) | Documentado como mecanismo: Reincarnation Server monitora heartbeat e suporta restart/backoff; taxa e integridade não medidas (EVD-026) | Evidência insuficiente nesta revisão |
| 10. Inicialização segura | Evidência insuficiente: não comparada aqui por distro/firmware | Não verificado; prova do kernel não cobre firmware/root-of-trust automaticamente (EVD-009) | Parcialmente documentado: update system compara imagens/vbmeta/ZBI; cadeia completa de boot não auditada (EVD-016) | Evidência insuficiente nesta revisão | Evidência insuficiente nesta revisão | Evidência insuficiente nesta revisão |
| 11. Atualizações/reversão | Evidência insuficiente: altamente dependente de distribuição | Evidência insuficiente: depende do sistema que integra o kernel | Documentado: TUF 1.0, metadados/hash e updater/OTA; resistência a power-cut não medida aqui (EVD-016) | Evidência insuficiente nesta revisão | Evidência insuficiente nesta revisão | Evidência insuficiente nesta revisão |
| 12. Sistema de arquivos | Parcialmente documentado: diversidade não quantificada nesta revisão | Não aplicável ao kernel isolado; serviço filesystem depende da integração | Não verificado nesta revisão | Documentado como tipo de componente possível, implementações concretas não comparadas (EVD-017) | Evidência insuficiente nesta revisão | Parcialmente documentado como OS Unix-like; comportamento FS não examinado (EVD-018) |
| 13. Rede | Parcialmente documentado: sem comparar implementação/benchmarks nesta pesquisa | Não aplicável ao kernel isolado; rede depende do sistema | Evidência insuficiente nesta revisão | Documentado como stack possível de componente; suporte por plataforma não avaliado (EVD-017) | Evidência insuficiente nesta revisão | Parcialmente documentado como OS, protocolos não examinados (EVD-018) |
| 14. Compatibilidade de hardware | Parcialmente documentado: amplo ecossistema não quantificado nem inventariado aqui | Evidência insuficiente: suporte depende da plataforma, configuração e drivers | Evidência insuficiente: framework documentado, cobertura por dispositivos não medida | Evidência insuficiente: múltiplas base platforms não equivalem a matriz de dispositivos (EVD-017) | Parcialmente documentado: site afirma x86 e ARM, sem matriz/reprodução nesta pesquisa (EVD-007) | Parcialmente documentado: autodescrição para arquiteturas 64-bit; hardware suportado não auditado (EVD-018) |
| 15. Compatibilidade de aplicações | Evidência insuficiente nesta revisão; distro/ABI/workload variam | Evidência insuficiente: depende de runtime, ABI e sistema construído sobre kernel | Evidência insuficiente: não examinada nesta revisão | Documentado: pode executar aplicações nativas e containers de software legado, sem matriz avaliada (EVD-017) | Parcialmente documentado: site afirma compatibilidade com NetBSD e pacotes, sem teste independente nesta revisão (EVD-007) | Parcialmente documentado: autodescrição Unix-like; compatibilidade real não medida (EVD-018) |
| 16. Ferramentas de desenvolvimento | Documentado parcialmente: guia identifica KUnit, kselftest, coverage e analisadores (EVD-003) | Parcialmente documentado: tutoriais oficiais; cobertura de tooling não inventariada (EVD-014) | Parcialmente documentado: framework/SDK não pesquisados em profundidade | Documentado parcialmente: documentação de arquitetura e framework (EVD-017) | Evidência insuficiente nesta revisão | Parcialmente documentado: repositório expõe ferramentas/CI; profundidade não auditada (EVD-018) |
| 17. Testes e fuzzing | Documentado: KUnit, kselftest, KCOV e análise estática/dinâmica descritos (EVD-003) | Parcialmente documentado: prova formal de escopo delimitado e tutoriais; fuzzing de sistema não pesquisado (EVD-009, EVD-014) | Parcialmente documentado: testes de driver não examinados profundamente (EVD-015) | Evidência insuficiente nesta revisão | Evidência insuficiente nesta revisão | Parcialmente documentado: README lista CI/fuzzing, sem avaliação de cobertura (EVD-018) |
| 18. Verificação formal | Não é claim verificado aqui sobre sistema Linux completo; ferramentas listadas são outras classes de evidência | Documentado: proofs cobrem propriedades/configurações do kernel sob hipóteses explícitas; não prova OS completo (EVD-024) | Evidência insuficiente para claim de OS inteiro | Parcialmente documentado: arquitetura admite assurance por componente; não equivale a prova do framework completo (EVD-017) | Evidência insuficiente nesta revisão | Evidência insuficiente nesta revisão |
| 19. Benchmarks públicos | Não comparados; sem resultado citado nesta matriz | Um benchmark publicado de IPC está documentado com limites; não é comparável ao restante (EVD-025) | Evidência insuficiente de benchmark diretamente comparável | Evidência insuficiente de benchmark diretamente comparável | Evidência insuficiente de benchmark diretamente comparável | Evidência insuficiente de benchmark diretamente comparável |
| 20. Maturidade/manutenção | Evidência insuficiente para nota comparativa nesta revisão | Evidência insuficiente para nota comparativa | Evidência insuficiente para nota comparativa | Evidência insuficiente para nota comparativa | Evidência insuficiente para nota comparativa | Evidência insuficiente para nota comparativa |

## Objetivos e escopos não equivalentes

- Linux é um kernel de uso geral integrado a diferentes distribuições; documentação
  de kernel não descreve uma única política de update, desktop ou suporte de
  hardware. A existência de LSM e hardening é evidência de mitigação ativa, não
  de ausência de falhas (EVD-001, EVD-002).
- seL4 é um microkernel cuja propriedade formal estudada se refere a um kernel
  e modelo específicos, não automaticamente ao sistema que um integrador cria
  ao redor dele (EVD-009).
- Fuchsia documenta um sistema baseado em Zircon e componentes; o domínio de
  falha de drivers depende de agrupamento em driver hosts, e comunicação local
  pode usar runtime em-processo (EVD-015, EVD-021).
- Genode é framework de sistema componentizado e pode usar diferentes kernels
  como base; não deve ser confundido com uma implementação monolítica única
  (EVD-017).
- MINIX 3 é descrito por seu projeto como microkernel voltado à confiabilidade,
  com processos isolados e compatibilidade anunciada com NetBSD. Esta descrição
  não fornece avaliação independente de uso geral ou taxa de recuperação
  (EVD-007).
- SerenityOS é descrito como sistema gráfico Unix-like independente. Seu
  escopo de projeto e autodescrição não o tornam equivalente a Linux de
  produção nem comprovam cobertura ampla de dispositivos (EVD-018).

## Interpretação

1. A matriz identifica propriedade documentada versus evidência medida; não
   preenche lacunas com pontuação ou suposição.
2. “User mode”, “capability”, “formal verification” e “recovery” são dimensões
   separadas. Uma não prova automaticamente as outras.
3. A prova formal do seL4 não é evidência de que o sistema completo, todos os
   drivers, firmware ou política de update estejam provados.
4. Isolar driver por processo pode reduzir o domínio de falha, mas co-localizar
   drivers compartilha address space; IPC, restart e DMA também precisam de
   validação.
5. Não há nesta matriz benchmark que permita declarar vencedor. Ver
   `performance-analysis.md` e `benchmark-plan.md`.

## Conclusão para ORION

Esta pesquisa não decide a arquitetura da Fase 3 e não demonstra superioridade
de qualquer sistema. Ideias candidatas a prototipar incluem autoridade
explícita, limites de mensagem, serviço/driver isolável e mecanismo de teste
em camadas. Cada uma gera hipótese e custo a medir; não é conclusão de que
ORION já possui ou alcançará essas propriedades.
