# Contrato atual UEFI → kernel x86-64

**Estado:** descrição do scaffold executado em QEMU; revisão completa de
segurança do parser e do mapa ainda pendente. Este contrato não promete
portabilidade a firmware físico.

## Transferência

- Firmware executa `BOOTX64.EFI`, imagem UEFI PE/COFF x86-64.
- Loader localiza Simple File System, lê `kernel.elf`, valida campos ELF
  básicos e reserva/carrega segmentos `PT_LOAD`.
- Entry ELF está linkado em `0x200000` conforme `kernel/linker.ld`.
- Loader obtém memory map UEFI e tenta `ExitBootServices`, incluindo retry
  quando a key fica obsoleta; não usa serviços UEFI após sucesso.
- Entry `_start` é Assembly x86-64; desabilita interrupts, troca para stack
  estática do kernel alinhada em 16 bytes e chama `kernel_main`.
- No ABI utilizado, o primeiro argumento C é o ponteiro para `orion_boot_info`.
- Kernel espera assinatura `0x314f52494f4f5442`, versão 1, ponteiro de mapa,
  tamanho em bytes e `descriptor_size`. O kernel rejeita campos nulos/inválidos
  segundo verificações atuais.
- GDT/TSS/IDT próprias de exceções estão instaladas; IF permanece desabilitado
  e IRQs externos não são configurados.
- O mapa foi alocado antes de `ExitBootServices` e é passado ao kernel. O
  kernel aceita descritores UEFI com stride alinhado de pelo menos 40 bytes,
  valida limites, overflow e sobreposição e libera para alocação apenas
  `EfiConventionalMemory` abaixo de 4 GiB. Kernel/loader/boot-map continuam
  reservados por seus tipos UEFI não convencionais.
- A Fase 8 possui bitmap físico e page-table manager com testes host. No
  caminho QEMU com NX habilitado, o kernel copia o PML4 herdado para um root
  estático reservado, preserva as entradas herdadas, troca o CR3 para essa
  cópia e testa um mapeamento temporário. Isso não cria um espaço de processo
  nem estabelece um layout virtual independente.
- Kernel escreve diretamente na UART COM1 (0x3f8) do ambiente QEMU e para no
  loop `cli; hlt`.

## Limitações explícitas

- Nenhuma autenticação criptográfica do ELF, política de Secure Boot, verificação
  anti-rollback ou assinatura de imagem foi demonstrada.
- O loader não deve ser considerado parser ELF endurecido: validações contra
  overflow, segmentos sobrepostos, endereços permitidos, tamanho de imagem,
  alinhamento e casos adversariais ainda exigem revisão/fuzzing.
- O código depende de detalhes de ABI UEFI x64 e de firmware QEMU/EDK2.
- Não há garantias sobre page tables/identity map de outros firmwares. O
  kernel detecta se NX e EFER.NXE estão ativos no CPU testado, não altera
  EFER e não habilita NX por conta própria. O adaptador de paginação assume
  que suas tabelas estáticas podem ser acessadas por endereço virtual igual ao
  físico; isso passou no QEMU/EDK2 observado, mas não foi comprovado em outros
  firmwares ou hardware.
- A stack é reservada estaticamente no ELF; não há guard page.
- Um page fault não tratado continua fatal; não há demand paging ou recovery.

## Teste de aceitação observado

`scripts/test-boot.ps1` requer mensagens seriais de ELF validado, load segments
reservados, entry do kernel, boot services encerrados e kernel mínimo em
execução. A validação atual também registra o mapa UEFI e a contagem de páginas
livres e realiza allocate/release no PMM. O teste QEMU passa no ambiente
observado; repetir em CI, firmware e hardware distintos antes de ampliar o
contrato.
