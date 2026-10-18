# Fase 11: threat model

## Estado e fronteira de confiança

**Implementado e testado:** um gerenciador bounded de capabilities executado
somente no kernel, integrado ao processo kernel. **Não implementado:** user
mode, syscalls, processos independentes ou uma fronteira entre código hostil e
kernel. Os testes atuais demonstram decisões de autorização nas APIs internas;
não demonstram isolamento de um atacante em Ring 3.

O kernel, o compilador/linker, o loader, o firmware, a CPU e a memória física
continuam na base de confiança. As tabelas de capacidades e seus metadados são
memória gravável pelo kernel. Comprometimento de CPL0 permite contornar o
gerenciador.

## Ameaças cobertas nesta fatia

| Ameaça | Mitigação implementada | Limite/evidência |
|---|---|---|
| Handle desconhecido, alterado ou de outro processo | Busca na tabela do proprietário, validação de índice, geração e objeto | Testes host negativos; handles não são ponteiros |
| Uso após close/reuso de slot | Geração por slot; slots são aposentados antes de wrap | Testes de handle obsoleto e reutilização |
| Reuso do mesmo objeto/handle após nova tabela de processo | Geração da tabela compõe o handle | Teste de registro/liberação e handle anterior |
| Direitos insuficientes ou elevação por derivação/transferência | Direitos verificados centralmente; direitos derivados/transferidos devem ser subset | Testes host; API de criação inicial é kernel-only |
| Confusão de tipos | Verificação do tipo esperado em `acquire` | Testes host e autoteste QEMU |
| Uso de objeto globalmente revogado | Revogação remove referências de todos os slots | Teste host e autoteste QEMU |
| Corrida local entre lookup, alteração e revogação | APIs serializam os metadados desabilitando interrupções no modelo single-core | Não protege contra SMP; nenhum teste de corrida multi-CPU |
| Destruição enquanto uma operação adquiriu referência | Lease mantém o objeto vivo até release; destrutor adiado | Testes host e autoteste QEMU |
| Esgotamento de tabela/recursos | Limites estáticos e estados de erro explícitos | Testes host; sem alocação dinâmica neste módulo |

## Fora do escopo ou apenas parcialmente mitigado

- Escrita/leitura de memória entre processos e páginas do kernel: sem CPL3 ou
  roots independentes por processo; não mitigado pelo módulo.
- IPC malformado, serviço privilegiado comprometido e abuso de syscalls:
  subsistemas ainda não existem.
- Exaustão global do kernel: limites locais existem, mas não há quotas por
  usuário, orçamento de CPU ou política de fairness.
- Drivers, MMIO, DMA e IOMMU: sem mediação de dispositivos implementada.
- Adulteração antes da entrada do kernel: sem cadeia de boot autenticada.
- Firmware, hardware, vulnerabilidades de CPU, canais laterais, erros de
  especificação, compilador comprometido e comprometimento completo do kernel:
  fora da garantia desta fase.

## Regras de decisão

Falhas de validação negam a operação; nunca concedem direitos por fallback.
Corrupção de invariantes retorna estado explícito ao chamador, mas a resposta
segura de produção (por exemplo, panic ou isolamento do subsistema) ainda
depende de integração futura. Segurança observada é limitada aos testes
enumerados em [security-testing.md](security-testing.md).
