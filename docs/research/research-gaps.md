# Lacunas de pesquisa e questões pendentes

**Estado:** pesquisa documental da Fase 2 concluída no escopo definido; as
questões abaixo permanecem como dependências de especificação, protótipo e
validação, não como resultados presumidos. O registro por fonte está em
`evidence-register.md`; comparações sem cobertura suficiente estão marcadas
explicitamente em `architecture-comparison.md`.

## 1. Questões que esta pesquisa não pode resolver sem implementação

| Lacuna | Por que não é possível concluir só por pesquisa | Gate/experimento necessário |
|---|---|---|
| Custo de IPC e boundary | Varia com protocolo, payload, cópia, CPU, runtime e contenda. | Prototipar round-trip e comparar operação funcionalmente equivalente sob ambiente fixo. |
| Context switch, scheduler, justiça e jitter | Algoritmo/documentação não determina comportamento no ORION nem em workload real. | Implementar em fases de threads/timer; medir wake-to-run, CPU share, percentis e jitter. |
| Delegação/revogação de capabilities | Sem objeto/API ORION, detalhes semânticos e custos são desconhecidos. | Formalizar authority/lifetime e testar rights negative, revoke, stale handles e exhaustion. |
| User-mode boundary e isolamento de memória | Ainda não há address spaces/VM manager ORION implementados. | Implementar page tables e fault paths, executar testes de isolamento negativos. |
| Isolamento/recovery de driver e DMA | Depende de classe de device, IOMMU, IRQ, estado de hardware e shutdown protocol. | Identificar hardware; testar host isolado, quiescence, DMA restriction e restart/integridade. |
| Performance de storage/rede e compatibilidade física | Não há drivers nem matriz de hardware ORION. | Selecionar dispositivos identificados; testes de leitura/escrita, flush, link, integridade e carga. |
| Boot autenticado/update/anti-rollback | Root of trust, armazenamento transacional e policy ainda não foram escolhidos. | Design/revisão de trust chain e power-cut tests antes de release/update claims. |
| Segurança do sistema completo | Kernel, services, userspace, firmware, build e policy juntos formam a composição. | Análise threat-driven por fase, negative tests, fuzzing, audit e revisão independente. |

## 2. Limitações bibliográficas restantes

- A documentação consultada do Fuchsia Components v2 foi extraída apenas como
  licença/data, sem corpo técnico; claims detalhados desse documento não foram
  usados (EVD-019).
- A documentação de Linux Device Model disponível pela URL consultada é
  explicitamente de 2002/2006. Ela foi usada apenas para o fato histórico, não
  para representar todo o driver API contemporâneo (EVD-012).
- MINIX 3 foi revisado por meio do artigo de 2006 e do source snapshot do
  Reincarnation Server; os mecanismos são documentados, mas não foi localizada
  nesta pesquisa uma medida controlada de taxa/tempo de recovery e integridade
  (EVD-007, EVD-026).
- O README SerenityOS está em branch mutável; para comparação por release é
  necessário fixar commit/tag e examinar matriz de hardware/testes (EVD-018).
- Fontes “master” ou documentação viva precisam de versão/commit fixado em
  qualquer estudo quantitativo reproduzível.
- Não há nesta comparação evidência suficiente para pontuar multicore,
  maturidade/manutenção, cobertura de hardware, benchmarks end-to-end ou
  segurança global de cada sistema. Isso está sinalizado por célula na matriz.

## 3. Métricas e benchmarks

Não há benchmark ORION nesta fase. Benchmarks públicos só são comparáveis
quando as condições de hardware, versão, configuração, workload, método e
estatística são recuperadas. A documentação pública de scheduler, IPC e driver
analisada nesta revisão não fornece por si só medidas que permitam comparar os
seis sistemas; resultados experimentais encontrados em literatura devem ficar
separados até esses metadados serem verificados. Ver `performance-analysis.md`
e `benchmark-plan.md`.

## 4. Segurança e prova

Há distinção suficiente para orientar os próximos gates: menor kernel, menos
código privilegiado, isolamento de processo, capabilities, prova formal de
kernel e assurance do OS inteiro são alegações diferentes. As limitações
específicas de prova e premissas devem acompanhar todo claim atribuído ao seL4
ou outro sistema (EVD-009). ORION não tem código implementado que possa ser
formalmente verificado ou submetido a teste de penetração nesta fase.

## 5. Perguntas para decisão/validação posterior

1. Quais propriedades de segurança serão requisitos verificáveis e quais
   hipóteses de firmware/hardware serão aceitas?
2. Qual baseline x86-64, modelo QEMU/firmware e matriz física serão mantidos?
3. Quais classes de driver exigem IOMMU e quais podem operar com fallback
   fail-closed?
4. Qual ABI de serviço/aplicação e qual nível de compatibilidade são
   necessários?
5. Que armazenamento e protocolo de update satisfazem atomicidade, recuperação
   e anti-rollback?
6. Quais workloads e sistemas de referência serão usados para comparação
   futura, com equivalência de função documentada?

## 6. Integridade e independência

Pesquisa não incorporou código de terceiros. Materiais de outros projetos
foram usados como documentação/artefatos de evidência, sem copiar
implementações para o repositório. A Fase 2 não seleciona nem altera a
arquitetura; recomendação arquitetural pertence à Fase 3 e deve permanecer
revisável pela evidência dos testes.
