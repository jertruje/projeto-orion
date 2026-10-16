# Limitações técnicas e reclamações relacionadas ao Linux

**Revisão:** 2026-10-16. Esta análise não conclui que Linux, como família de
distribuições, seja geralmente instável ou inseguro. Distingue propriedades
arquiteturais documentadas, consequências plausíveis e reclamações sem
reprodução. Não foi feita auditoria de código nem estudo quantitativo de
incidentes nesta fase.

## Classificação das evidências

- **Demonstrável nesta pesquisa:** a fonte oficial documenta o mecanismo,
  configuração ou limite diretamente.
- **Consequência arquitetural/inferência:** consequência condicional a um
  mecanismo documentado, sem alegação de frequência.
- **Não demonstrado:** reclamação ampla sem dados de hardware, versão,
  configuração, logs e reprodução. Não é tratada como defeito geral.

## Problemas e limitações por origem

| Origem e descrição | Fonte e contexto | Frequência/gravidade e causa | Soluções Linux e limite | Hipótese ORION, custo/risco e teste |
|---|---|---|---|---|
| **Kernel — superfície privilegiada e entradas expostas.** O próprio guia de autoproteção considera cenário de atacante local com leitura/escrita arbitrária de memória como modelo de pior caso e enumera redução de pontos de entrada e proteções de memória. | Linux Kernel Self-Protection (EVD-001). É uma declaração de modelo de ameaça e defesa, não estatística de vulnerabilidades. | Frequência: não estabelecida por essa fonte. Gravidade potencial: alta se o atacante obtiver execução/leitura/escrita privilegiada. Causa: código e objetos compartilhando o domínio do kernel, condicional à falha explorável. | Strict kernel/module RWX, SMEP/SMAP ou equivalentes, restrição de syscalls, módulos assinados/desativados e LSM estão documentados. Mitigam classes e superfície, não provam ausência de bugs (EVD-001, EVD-002). | Considerar menos código privilegiado e interfaces explícitas como hipótese. Custo: mais IPC, protocolos e complexidade de authority/recovery. Teste: negative tests e fuzz de chamadas; medir TCB e overhead contra implementação equivalente. |
| **Kernel — política de segurança integrada.** LSM define hooks para mecanismos/políticas selecionáveis em build/boot; o nome “module” não significa que sejam necessariamente módulos carregáveis. | Linux Security Module Usage (EVD-002). | Não é problema por si. A existência de pontos de extensão é demonstrável; conflitos, cobertura ou falhas por configuração não foram quantificados. | Linux capabilities e LSM (SELinux, Smack, TOMOYO, AppArmor) permitem política de controle de acesso. Configuração e ordem/importância das políticas dependem de build/boot; não substituem validação de kernel. | ORION pode manter mecanismo e política separados, mas isso não exige microkernel. Teste: comparar políticas de privilégio mínimo em cenários equivalentes; revisar bypasses e custo de manutenção. |
| **Kernel — regressões e defeitos de implementação.** Linux mantém camadas de teste, coverage, análise dinâmica/estática e fuzzing; o guia adverte que análise estática pode produzir falsos positivos. | Kernel Testing Guide (EVD-003). | Frequência de regressões não quantificada aqui. Gravidade depende do subsistema e do caminho atingido. A documentação prova que existem estratégias de detecção, não a taxa residual de defeitos. | KUnit, kselftest, KCOV, ferramentas dinâmicas e analisadores estáticos cobrem diferentes níveis e classes; nenhuma classe individual é prova de ausência de bugs. | ORION deve iniciar testes unitários, integration, fuzzing e regressões desde cedo. Custo: harnesses/manutenção/tempo de CI. Teste: registrar casos reproduzíveis, coverage, triagem e defeitos encontrados por versão; comparar taxas só com protocolo e exposição equivalentes. |
| **Drivers — falha em um driver e contenção.** A página Linux consultada descreve o modelo hierárquico de dispositivos e callbacks comuns de probe/shutdown/power-management, mas é explicitamente um texto de 2002/2006. | Linux Kernel Device Model (EVD-012); UIO HOWTO (EVD-022). | As fontes não informam frequência nem provam que drivers frequentemente derrubem o sistema. Inferência condicional: código com autoridade de kernel pode afetar estado privilegiado caso uma falha atravesse validações/isolamentos existentes. | Linux tem APIs, permissões, validação, módulos e mitigations, mas a página histórica não permite comparar containment atual. UIO permite mapear recursos e repassar eventos a userspace, mas mantém módulo kernel para ações críticas de interrupção e não é solução universal (EVD-022). | ORION poderia isolar alguns drivers por processo/host, mediando MMIO/IRQ/DMA. Custo: IPC, compatibilidade, recovery e DMA/IOMMU. Teste: injeção de falha por driver, verificar contenção, restart, device quiescence, dados e tempo. |
| **Firmware/hardware — comportamento depende da plataforma.** O projeto de kernel pode restringir software, mas a fonte consultada não fornece censo de falhas de firmware/hardware Linux. | Limite epistemológico desta pesquisa; não há evidência primária quantitativa registrada nesta fase. | Frequência/gravidade: não estabelecidas. Causa precisa de caso reprodutível; falha observada pode estar em firmware, ACPI, dispositivo, driver, kernel ou userspace. | Secure Boot, medidas de boot, microcode, IOMMU e workarounds dependem de hardware, fornecedor, configuração e cadeia de confiança. | ORION deve registrar modelo/revisão/firmware de cada alvo e separar testado de planejado. Custo: matriz de validação e manutenção de quirks. Teste: reprodução em firmware/hardware identificado, logs, versão de imagem e comparação com ambiente QEMU separado. |
| **Distribuição — configurações, integrações e atualizações.** A mesma família de kernel é distribuída com configurações, patches, drivers, firmware, serviços e políticas diferentes. | Distinção entre kernel e integração de distribuição; esta pesquisa não mediu incidente distribuído. | “Linux é instável” não tem frequência ou causa única demonstrada. Gravidade e causa variam conforme o caso. Não atribuir problema de pacote/boot configuration ao kernel sem evidência. | Distribuições usam canais de update, kernels LTS/rolling, rollback e ferramentas próprias, que não são uniformes. Limite: integração e suporte diferem entre produtos. | ORION deve versionar kernel, userland, firmware e configuração de release separadamente. Custo: matriz e política de compatibilidade. Teste: reproduzir em imagem limpa, documentar manifest/versões e comparar o mesmo workload. |
| **Desktop/GPU/áudio — reclamações de experiência.** Erros gráficos ou de áudio podem cruzar kernel driver, firmware, compositor, servidor de display, toolkit e aplicação. | Não foi coletado corpus de tickets/incidentes reproduzidos; portanto alegações de prevalência não são sustentadas. | Frequência: desconhecida. Gravidade depende do usuário/carga, não necessariamente falha do kernel. Causa: só confirmável com stack trace, logs e reprodução. | Drivers, interfaces de display, sandboxes e diagnósticos oferecem mitigação variável por distro/device; não são garantia de compatibilidade universal. | Não priorizar GUI antes de definir suporte de GPU/input. Custo: grande stack e superfície de teste. Teste: matriz por modelo/revisão/driver e casos de render/input, recovery e carga. |
| **Aplicações — compatibilidade/ABI.** Aplicações podem falhar por runtime, biblioteca, ABI, pacote ou pressupostos de distro sem defeito no kernel. | Esta pesquisa não investigou tickets específicos nem compatibilidade de aplicativos. | Não há frequência geral mensurável aqui; classificar caso a caso. | Camadas de compatibilidade, runtimes e pacotes mitigam subconjuntos, com custo de manutenção. | ORION deve definir API e compatibilidade como requisitos separados; medir conjunto de aplicações nomeado, não “compatibilidade Linux” genérica. |
| **Documentação/manutenção.** A documentação oficial é extensa, mas página individual pode ser histórica ou não representar estado de todas as versões/configurações. | Exemplo verificável: Device Model Overview declara 2002/2006; EEVDF documenta mudança de política ao longo de versões (EVD-011, EVD-012). | Não é incidente de segurança. Causa concreta observada: documentos têm datas/escopos distintos. | Fixar branch/version e revisar release notes ajuda, mas exige trabalho de curadoria. | ORION deve versionar especificações e docs junto a código/teste; custo de manutenção contínua. Teste: verificar link, versão, comando e assertion no CI quando automatizável. |

## Reclamações sem suporte suficiente

Expressões como “pesado”, “não funciona no meu computador”, “instável”,
“driver ruim” ou “desktop confuso” não são, isoladamente, evidência de um
defeito geral do Linux. Para classificar uma ocorrência são necessários, no
mínimo: distribuição e versão, kernel/configuração, dispositivo/revisão,
firmware, driver, workload, logs, efeito observado e passos de reprodução.
Sem esses dados, classificar como **relato não verificado**, não como defeito
de kernel.

## Conclusões sustentadas

1. É demonstrável que Linux documenta autoproteção, permissões estritas,
   mecanismos de política e camadas de teste (EVD-001–003).
2. É demonstrável que o próprio projeto explicita objetivos que podem conflitar
   — efetividade, default-on, impacto de performance, depuração e testes — no
   desenho de autoproteção (EVD-001).
3. É demonstrável que uma fonte do modelo de dispositivos consultada é
   histórica; não se deve extrapolá-la para todas as APIs atuais (EVD-012).
4. Esta pesquisa **não** demonstra uma frequência geral de kernel crashes,
   bugs de driver, falhas de desktop nem que um microkernel reduziria o total
   de defeitos. Os custos e benefícios ORION continuam hipóteses testáveis.

Fontes primárias, escopo e limitações: `evidence-register.md` (EVD-001–003,
EVD-011–013, EVD-020, EVD-022). Não foi feita mineração de CVEs, incidentes ou reclamações de
usuários; tais contagens não devem ser inferidas.
