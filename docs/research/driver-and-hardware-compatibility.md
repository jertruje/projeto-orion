# Compatibilidade de drivers, hardware e plataforma

**Estado:** estratégia incremental e base para requisitos futuros; não é suporte implementado.

## Evidência comparativa examinada

- O framework Fuchsia descreve driver host como processo com address space
  próprio, mas permite co-localização de drivers no mesmo processo; a
  comunicação entre co-localizados pode usar runtime em-processo. A
  documentação declara ABI ainda não estável (EVD-015). Portanto, “driver em
  userspace” não define sozinho a fronteira de falha, custo ou compatibilidade.
- O Genode descreve drivers, filesystem e network stack como possíveis
  componentes, com recursos atribuídos por composição (EVD-017). Isto é
  documentação arquitetural, não evidência de suporte universal de dispositivos.
- Linux UIO permite expor regiões e interrupções a processo userspace, mas
  exige handling no módulo de kernel para ações que não podem aguardar processo
  user-mode (EVD-022). É exemplo limitado, não equivalência com um modelo
  completo de driver isolado.
- MINIX 3 anuncia isolamento em processos de user mode e compatibilidade com
  NetBSD no site do projeto (EVD-007); essa autodescrição não substitui matriz
  reproduzível de dispositivos ou taxa de recuperação.

Nenhum destes dados demonstra que ORION já suporte um dispositivo. Antes de
chamar suporte de “testado”, registrar ID/revisão de dispositivo, firmware,
target, driver/build, cenário funcional, falhas negativas, logs e resultado
repetível. QEMU e equipamento físico são ambientes distintos.

## 1. Estratégia incremental

### a) Drivers
- Fase inicial: drivers de console/serial, temporizador mínimo, memória detectada, boot devices e logs.
- Fase de expansão: bus abstrações e drivers simples de dispositivos de bloco e PCI.
- Fase de serviço: isolamento em processos/hosts, com capability-based access a MMIO, IRQ, DMA e storage.
- Critério de suporte: driver deve ter teste de inicialização, testes de erro e rotina de descarregamento/restart.

### b) USB e PCI
- Suporte inicial: discovery, enumeration e access control.
- Requisitos de hardware: IOMMU disponível, map e reservation explícita, fallback seguro.
- Critério objetivo: enumerate device tree, bind driver, perform read/write test and interrupt reception within defined timeout.

### c) Armazenamento
- Suporte inicial: block device, read/write, flush and error path.
- Critério objetivo: persistência em um volume conhecido e sinalização de corrupção por checksum/hash ou metadata validated.

### d) Rede
- Suporte inicial: NIC discovery, initialization, DMA buffer, interrupts and packet transmit/receive under test.
- Critério objetivo: medição de link up, throughput e packet loss em ambiente reproduzível.

### e) Áudio
- Operação inicial: stream minimal, rate and buffer config.
- Critério objetivo: estabilidade de buffer, latência e recuperação de overrun/underrun.

### f) GPU
- Requer driver stack, firmware and DMA constraints.
- Critério objetivo: boot console, basic drawing operations and recovery path without full-system crash.

### g) Gerenciamento de energia
- O suporte deve ser definido em fases; inicial não depende de ACPI full support.
- Critério objetivo: idle state, suspend/resume policy and power-aware timer behavior clearly specified.

### h) Interfaces de programação
- Primeira etapa: syscall/interface minimal and documented.
- Critério objetivo: semântica e segurança da syscall definidas, com lista de parâmetros e error codes.

### i) Aplicações
- Compatibilidade inicial: POSIX-like subset, then narrow runtime support.
- Critério objetivo: execução de uma aplicação de referência em ambiente validado.

### j) Interface gráfica
- Requer compositor, input stack e driver graphics stack.
- Critério objetivo: renderização básica, input event delivery e lifecycle de janela.

## 2. Compatibilidade conceitual, implementada e testada

- Compatibilidade conceitual: o sistema foi projetado para suportar alguma classe de hardware ou protocolo.
- Compatibilidade implementada: há código para a funcionalidade, mas ainda não está validado em ambiente estável.
- Compatibilidade efetivamente testada: há testes automatizados, logs e critérios de sucesso documentados.

## 3. Princípios de suporte

- Não afirmar suporte para hardware sem requisito, driver, teste de regressão e limite de atendimento.
- Considerar IOMMU/DMA e endianness, PCI IDs e CPU features.
- QEMU não pode ser tratado como substituto da compatibilidade com hardware real.

## 4. Recomendação ORION

A primeira geração do ORION deve declarar suporte em camadas, com um portfólio mínimo e revisável:

- serial/console,
- timers,
- block storage básica,
- memória e CPU baseline,
- PCI discovery simples,
- user-space minimal runtime.

Qualquer suporte de GPU, rede, USB ou desktop deve ser declarado como "experimental" até que testes e registros existam.
