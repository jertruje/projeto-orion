# Protocolo para comparação futura com Linux

Não há comparação de desempenho ou segurança concluída. O scaffold ORION não
tem funções equivalentes a um sistema operacional de uso geral; comparar seus
resultados atuais com Linux seria enganoso.

## Referência

Selecionar distribuição e versão suportadas, kernel/build ID, configuração,
firmware, microcode, pacotes e patches. Manter imagem imutável e publicar
manifesto. Escolher uma instalação minimizada e uma configuração prática
quando ambas forem relevantes; não misturar resultados.

## Equivalência e ambiente

- Mesmo host, CPU física/microcode, memória, firmware, estado de energia,
  temperatura, armazenamento e periféricos sempre que comparável.
- QEMU só deve ser comparado a QEMU: mesma versão, machine type, accelerator
  (TCG ou mesma aceleração), quantidade de vCPU/RAM, dispositivos, firmware e
  argumentos. Resultados virtualizados não representam bare metal.
- Compilar ambas as imagens com configurações divulgadas. Aplicar otimizações
  equivalentes quando possível; listar diferenças inevitáveis.
- Executar carga funcional equivalente. Se uma OS não implementa a operação,
  registrar “não comparável”, não zero nem falha numérica.

## Benchmarks

Para cada pergunta, publicar workload, dataset, versão, setup, duração,
warm-up, cache state, número de repetições, distribuição/percentis, energia
(se medida), logs brutos e scripts. Medir throughput e latência/tail latency;
não usar somente um score agregado. Repetir e relatar ruído/intervalo de
confiança apropriado.

## Segurança e confiabilidade

Comparar propriedades explícitas com testes equivalentes: isolamento, acesso
negado, impacto de driver/service crash, integridade após interrupção,
superfície privilegiada conforme método comum. Não comparar contagens de
vulnerabilidades sem normalizar política de reporte, janela de tempo,
severidade e exposição. Ausência de CVE ou de crash não é prova de segurança.

## Critérios para alegação

Uma alegação de superioridade só pode ser feita se:

1. requisito e métrica foram definidos antes da campanha;
2. versão, commits, configurações, workloads e dados são publicáveis;
3. comportamento funcional foi validado e condições são materialmente
   equivalentes;
4. repetição independente confirma efeito maior que ruído;
5. limitações, regressões e dimensões em que ORION é inferior também são
   reportadas;
6. revisão técnica independente não encontra viés metodológico material.

Mesmo assim, restringir a conclusão à métrica, workload, ambiente e versão
testados; nunca generalizar automaticamente para “melhor que Linux”.
