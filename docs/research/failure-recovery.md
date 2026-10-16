# Recuperação de falhas e limites de garantia

**Estado:** análise do tipo de falha e cenários aceitáveis; sem implementação ORION.

## Evidência de projetos examinados

- MINIX 3 se apresenta como microkernel pequeno com restante do OS em processos
  protegidos de user mode e como projeto de confiabilidade (EVD-007). Um source
  snapshot do Reincarnation Server documenta heartbeat checks, restart/replica
  handling e backoff após falhas repetidas (EVD-026). Isso demonstra a
  existência do mecanismo, não taxa de sucesso, tempo de recuperação ou
  integridade geral de dados.
- Fuchsia documenta driver hosts como processos com address space isolado,
  possível compartilhamento de address space quando drivers co-localizam, e
  gerenciamento de start/stop pelo driver manager (EVD-015). Isto mostra
  lifecycle e fronteira, mas não comprova restart correto, integridade de
  estado ou retomada de I/O após falha.
- Fuchsia OTA documenta staging e comparação de imagens/metadata em seu fluxo
  de atualização (EVD-016). Isso não garante recuperação após qualquer corte
  de energia sem validar storage, slots e boot policy.

Consequentemente, recovery só pode ser chamado de “demonstrado” quando há
injeção reproduzível, contenção observada, serviço saudável, estado validado e
perda/durabilidade explicitadas.

## 1. Definição de recuperação bem-sucedida

Uma recuperação é bem-sucedida quando:

- o efeito da falha permanece contido,
- o serviço ou subsistema pode continuar ou ser reiniciado com integridade,
- invariantes do sistema continuam válidos,
- o cliente observa uma resposta correta, ou uma falha explícita e controlada,
- a perda de dados é definida e declarada.

## 2. Classes de falhas

| Classe | Exemplo | Recuperação sem reinicialização | Necessita reinicialização | Perda de dados |
|---|---|---|---|---|
| Falha de aplicação | bug em processo usuário | Possível em muitos casos | Se o aplicativo não é isolado ou não tem checkpoint | Possível, depende do aplicativo |
| Falha de serviço | serviço de rede, I/O, logger | Possível se serviço é isolado e reentrante | Frequentemente sim | Possível |
| Falha de driver | MMIO, IRQ, DMA error | Possível em host isolado com restart do driver | Muito provável em défaut crítico | Possível |
| Falha do kernel | bug em ring 0, página inválida, deadlock | Não em geral | Sim, em maioria dos casos | Alta, depende do estado persistente |
| Falha de armazenamento | corrupção de bloco, I/O falha | Possível por redundância e journal | Sim, se a mídia não responde | Possível |
| Falha durante atualização | interrupted upgrade or bad image | Em alguns casos com transaction model | Sim, para rollback ou emergência | Possível |
| Falha de hardware | RAM, CPU, storage, device | Não se o hardware falhou | Sim, em muitos casos | Alta |

“Possível” na tabela significa possível sob mecanismo, arquitetura e
invariantes adequados; não é promessa de comportamento do ORION nem afirmação
de que os sistemas comparados ofereçam isso em todas as configurações.

## 3. O que é tecnicamente plausível

### Recuperação sem reinicialização
- Aplicações isoladas em user mode com gerenciamento de memória e IPC bem definidos.
- Serviço especificado com idempotência, timeout limitado, restart, validação
  de estado e replay apenas de operações seguras.
- Driver host isolado com shutdown/quiescence e lógica de detach antes de
  rebind/restart; processo separado, por si, não retira DMA nem para hardware.

### Recuperação por reinicialização
- Reboot do serviço ou driver em domínio isolado.
- Reinício do sistema inteiro quando o kernel ou a base crítica falhar.
- Rollback de atualização interrompida usando image e metadata validada.

### Perda de dados e garantias
- Dados transientes podem ser perdidos em falhas de serviço.
- Dados persistentes podem ser perdidos em falha de storage, update interrompida ou hardware falhando sem redundância.
- Nunca prometer recuperação total em cenários de falha de kernel, hardware ou storage sem uma estratégia explícita.

Requisições que estavam em voo precisam de semântica explícita: retry pode
duplicar uma operação não idempotente. Clientes precisam obter sucesso,
resultado desconhecido ou erro definitivo; timeout não prova que o dispositivo
ou serviço não executou a operação.

## 4. Limites importantes

- Isolamento por processo não garante recuperação automática sem design de restart e limpeza de state.
- IOMMU e DMA complexos podem criar falhas que exigem reboot do dispositivo ou do sistema.
- Segurança e integridade de boot devem ser avaliadas antes de considerar recovery após update.
- Para update, confirmar atomicidade dos metadados e dados, flush/barriers da
  mídia, boot fallback e anti-rollback; hash de imagem não torna uma
  transação power-fail-safe.

## 5. Conclusão

A arquitetura ORION deve tratar recuperação como um requisito específico por classe de falha e não como um slogan. O desenho mais promissor é aquele que separa:

- aplicação/service recovery,
- driver recovery,
- kernel reboot policy,
- persistent storage failover.

Cada um desses domínios exige critérios objetivos e testes negativos ao longo das fases futuras.
