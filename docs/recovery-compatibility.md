# Recuperação de falhas e estratégia de compatibilidade

## Definição operacional de recuperação bem-sucedida

Uma recuperação conta como bem-sucedida somente se: (1) a falha foi detectada
e classificada, (2) o componente afetado foi contido, (3) o sistema restante
preservou invariantes, (4) o componente/serviço voltou a responder ou o sistema
reiniciou em estado conhecido, e (5) dados foram validados contra o objetivo
definido. Mensagem de log ou retorno do QEMU, sozinhos, não demonstram
recuperação.

| Classe de falha | Recuperação sem reboot plausível? | Reboot/risco de dados |
|---|---|---|
| Aplicação isolada | Sim, finalizar/recriar processo; pode perder estado volátil. | Não precisa reboot se isolamento real funcionar. |
| Serviço de usuário | Possível se dependências e estado forem recuperáveis/idempotentes. | Clientes podem falhar; dados em transação incompleta exigem journal/validação. |
| Driver isolado | Possível para dispositivo e serviço quando DMA/IRQ/handles são revogáveis e hardware pode ser resetado. | Dispositivo pode exigir reset ou reboot; I/O em andamento pode perder dados. |
| Kernel | Geralmente não há recuperação confiável sem reboot após corrupção/estado desconhecido. | Reboot controlado ou halt; dados não sincronizados podem perder-se. |
| Armazenamento | Às vezes por retry/remount se erro transitório; não reparar hardware quebrado por software. | Falha física pode causar perda; exige backup e protocolo de integridade. |
| Atualização | A/B/transação pode reverter se imagem anterior intacta e boot policy segura. | Sem cópia válida, dispositivo pode não inicializar; dados podem ficar em estado misto. |
| Hardware/energia | Depende de redundância e semântica do dispositivo. | Falha física/energia pode interromper operações e perder dados ainda não persistidos. |

Nenhum desses mecanismos está implementado no boot scaffold.

## Níveis de compatibilidade

1. **Conceitual:** protocolo/arquitetura foi especificado; código pode não existir.
2. **Implementada:** driver/API existe, mas não necessariamente foi exercitado.
3. **Testada:** modelo/revisão, QEMU ou hardware, firmware, configuração e teste
   aprovados estão registrados.
4. **Suportada para release:** testes repetidos, falhas conhecidas, recuperação,
   regressão e política de manutenção definidas.

Não declarar “suportado” apenas por enumeração PCI ou enumeração de protocolo.

## Incremento de plataforma

| Área | Sequência sugerida | Critério mínimo antes de declarar suporte |
|---|---|---|
| UEFI/boot | QEMU/EDK2 primeiro; depois firmware físico selecionado. | Repetir boot frio/quente, validar mapa/contrato e erro de imagem. |
| PCI | Descoberta read-only → configuração controlada → driver isolado. | IDs/revisão, BAR/IRQ/DMA documentados; teste de bounds e falha. |
| USB | Controlador virtual escolhido → enumeração → armazenamento/entrada. | Dispositivo e versão, hotplug, malformed descriptors e desconexão. |
| Armazenamento | Bloco virtual → leitura → escrita transacional → hardware listado. | Checksums, reset/interrupção de escrita, integridade pós reboot. |
| Rede | NIC virtual → Ethernet/IP → sockets → modelos selecionados. | Throughput/latência, packet loss, fuzz e recuperação de link. |
| Áudio | Codec/controlador específico após scheduler/timer. | Teste de stream, underrun, latência e dispositivo identificado. |
| GPU | Primeiro framebuffer/console, depois aceleração selecionada. | Resolução/formatos, bounds, reset e carga gráfica. |
| Energia | Política inicial apenas em plataforma validada. | Suspend/resume, estados ACPI e temperatura/energia verificadas. |
| API e apps | API própria versionada; compatibilidade existente só por camada explicitamente licenciada/testada. | Suite de ABI por arquitetura/versão e apps nomeadas. |
| GUI | Framebuffer/software rendering antes da aceleração. | Input/output, escala, acessibilidade, carga e recuperação. |

A ordem é proposta e depende de fases futuras; não é compromisso de datas nem
de suporte amplo.
