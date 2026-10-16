# Análise de desempenho e benchmarks comparáveis

**Estado:** pesquisa de resultados públicos; nenhum benchmark do ORION foi
executado nesta fase, pois o kernel ainda não oferece as funções a medir.

## 1. Resultado público com metadados recuperados

| Projeto/fonte | Métrica e resultado publicado | Condições recuperadas | O que não permite concluir |
|---|---|---|---|
| seL4 `sel4bench-results`, commit `883899eeca5bd1d4a9bec25969ea44281d5d4556`, linha `2026/pc99-haswell3/PC99_64_haswell3.jsonl` (EVD-025) | `seL4_Call` cross-address-space, zero message words: média **782 cycles**, desvio padrão **2.9 cycles**, **n=16**; run datado 2026-01-27. | Série do projeto seL4; identificação publicada `PC99_64_haswell3`; métrica definida em `metrics.yml`; repositório do benchmark descreve microbenchmarks com hot cache. | Não é teste independente, workload de aplicação, wall-clock, comparação Linux/Fuchsia/Genode/MINIX/Serenity, nem target para ORION. O SKU e clock exatos não foram estabelecidos neste registro; n=16 não sustenta generalização ampla. |

O valor acima é mantido como **resultado publicado por projeto** e não como
conclusão de performance geral. Não se deve converter ciclos em tempo sem
frequência efetiva e condições de execução, nem comparar com números de outra
plataforma ou método.

## 2. Papers históricos e ausência de comparação atual equivalente

Jochen Liedtke publicou estudos primários de IPC e microkernel construction
(SOSP 1993, DOI `10.1145/168619.168633`; SIGOPS Operating Systems Review 1995,
DOI `10.1145/224057.224075`). São referências históricas pertinentes a
trade-offs de microkernel. ACM devolveu HTTP 403 às páginas/PDFs durante a
verificação desta revisão; por isso, não foram reproduzidos números,
processadores ou protocolos que não puderam ser conferidos no texto integral
(EVD-027).

Não foi encontrado, entre os resultados efetivamente verificados nesta
pesquisa, um benchmark que compare Linux, seL4, Fuchsia, Genode, MINIX 3 e
SerenityOS sob hardware, versões, build/configuração, workload e protocolo
equivalentes. A documentação de scheduler Linux (EVD-011), IPC seL4
(EVD-014), driver framework Fuchsia (EVD-015) e security/testing
(EVD-001–003) descreve mecanismo ou processo, não números comparáveis.

## 3. O que a documentação afirma — sem converter em benchmark

- Linux documenta EEVDF para seleção por elegibilidade/virtual deadline e
  evolução do scheduler (EVD-011). Não foi medido ORION contra ele.
- seL4 documenta IPC síncrono com mensagens limitadas, reply capabilities e
  caminhos de transferência por registradores/buffer (EVD-014). Tutorial não
  reporta a latência da configuração ORION.
- Fuchsia documenta driver host como processo, co-localização e runtime
  in-process para comunicação local (EVD-015). “Mais eficiente” é descrição
  do design do runtime; não é número transferível para ORION.
- Genode descreve composição e TCB por aplicação (EVD-017), mas essa
  documentação, por si, não demonstra overhead/throughput para uma configuração.
- MINIX 3 documenta monitor/restart mechanisms (EVD-026); nenhum tempo ou taxa
  de recuperação foi verificado aqui.

## 4. Regras de comparabilidade

Para qualquer resultado futuro, registrar: host/CPU model e stepping,
microcode, RAM, firmware, sistema/build/commit, compiler/linker/flags,
mitigations, configuração, workload/dataset, número de repetições, warm-up,
unidade, quantis/desvio, script e logs.

Comparar apenas a mesma função semântica. Reportar separadamente:

- latência e throughput;
- payload, fila, concurrency e cache state;
- caminho kernel-internal, syscall, IPC same-address-space e cross-address-space;
- QEMU e bare metal;
- isolado e sob carga.

Se qualquer condição essencial for desconhecida ou houver funcionalidade não
equivalente, classificar **não comparável**. Uma média isolada, boot time ou
microbenchmark não autoriza uma alegação de superioridade de sistema.

## 5. Conclusão

Há ao menos um resultado público de IPC com condições úteis para uma
reprodução específica do seL4, mas sem base para ranking cruzado. O ORION está
sem medições de performance. Critérios operacionais e cargas futuras estão em
`benchmark-plan.md`; a arquitetura e o custo real precisam ser medidos após a
implementação correspondente.
