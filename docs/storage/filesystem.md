# Fase 16 — armazenamento em blocos e ORIONFS

## Estado e superfície implementada

A Fase 16 está **parcial**, validada somente em host e em QEMU/OVMF q35 com um
disco IDE virtual dedicado. O caminho atual é:

```text
VFS interno → ORIONFS v1 → block layer → ATA PIO primary-master
```

O VFS é uma fachada de chamadas internas do kernel. Não há syscalls, descritores
de arquivo em user mode, mount namespace nem autoridade de dispositivo mediada
por capabilities.

### Contratos e limites

- Block layer valida disponibilidade, read-only, faixa de blocos, overflow,
  buffer e alinhamento antes de encaminhar I/O.
- ATA PIO usa canais legacy primary-master (portas `0x1F0`–`0x1F7` e `0x3F6`),
  IDENTIFY, LBA28, setores de 512 bytes, polling com limite e cache flush.
  Operações são síncronas; não usa DMA nem IRQ de dispositivo.
- ORIONFS v1 codifica metadados little-endian, possui superbloco com CRC32,
  bitmap, 64 inodes e raiz. São permitidos nomes de até 31 bytes, caminhos
  absolutos limitados, até oito componentes e arquivos com seis blocos diretos
  (3 KiB). A tabela de alocação suporta no máximo 4096 setores, portanto o
  filesystem usa no máximo 2 MiB de um dispositivo que pode ser maior.
- O formato não tem MBR/GPT, journal, cache, diretórios indexados, links,
  permissões, atomicidade, recuperação de power-loss ou garantia de persistência
  em hardware físico. Uma falha de I/O invalida a montagem em memória; remount
  pode detectar corrupção, mas não restaura dados nem assegura consistência.
- O caminho foi construído para o atual kernel single-core e trusted kernel
  buffers. Não é seguro para SMP nem para buffers de processos hostis.

## Política de formatação e boot

O boot normal apenas tenta identificar o primary-master ATA. Disco ausente é
opcional e não impede o kernel de inicializar. O boot normal não monta,
formata nem grava filesystem automaticamente; o teste especial de persistência
monta somente a imagem explicitamente passada ao harness e grava um arquivo de
prova.

O formatador opera apenas numa imagem raw existente e alinhada. Para preparar
a imagem descartável usada pelo QEMU:

```powershell
mingw32-make prepare-storage-tool
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts\prepare-storage-image.ps1 `
  -ImagePath build\orionfs-test.raw -SizeMiB 16 -Format
```

Se o caminho já existir, o script recusa sobrescrevê-lo. A substituição exige
`-Force` além de `-Format`, e destrói todo conteúdo daquela imagem. Use apenas
uma imagem de teste dedicada; não passe discos de usuário.

## Testes e evidência

```powershell
mingw32-make test-unit
mingw32-make test-boot
mingw32-make test-storage-qemu
```

`test-storage-qemu` exige `-AllowWrite` no script interno, usa a imagem
`build\orionfs-test.raw` já formatada, copia um loader/kernel de teste para uma
ESP isolada e executa dois processos QEMU sequenciais. O primeiro grava e
sincroniza `/phase16-persistence-proof`, fecha o handle e desmonta ORIONFS; o
segundo relê e compara byte a byte a mesma imagem. O teste **não formata** a
imagem. O harness encerra o processo QEMU depois da janela de observação; os
dados foram flush/unmount pelo kernel, mas isso não simula falha abrupta de
energia nem guest shutdown gracioso.

No ambiente observado, `mingw32-make test-unit build
build/storage-test-kernel.elf`, `mingw32-make test-storage-qemu` e
`mingw32-make test-boot` passaram. O QEMU 11.1.2/OVMF q35 identificou 32.768
setores na imagem raw de 16 MiB; o segundo processo QEMU recuperou o payload
gravado pelo primeiro. Essa evidência não deve ser generalizada para outras
máquinas, controladores, firmware, formatos ou falhas de energia.

## Próximos gates

1. Injetar falha parcial em cada escrita de metadados e validar remount,
   corrupção reportada e limites de perda sem alegar transação.
2. Escolher e medir uma alternativa de dispositivo (por exemplo VirtIO block)
   antes de adicionar descoberta PCI/MMIO ou ampliar compatibilidade.
3. Definir journaling/commit protocol e testar reboot/power-cut simulado antes
   de qualquer alegação de atomicidade ou recuperação de atualização.
4. Integrar handles a syscalls e capabilities somente após existir user mode,
   cópia/validação de buffers e lifecycle de processos.
5. Adicionar timeouts e sincronização concorrente ao amadurecer wait/wakeup e
   SMP; o driver atual permanece polling synchronous em single-core.
