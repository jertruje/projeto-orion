# Modelo de ameaças inicial

**Estado:** baseline preliminar; deve ser refinada antes de processos em modo
usuário, IPC e dispositivos reais.

## Ativos e limites de confiança

Ativos: integridade do kernel e do boot chain; confidencialidade/integridade
de memória; integridade dos dados persistentes; disponibilidade; autoridade
de dispositivo; atualização correta; diagnósticos úteis.

Limites inicialmente considerados: firmware→loader→kernel; kernel→futuro
processo; kernel→serviço; serviço→driver; CPU→DMA/dispositivo. No protótipo
atual, quase todo código ORION executado após o boot está no kernel ou loader;
não há isolamento entre processos.

## Ameaças e tratamento esperado

| ID | Ameaça | Escopo inicial | Mitigação futura/necessária | Estado atual |
|---|---|---|---|---|
| TH-01 | Aplicação/processo malicioso ou comprometido | Dentro do escopo do objetivo | Separação user/kernel, page tables, capacidades, limites de recursos, testes negativos. | Sem processos/isolamento. |
| TH-02 | Leitura/escrita de memória não autorizada | Dentro | Page tables verificadas, validação de ponteiros/handles, invariantes e fuzzing. | Há gerenciamento de memória de bootstrap, mas não address spaces de usuário, validação de ponteiros de processo ou isolamento. |
| TH-03 | Escalada de privilégios | Dentro | Mínima autoridade, validação de syscall/IPC, transições de privilégio e testes de abuso. | Não aplicável a user mode ainda. |
| TH-04 | Driver defeituoso/comprometido | Dentro, progressivo | Driver isolado quando viável; DMA restrito por IOMMU quando suportado; watchdog e recuperação segura. | Sem drivers. |
| TH-05 | Mensagem IPC malformada | Dentro | Parsing por comprimento/versão, quotas, fuzzing, cancelamento/timeout bem definidos. | Sem IPC. |
| TH-06 | Exaustão CPU/memória/handles/filas | Dentro | Contabilidade, quotas, limites, falha controlada e backpressure. | Sem accounting. |
| TH-07 | Atualização interrompida/corrompida | Parcial | A/B ou transação, verificação de integridade, fallback conhecido e teste de power-cut. | Sem armazenamento/atualizador. |
| TH-08 | Adulteração de boot component | Parcial | Secure Boot/assinaturas e cadeia de confiança, proteção de chaves, medição e política de rollback. Firmware pode ser raiz comprometida. | Loader não autentica kernel criptograficamente. |
| TH-09 | Falha de serviço privilegiado | Dentro | Limitar authority, supervisionar, reiniciar quando estado idempotente/consistente e validar dependentes. | Sem serviços de usuário/supervisor. |
| TH-10 | Bug de kernel, corrupção ou deadlock | Parcial | Fuzzing, assertions, isolamento de módulos, crash record e reboot controlado. Bug em ring 0 geralmente exige reinício. | Kernel mínimo; sem panic/handlers próprios. |
| TH-11 | Firmware, hardware ou dispositivo comprometido | Parcial/fora da garantia inicial | Secure Boot ajuda apenas se raízes/chaves forem confiáveis; IOMMU/atestado dependem de plataforma. | QEMU emulado apenas. |
| TH-12 | Vulnerabilidade de CPU ou canal lateral | Parcial/fora da garantia | Mitigações específicas do fabricante, isolamento, conhecimento de modelo e testes; residuais podem persistir. | Não mitigado/medido. |
| TH-13 | Erro de especificação, compilador, assembler ou ferramenta | Parcial | Revisão independente, versões fixadas, build reproducível, análise e testes cruzados. | Build configurado, sem prova bit a bit/cross-toolchain. |
| TH-14 | Ataque físico, falha elétrica ou sabotagem de laboratório | Fora do escopo inicial | Procedimentos físicos e backup externo são necessários. | Não endereçado. |

## Adversários e pressupostos

Supõe-se que código ORION, documentação, toolchain e imagem possam conter bugs.
Não se assume firmware, CPU, memória ou dispositivos confiáveis em hardware
real. QEMU reduz variabilidade e facilita automação, mas não representa
adversários físicos nem todos os dispositivos. Segurança de boot só pode ser
afirmada relativamente a uma raiz de confiança e uma política de chaves
especificadas.

## Propriedades que exigem evidência

- Isolamento: tentativa inválida falha sem alterar memória/estado de terceiro.
- Menor privilégio: componente não obtém direito além do explicitamente
  concedido.
- Contenção: crash injetado fica dentro do componente anunciado como isolado.
- Recuperação: serviço volta a responder e invariantes/dados são validados.
- Integridade de boot: imagem rejeitada quando assinatura/hash esperado não
  corresponde, com raiz e política documentadas.

Testes de sucesso nominal não bastam. Cada propriedade requer teste negativo,
registro de versão/configuração e repetição.
