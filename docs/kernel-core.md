# Núcleo inicial do kernel — Fase 6

## Implementado nesta base

- `kernel_main` executa sequência linear: inicializa a UART COM1; avança o
  estado de inicialização; valida boot-info; avança estados; registra
  informação real do mapa; informa que serviços UEFI terminaram e interrupções
  continuam desabilitadas; entra no idle loop `cli; hlt`.
- `include/orion/init_state.h` aplica transições estritamente monotônicas; o
  teste de unidade cobre sequência válida, duplicidade, enum inválida, estado
  nulo, nomes e preservação do estado quando uma transição falha.
- `include/orion/boot_info.h` define o contrato compartilhado por loader/kernel
  e valida assinatura/versão, mapa não nulo e alinhado, tamanho mínimo do
  descritor, tamanho total e múltiplo do descritor. Static asserts fixam offsets
  e tamanho ABI em 40 bytes no alvo de 64 bits.
- `kernel/console/serial.c` configura UART 16550 COM1 e faz transmissão com
  espera limitada; retorna erro se o THR não ficar disponível.
- `kernel/log/log.c` fornece níveis DEBUG/INFO/WARN/ERROR, origem, limite de
  módulo de 32 bytes e limite de mensagem de 192 bytes, sem libc. Mensagem no
  limite recebe marcador de truncamento; logger retorna `false` em falha de
  I/O ou truncamento.
- `kernel/panic/panic.c` desabilita interrupts, registra módulo de origem e
  razão numa mensagem, impede recursão de panic e chama backend `cli; hlt`
  não retornável; não reinicia automaticamente.
- `kernel/arch/x86_64/cpu_state.S` confere o ponteiro/alinhamento da pilha e
  bits de long mode/paging, PAE e IF.
- Testes host isolados cobrem logger e panic com mocks; os testes não executam
  instruções privilegiadas.

## Invariantes e limites

- A ordem de inicialização é monotônica e não permite transição duplicada na
  sequência atual; não é um framework de inicialização genérico.
- Não declarar serial pronto por simples log: o UART é configurado, mas a
  disponibilidade física do dispositivo não é testada; timeout detecta
  transmissão que não avança.
- Logger/panic não têm lock nem são reentrantes/thread-safe; são para a fase
  single-CPU com interrupts desabilitadas. Não os usar de ISR/SMP sem redesenho.
- Ponteiro do mapa só pode ser estruturalmente validado. O kernel ainda não
  tem memória virtual própria para provar que todo byte apontado é acessível.
- A verificação CPU da Fase 6 limita-se aos bits de controle e à pilha
  declarados; não audita tabelas de página, feature flags gerais ou
  acessibilidade de toda a memória. A Fase 7 adicionou GDT/TSS/IDT próprias;
  os limites constam em `kernel/gdt.md` e `kernel/idt.md`.
- Panic depende da UART estar acessível. Se I/O falhar, o comportamento final
  continua sendo halt sem diagnóstico garantido.

## Validação observada

- `mingw32-make test-unit build inspect` passou: testes boot-info/estado,
  logger e panic; build ELF64 x86-64 linkado com `--no-undefined`; inspeção
  LLVM executada sem símbolos indefinidos reportados.
- `mingw32-make test-boot` passou em QEMU 11.1.2; serial confirmou a sequência
  até `IDLE` e mensagem do idle loop. A VM permaneceu ativa durante 15 s e foi
  parada pelo harness.
- `mingw32-make test-panic-qemu` usa uma imagem de kernel de teste separada;
  confirma log fatal de origem/motivo e permanência da VM durante a janela
  observada. O teste não altera o kernel normal.
- Ver [estrutura](kernel/core-structure.md), [inicialização](kernel/initialization-sequence.md),
  [logs](kernel/logging.md), [pânico](kernel/panic-handling.md),
  [contratos](kernel/module-contracts.md), [invariantes](kernel/invariants.md)
  e [testes](kernel/testing.md).
