# Plano de benchmarks futuros para ORION OS

**Estado:** plano de baseline futuro; não há resultados ORION medidos.

## Evidência pública examinada

Esta pesquisa localizou um microbenchmark público seL4, mas não uma série que
compare os seis sistemas no mesmo hardware, versão, configuração e workload.
Portanto, nenhum número heterogêneo foi convertido em ranking ou inserido como
baseline ORION. O material consultado inclui:

- Linux EEVDF: documentação do algoritmo e de sua evolução (EVD-011), sem
  benchmark quantificado;
- seL4 IPC tutorial e prova formal: descrição de mecanismo/garantias (EVD-009,
  EVD-014), não substitutos de medição de latência;
- Fuchsia driver framework: descreve caminho IPC e alternativa in-process
  entre drivers co-localizados, explicitando trade-off, mas sem números
  comparáveis nesta consulta (EVD-015);
- seL4 `sel4bench-results`: uma execução reporta cross-address-space
  zero-word `seL4_Call`, média 782 cycles, SD 2.9, n=16, datada 2026-01-27.
  Plataforma identificada como `PC99_64_haswell3`; não foi recuperado SKU/clock
  exato e a medida do projeto não equivale a benchmark de aplicação (EVD-025).
- Liedtke SOSP 1993 e OSR 1995 são referências de microkernel/IPC, mas ACM
  retornou 403 para o texto integral; nenhum número histórico foi reproduzido
  sem verificar as condições (EVD-027);
- Linux test guide e syzkaller: descrevem ferramentas de validação/fuzzing
  (EVD-003, EVD-023), não performance.

Papers históricos de microkernel só devem entrar numa tabela numérica se
hardware/clock, versão, compiler, flags, workload/payload, repetições,
estatística e método forem recuperados do paper. Na ausência de qualquer desses
dados, registrar o resultado como “publicado, condições insuficientes para
comparação”, sem transportar o valor.

## 1. Categorias de benchmark

### a) Base do sistema
- boot time,
- interrupt latency,
- context switch time,
- thread wakeup time,
- idle overhead.

### b) IPC e serviços
- synchronous round-trip,
- asynchronous buffered throughput,
- waiting queue latency,
- capability invocation cost,
- message copy overhead.

### c) Memória
- allocation latency,
- fragmentation,
- page fault cost,
- heap/arena allocation under contention,
- TLB/cache behavior.

### d) Drivers e I/O
- storage read/write latency,
- network packet latency,
- DMA interrupt latency,
- driver restart time.

### e) Recuperação
- service crash recovery time,
- driver restart success rate,
- state validation after recovery,
- rollback time after interrupted update.

### f) Desempenho sob isolamento
- chamada privilegiada direta versus round-trip por IPC em funções equivalentes;
- cópia de mensagem versus shared-memory transfer por tamanhos explícitos;
- driver in-process versus driver-host isolado;
- custo de capability lookup/transfer/revocation, se implementado.

## 2. Métricas mínimas

- p50, p95, p99
- throughput
- jitter
- CPU utilization
- failure rate
- recovery time
- error after crash
- payload-specific results

Guardar amostras brutas e estatísticas além do resumo; quantis sem número de
observações e método são difíceis de interpretar. Fixar unidade, relógio e
warm-up; documentar regra pré-definida para amostras excluídas.

## 3. Ambiente de medição

- Ambiente fixado e documentado.
- QEMU para desenvolvimento inicial e hardware real para validação final.
- Configuração explícita da CPU, RAM, boot param, serial console, firmware, driver version.
- Repetição mínima dependente da métrica; documentação do número de execuções.
- Fixar commit/tag, compiler/linker/flags, firmware, CPU model/microcode, RAM,
  QEMU/accelerator/machine, configuração e logs; registrar variações.
- Medir QEMU e hardware físico separadamente, sem agregar os conjuntos.
- Usar baseline Linux com distribuição/kernel/config identificados e workload
  funcionalmente equivalente; declarar diferenças e recusar comparação quando
  equivalência não puder ser estabelecida.

## 4. Regras de benchmark

- nunca medir sem definir workload,
- nunca usar benchmark amplo como base para reivindicação global,
- nunca misturar QEMU e hardware físico na mesma comparação,
- registrar se a função ainda não existe como “não aplicável”;
- pré-definir hipótese, métrica primária, payload, repetições, regra de parada
  e tratamento de incerteza;
- medir overhead de logger/trace separadamente quando não fizer parte da carga;
- disponibilizar comandos, scripts, logs brutos, hashes e ambiente quando
  tecnicamente possível;
- classificar resultado como medido, não medido, falhou, não implementado ou
  não comparável; esses estados não são intercambiáveis.

## 5. Lista específica para implementação futura

1. Boot path measurement
2. Timer and interrupt latency
3. Context switch latency
4. IPC empty message roundtrip
5. IPC throughput and queue length
6. Memory allocation performance
7. Page fault latency
8. Service recovery latency
9. Driver restart containment
10. Storage read/write performance
11. Network transmit/receive latency
12. Update interruption and rollback behavior

### Definição operacional inicial

| Benchmark | Medida primária | Método futuro |
|---|---|---|
| IPC | tempo round-trip por payload; mensagens/s | cliente/servidor; payload vazio, pequeno, limite e acima do limite; registrar falhas e carga |
| Context switch | tempo de yield até retomada | duas threads com afinidade controlada; distinguir mesma/diferente address space |
| Scheduler | CPU share, wake-to-run e jitter | concorrentes controlados; distribuição por thread, não só média |
| Memória | latência alloc/free, fragmentation e page-fault service | tamanhos/padrões fixos; sem e sob contenção; verificar estado final |
| Latência sob carga | p50/p95/p99/max e deadline misses | carga de fundo especificada; registrar throughput e CPU |
| Recuperação | tempo até serviço saudável, sucesso e integridade | crash injection/restart; health-check e dados de referência definidos |
| Armazenamento | latência/throughput read/write/flush e integridade | dispositivo, cache mode, block size, sync semantics e dataset fixos |
| Rede | throughput, latency e loss | peer, link, MTU, protocolo, tamanhos e offloads registrados |
| Inicialização | tempo por marco e boot-to-ready | definir “ready”; separar firmware/loader/kernel/services e boot frio/quente |

## 6. Conclusão

O benchmark do ORION precisa começar com um conjunto pequeno, reproduzível e bem documentado. Isso deve preceder qualquer alegação de superioridade técnica.
