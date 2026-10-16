# Registro de evidências da Fase 2

**Estado:** APROVADA como pesquisa documental com limites explícitos; 27
entradas rastreáveis. Resultado publicado e fatos documentados permanecem
separados de inferências e hipóteses.

## Categorias adotadas

- Fato documentado: descrição direta em documentação oficial ou publicação técnica.
- Resultado publicado: resultado medido ou afirmado por organização/código aberto, com condições explícitas.
- Inferência: relação lógica a partir de evidências existentes.
- Hipótese: proposta não confirmada por experimentos do ORION.

## Entradas

### EVD-001 — Redução de superfície de ataque no Linux
- Questão investigada: quais mecanismos de autoproteção o Linux utiliza para reduzir a superfície de ataque?
- Afirmação sustentada: a documentação oficial do Linux descreve proteção de memória, migração de código para regiões não graváveis e separação kernel/usuário como parte do modelo de autoproteção.
- Fonte primária: Linux kernel documentation, "Kernel Self-Protection".
- URL: https://docs.kernel.org/security/self-protection.html
- Data de acesso: 2026-10-15
- Tipo de evidência: fato documentado
- Limitações: documento descreve mecanismos, não prova que o sistema completo é seguro ou que a política se aplica igualmente a todas as distribuições.
- Confiança: alta
- Implicação para o ORION: útil para comparar propriedades de hardening, mas não prova que arquitetura monolítica é inferior ou superior.
- Pesquisa adicional necessária: mapear como essas proteções se relacionam com isolamento de driver e kernel/user separation.

### EVD-002 — Linux Security Module (LSM)
- Questão investigada: como o Linux gerencia política de segurança em kernel monolítico?
- Afirmação sustentada: a documentação do LSM mostra que a política de segurança é conectada via hooks e não necessariamente via módulos carregáveis, sendo configurada em build/boot.
- Fonte primária: Linux kernel documentation, "Linux Security Module Usage".
- URL: https://docs.kernel.org/admin-guide/LSM/index.html
- Data de acesso: 2026-10-15
- Tipo de evidência: fato documentado
- Limitações: não é uma prova de que um kernel monolítico seja seguro; mostra extensibilidade e política separada do mecanismo.
- Confiança: alta
- Implicação para o ORION: reforça que segurança e política podem coexistir em modelos monolíticos, mas não reduzem a necessidade de isolamento de falhas.
- Pesquisa adicional necessária: comparar LSM com modelos baseados em capacidades e serviços isolados.

### EVD-003 — KUnit e testes de kernel
- Questão investigada: como o Linux estrutura testes em kernel e usuáriospace?
- Afirmação sustentada: a documentação de testes do Linux distingue KUnit (white-box/in-kernel) e kselftest (usuáriospace) e explica que cada uma valida camadas diferentes.
- Fonte primária: Linux kernel documentation, "Kernel Testing Guide".
- URL: https://docs.kernel.org/dev-tools/testing-overview.html
- Data de acesso: 2026-10-15
- Tipo de evidência: fato documentado
- Limitações: documentação não mede confiabilidade de sistema completo; é uma estratégia de validação.
- Confiança: alta
- Implicação para o ORION: testes de arquitetura e testes de integração devem ser distintos e não substituíveis.
- Pesquisa adicional necessária: definir uma suíte mínima para boot, IPC, memória e recuperação.

### EVD-004 — Modelos de capacidades e IPC do seL4
- Questão investigada: como seL4 representa permissões e recursos?
- Afirmação sustentada: o tutorial oficial do seL4 explica que capacidades são tokens não forjáveis e que o kernel controla acesso a objetos e recursos por meio de CNodes e CSlots.
- Fonte primária: seL4 documentation tutorial "Capabilities".
- URL: https://docs.sel4.systems/Tutorials/capabilities
- Data de acesso: 2026-10-15
- Tipo de evidência: fato documentado
- Limitações: tutorial explica a API; não substitui uma verificação do sistema inteiro nem uma comparação confiável de latência em um workload específico.
- Confiança: alta
- Implicação para o ORION: mecanismos de capability têm potencial para exploração explícita de privilégio, mas exigem projeto de autoridades, delegação, revogação e capacidade de testes negativos.
- Pesquisa adicional necessária: comparar custo de invocação de capability, contexto de threads e gestão de memória com a proposta ORION.

### EVD-005 — Drivers do Fuchsia e isolamento por driver host
- Questão investigada: como o Fuchsia isola drivers?
- Afirmação sustentada: a documentação do driver framework afirma que o driver_manager cria driver hosts, cada host é um processo com endereço isolado e que drivers co-localizados compartilham o mesmo espaço de endereços.
- Fonte primária: Fuchsia documentation, "Driver Framework".
- URL: https://fuchsia.dev/fuchsia-src/concepts/drivers/driver_framework
- Data de acesso: 2026-10-15
- Tipo de evidência: fato documentado
- Limitações: a documentação não demonstra ganho absoluto em segurança ou desempenho em comparação com outras arquiteturas; também observa falta de estabilidade de ABI.
- Confiança: alta
- Implicação para o ORION: isolamento de drivers exige definição de host, namespace e boundary explicitamente; “driver em userspace” não é garantia suficiente de contenção.
- Pesquisa adicional necessária: study of DMA/IOMMU, driver lifecycle, crash containment.

### EVD-006 — TCB específico por aplicação em Genode
- Questão investigada: como o Genode organiza mínima confiança por aplicação?
- Afirmação sustentada: a documentação oficial do Genode descreve a árvore de processos, a parent-child hierarchy e a ideia de trusted computing base específica para cada aplicação, reduzindo dependências para um caso concreto.
- Fonte primária: Genode documentation, "General overview".
- URL: https://genode.org/documentation/general-overview/index
- Data de acesso: 2026-10-15
- Tipo de evidência: fato documentado
- Limitações: é uma descrição arquitetural, não um benchmark em produção ou uma prova de que cada aplicação tem TCB menor em todos os cenários.
- Confiança: alta
- Implicação para o ORION: reforça a relevância de TCB explícito, mas não resolve problema de kernel user-space boundary, policy e serviços.
- Pesquisa adicional necessária: comparar custos de comunicação e a organização de serviços na proposta ORION.

### EVD-007 — MINIX 3 e design microkernel orientado à confiabilidade
- Questão investigada: o MINIX 3 usa arquitetura microkernel com serviços isolados?
- Afirmação sustentada: a página oficial do projeto descreve MINIX 3 como sistema baseado em microkernel e processos protegidos em userspace, com foco em confiabilidade/segurança; o artigo de Herder et al. (2006) documenta o desenho de OS “self-repairing”. Um source snapshot do Reincarnation Server descreve heartbeat checks, monitoramento/restart e backoff para falhas repetidas (EVD-026).
- Fonte primária: MINIX 3 site oficial.
- URL: https://www.minix3.org/
- Data de acesso: 2026-10-15
- Tipo de evidência: fato documentado
- Limitações: a página principal é de apresentação; o material técnico mais detalhado está na documentação do livro, e a wiki de documentação da versão atual retornou “topic does not exist".
- Confiança: alta para descrição do projeto e mecanismo codificado; média para eficácia prática sem estudo controlado.
- Implicação para o ORION: útil como caso de arquitetura microkernel; não é prova de um design melhor nem de um benchmark comparável.
- Pesquisa adicional necessária: buscar medidas controladas de taxa/tempo de recovery, integridade e limites do mecanismo.

### EVD-008 — SerenityOS e objetivo de kernel próprio
- Questão investigada: o SerenityOS é um sistema independente com kernel próprio, e quais suas características?
- Afirmação sustentada: o README oficial se descreve como sistema gráfico Unix-like para computadores de 64 bits e o repositório lista recursos de CI/fuzzing.
- Fonte primária: SerenityOS repository README.
- URL: https://raw.githubusercontent.com/SerenityOS/serenity/master/README.md
- Data de acesso: 2026-10-15
- Tipo de evidência: fato documentado
- Limitações: README descreve objetivos e recursos, não benchmarks comparáveis nem avaliação de segurança formal completa.
- Confiança: alta para autodescrição/recursos listados no README; baixa para maturidade e desempenho global.
- Implicação para o ORION: serve como referência de kernel independente, mas não é base para implementação e não substitui ferramentas próprias de avaliação.
- Pesquisa adicional necessária: mapear arquitetura do kernel e ferramentas de desenvolvimento disponíveis no projeto.

### EVD-009 — seL4 e verificação formal
- Questão investigada: o que a documentação oficial afirma sobre verificação formal de seL4?
- Afirmação sustentada: fontes oficiais e trabalhos publicados documentam provas formais para propriedades especificadas de versões/configurações do kernel seL4. Escopo e premissas são descritos por configuração em EVD-024.
- Fonte primária: seL4 Foundation and SOSP 2009 paper.
- URL: https://sel4.systems/About/ e https://www.sigops.org/s/conferences/sosp/2009/papers/klein-sosp09.pdf
- Data de acesso: 2026-10-15
- Tipo de evidência: resultado publicado + declaração institucional
- Limitações: claims não se estendem automaticamente a drivers, userland, firmware, boot, DMA, política ou OS implantado; configurações e premissas precisam ser nomeadas (EVD-024).
- Confiança: alta para existência da prova; o escopo de uma configuração particular exige leitura da tabela/artefatos de prova.
- Implicação para o ORION: formal verification é uma área promissora, mas deve ser tratada como propriedade do kernel específico, não do sistema operacional completo.
- Pesquisa adicional necessária: revisar artefatos e esclarecer hipótese de DMA/MMU antes de qualquer comparação de garantia.

### EVD-010 — Fuchsia e atualização de software
- Questão investigada: como Fuchsia trata atualização e integridade?
- Afirmação sustentada: a documentação relata uso de TUF e processo de OTA com verificação por hash e metadados, além de pacote update e package server.
- Fonte primária: Fuchsia concepts documentation on package updates and OTA.
- URL: https://fuchsia.dev/fuchsia-src/concepts/packages/software_update_system e https://fuchsia.dev/fuchsia-src/concepts/packages/ota
- Data de acesso: 2026-10-15
- Tipo de evidência: fato documentado
- Limitações: não representa o modelo de atualização do ORION e não prova que um mecanismo de OTA é suficiente para um sistema crítico.
- Confiança: alta
- Implicação para o ORION: atualizações e reversão precisam de políticas explícitas, hashes, rollback e integração com boot chain.
- Pesquisa adicional necessária: mapear requisitos de atualizações atômicas e rollback no ORION.

## Evidências verificadas na retomada de 2026-10-16

### EVD-011 — EEVDF na documentação atual do Linux
- Questão investigada: qual política de fairness/latência a documentação atual descreve?
- Afirmação sustentada: a documentação descreve a transição do scheduler Linux para EEVDF desde Linux 6.6 e explica elegibilidade por lag, escolha por virtual deadline e deferred dequeue de tarefas que dormem.
- Fonte primária: Linux kernel documentation, “EEVDF Scheduler”.
- URL: https://docs.kernel.org/scheduler/sched-eevdf.html
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado; a referência científica de 1995 é resultado publicado relacionado.
- Limitações: documento vivo; não fixa configuração/versão de uma medição nem demonstra fairness absoluta em toda carga.
- Confiança: alta para a descrição atual documentada; média para comportamento de versões/configurações específicas.
- Implicação potencial para ORION: scheduler precisa de política, workload e fairness definidos; não transpor o algoritmo sem requisito e benchmark.
- Pesquisa adicional necessária: benchmark controlado por versão e workload antes de comparar latência/fairness.

### EVD-012 — Modelo de dispositivos Linux e idade da página consultada
- Questão investigada: que lifecycle e abstrações de dispositivos a página oficial documenta?
- Afirmação sustentada: a página descreve um modelo hierárquico comum de bus/dispositivo com descoberta, shutdown e power-management callbacks; a própria página declara rascunho de 2002 e atualização de 2006.
- Fonte primária: Linux kernel documentation, “The Linux Kernel Device Model”.
- URL: https://docs.kernel.org/driver-api/driver-model/overview.html
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado em uma página histórica.
- Limitações: a data explícita torna inadequado tratar esse texto isolado como descrição completa da implementação contemporânea; nenhuma conclusão sobre frequência de bugs de driver pode ser derivada dele.
- Confiança: alta para o conteúdo literal e sua idade; baixa para generalizações sobre o modelo atual.
- Implicação potencial para ORION: exigir revisão de lifecycle e controle de versão das fontes; separar evidência arquitetural de evidência de qualidade.
- Pesquisa adicional necessária: consultar documentação atual por bus, runtime PM, DMA, PCI e driver API antes de especificar interfaces.

### EVD-013 — Linux testing, coverage e análise estática
- Questão investigada: quais camadas de validação o guia oficial distingue?
- Afirmação sustentada: o guia distingue KUnit (white-box, in-kernel) de kselftest (interfaces/end-to-end em userspace), descreve KCOV para cobertura por tarefa/fuzzing e aponta análise dinâmica/estática, explicitando falsos positivos de análise estática.
- Fonte primária: Linux kernel documentation, “Kernel Testing Guide”.
- URL: https://docs.kernel.org/dev-tools/testing-overview.html
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado.
- Limitações: documentação de processo não quantifica a eficácia nem a cobertura real do kernel.
- Confiança: alta para distinções e ferramentas documentadas.
- Implicação potencial para ORION: testes unitários, integração, fuzzing e análise estática verificam superfícies diferentes e precisam de resultados próprios.
- Pesquisa adicional necessária: definir configuração e orçamento de fuzzing adequado a cada parser/IPC ORION.

### EVD-014 — IPC síncrono e mensagem limitada no seL4
- Questão investigada: como o tutorial oficial define IPC básico e custo de transferência?
- Afirmação sustentada: endpoints são objetos de comunicação; `Send` pode bloquear até consumo, `Call` combina request/reply por reply capability; mensagens têm tamanho limitado, dados podem ir em registradores e IPC buffer, e capability transfer é explícita. O tutorial descreve o protocolo e o caminho fastpath, não seu desempenho.
- Fonte primária: seL4 documentation, “IPC” tutorial.
- URL: https://docs.sel4.systems/Tutorials/ipc
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado.
- Limitações: tutorial/API não é prova do custo observado em uma plataforma nem benchmark comparativo.
- Confiança: alta para a interface descrita.
- Implicação potencial para ORION: message bounds, blocking behavior, ownership/reply lifetime e transfer de autoridade precisam fazer parte do contrato de IPC.
- Pesquisa adicional necessária: medir round-trip, payload, contenda e transferência de capability em build/CPU fixados.

### EVD-015 — Isolamento e co-localização no Fuchsia Driver Framework
- Questão investigada: qual é o domínio de isolamento real do driver no framework documentado?
- Afirmação sustentada: driver hosts executam como processos com address spaces próprios; mais de um driver pode co-localizar no mesmo host e então compartilhar address space. Driver runtime oferece comunicação local entre co-localizados para evitar passagens pelo kernel; a documentação declara que ABI ainda não está estável.
- Fonte primária: Fuchsia documentation, “Driver framework”.
- URL: https://fuchsia.dev/fuchsia-src/concepts/drivers/driver_framework
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado.
- Limitações: descrição de mecanismo, não evidência de redução quantitativa de falhas ou overhead.
- Confiança: alta para a arquitetura descrita.
- Implicação potencial para ORION: definir isolamento por driver versus host e medir comunicação local/IPC; “userspace” sozinho não define blast radius.
- Pesquisa adicional necessária: testar fault containment, DMA/IOMMU, restart e custos de driver em configurações equivalentes.

### EVD-016 — Atualização Fuchsia baseada em TUF e processo OTA
- Questão investigada: quais componentes e verificações a documentação de update descreve?
- Afirmação sustentada: o sistema de atualização cita TUF 1.0, metadados assinados/hash Merkle para pacotes, e componentes de checagem/updater; o documento OTA descreve verificação de mudanças de imagem/vbmeta/ZBI e staging.
- Fonte primária: Fuchsia documentation, “Software update system” e “Over-the-Air updates”.
- URLs: https://fuchsia.dev/fuchsia-src/concepts/packages/software_update_system ; https://fuchsia.dev/fuchsia-src/concepts/packages/ota
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado.
- Limitações: descreve Fuchsia e fluxo de produto; não prova que qualquer sistema que use TUF ou hashes sobreviva a cortes de energia nem fornece política diretamente aplicável ao ORION.
- Confiança: alta para o processo descrito.
- Implicação potencial para ORION: update requer autenticação de metadados, integridade de payload, estado de ativação/rollback e monitoramento explícitos.
- Pesquisa adicional necessária: testes de corte de energia, slot fallback, anti-rollback e persistência no desenho de armazenamento escolhido.

### EVD-017 — Componentes e TCB específico por aplicação no Genode
- Questão investigada: como a documentação arquitetural define componentes e TCB?
- Afirmação sustentada: a documentação do framework descreve decomposição em componentes isolados (incluindo drivers, filesystems e stacks de rede), atribuição de recursos e comunicação; define TCB específico por aplicação como conjunto limitado de componentes necessários à aplicação.
- Fonte primária: Genode OS Framework documentation, “Architecture” e “General overview”.
- URLs: https://genode.org/documentation/architecture/index ; https://genode.org/documentation/general-overview/index
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado/inferência arquitetural do próprio projeto.
- Limitações: TCB específico é um princípio de arquitetura; a documentação não demonstra que toda configuração real minimize TCB nem fornece, nestas páginas, overhead comparável.
- Confiança: alta para os princípios documentados; média para resultado efetivo de uma configuração concreta.
- Implicação potencial para ORION: explicitar grafo de componentes e autoridade por aplicação; auditar configurações efetivamente implantadas.
- Pesquisa adicional necessária: observar configuração executável, cobertura de testes e desempenho por plataforma.

### EVD-018 — Escopo de sistema e documentação de SerenityOS
- Questão investigada: que objetivo e capacidades o README oficial afirma?
- Afirmação sustentada: o projeto se descreve como sistema gráfico Unix-like para arquiteturas de 64 bits; o repositório apresenta código, documentação, ferramentas/CI e fuzzing.
- Fonte primária: SerenityOS repository README.
- URL: https://raw.githubusercontent.com/SerenityOS/serenity/master/README.md
- Data de acesso: 2026-10-16
- Tipo de evidência: declaração de escopo e artefatos públicos do projeto.
- Limitações: branch `master` é mutável; README/badges não medem maturidade, compatibilidade ou segurança global. Não foi feita auditoria de código.
- Confiança: alta para a autodescrição e os recursos listados no momento da consulta; baixa para avaliação comparativa de maturidade.
- Implicação potencial para ORION: exemplo de desenvolvimento de sistema independente com userland integrado, não benchmark de um OS de uso geral.
- Pesquisa adicional necessária: fixar commit e examinar releases, matriz de hardware, testes e limitações do ABI antes de comparações mais específicas.

### EVD-019 — Atualização da documentação oficial do Fuchsia components v2
- Questão investigada: a página de concepts/components/v2 oferece evidência técnica recuperável nesta consulta?
- Afirmação sustentada: nesta consulta automatizada, a página expôs apenas licença e data de atualização (2025-02-28), sem corpo técnico utilizável.
- Fonte primária: Fuchsia documentation, “Components v2”.
- URL: https://fuchsia.dev/fuchsia-src/concepts/components/v2
- Data de acesso: 2026-10-16
- Tipo de evidência: disponibilidade observada, não evidência técnica sobre o modelo de componentes.
- Limitações: falha de extração não significa ausência de conteúdo no site.
- Confiança: alta para o resultado da consulta; nula para alegações arquiteturais não retornadas.
- Implicação potencial para ORION: nenhuma conclusão foi atribuída a esta página nesta pesquisa.
- Pesquisa adicional necessária: consultar navegação renderizada/versão arquivada e registrar conteúdo específico antes de usar claims sobre manifests, lifecycle ou realms.

### EVD-020 — Linux virtual memory concepts
- Questão investigada: que abstrações e compromissos de memória virtual Linux a documentação oficial descreve?
- Afirmação sustentada: a documentação descreve páginas, page tables, proteção/compartilhamento de dados por VM e o papel do TLB; destaca que huge pages podem reduzir pressão de TLB e que a gestão varia com arquitetura/configuração.
- Fonte primária: Linux kernel documentation, “Concepts overview” (Memory Management).
- URL: https://docs.kernel.org/admin-guide/mm/concepts.html
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado.
- Limitações: visão conceitual; não mede latência ou fragmentação nem descreve todos os mecanismos de cada arquitetura.
- Confiança: alta para conceitos descritos.
- Implicação potencial para ORION: proteção e compartilhamento de memória devem ser avaliados junto a TLB, tamanho de página e hardware, sem presumir ganhos de huge pages.
- Pesquisa adicional necessária: fixar x86-64/CPU e medir fault/TLB/allocation em workload definido.

### EVD-021 — Fuchsia Zircon kernel scope
- Questão investigada: quais funções a documentação Fuchsia atribui ao kernel Zircon?
- Afirmação sustentada: Zircon é descrito como kernel acompanhado por pequeno conjunto de serviços, drivers e bibliotecas user-space necessários para boot, hardware e processos; syscalls cobrem process, threads, VM, IPC, object waits e futexes. A página declara que parte da syscall surface ainda é temporária durante bring-up.
- Fonte primária: Fuchsia documentation, “Zircon kernel concepts”.
- URL: https://fuchsia.dev/fuchsia-src/concepts/kernel
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado.
- Limitações: índice de conceitos não é benchmark nem descrição exaustiva do OS; declaração de evolução é dependente do estado da documentação.
- Confiança: alta para o texto acessado.
- Implicação potencial para ORION: registrar estágio de estabilidade/compatibilidade da interface, não tratar lista de syscalls como ABI final.
- Pesquisa adicional necessária: consultar documentação de ABI e release específica antes de comparação de compatibilidade.

### EVD-022 — Linux UIO e limite de driver user space
- Questão investigada: pode Linux executar partes de driver em user space?
- Afirmação sustentada: guia UIO permite mapear regiões de memória e expor interrupções a processo de usuário, mas exige que ação crítica por interrupção seja executada no módulo do kernel se não puder esperar pelo userspace; o processo pode terminar a qualquer momento.
- Fonte primária: Linux kernel documentation, “Userspace I/O HOWTO”.
- URL: https://docs.kernel.org/driver-api/uio-howto.html
- Data de acesso: 2026-10-16
- Tipo de evidência: fato documentado.
- Limitações: UIO é framework para casos específicos, não prova que drivers gerais estejam isolados nem elimina trusted kernel module.
- Confiança: alta para as restrições indicadas.
- Implicação potencial para ORION: user-mode driver precisa de contrato explícito de IRQ, DMA, quiescence e fallback kernel-side.
- Pesquisa adicional necessária: classificar drivers candidatos por requisito de interrupt handling e autoridade de hardware.

### EVD-023 — Syzkaller e requisitos de fuzzing kernel
- Questão investigada: que infraestrutura o guia oficial do syzkaller exige para fuzzing Linux?
- Afirmação sustentada: syzkaller é coverage-guided; o guia cita compilador/coverage, kernel com KCOV, VM ou dispositivo e configuração de acesso/SSH/debugfs conforme backend.
- Fonte primária: Google syzkaller, “Linux kernel setup”.
- URL: https://github.com/google/syzkaller/blob/master/docs/linux/setup.md
- Data de acesso: 2026-10-16
- Tipo de evidência: documentação de ferramenta.
- Limitações: instruções mutáveis em branch `master`; existência da ferramenta/configuração não mede bugs evitados nem se transfere sem adaptação ao ORION.
- Confiança: alta para requisitos descritos.
- Implicação potencial para ORION: planejar cobertura de chamadas/IPC em harness controlado, com corpus e triagem reprodutível.
- Pesquisa adicional necessária: selecionar modelo de entrada e coverage instrumentation após interfaces estarem implementadas.

### EVD-024 — Escopo e hipóteses da verificação do seL4
- Questão investigada: que propriedades e configurações estão cobertas pela verificação formal, e quais premissas limitam a garantia?
- Afirmação sustentada: o paper SOSP 2009 e o artigo TOCS 2014 documentam provas para propriedades especificadas do kernel seL4; a página de configurações lista evidência por arquitetura/build e registra lacunas, incluindo startup e endereçamento DMA/IOMMU não cobertos nas configurações atuais. FAQ explicita hipótese de DMA desabilitado ou restringido para que a MMU controle memória.
- Fontes primárias: Klein et al., “seL4: Formal Verification of an OS Kernel”, SOSP 2009; Klein et al., “Comprehensive formal verification of an OS microkernel”, ACM TOCS 32(1), 2014; seL4 verified configurations e FAQ.
- URLs: https://www.sigops.org/s/conferences/sosp/2009/papers/klein-sosp09.pdf ; https://doi.org/10.1145/2560537 ; https://docs.sel4.systems/projects/sel4/verified-configurations.html ; https://sel4.systems/About/FAQ.html#verification
- Datas de publicação: 2009-10-11 (SOSP 2009); 2014-02-26 (TOCS 2014). Data de acesso: 2026-10-16.
- Tipo de evidência: resultado formal publicado e escopo documentado por configuração.
- Limitações: prova é sobre modelo/artefato/configuração específicos e não cobre automaticamente serviços, drivers, firmware, boot chain ou sistema implantado inteiro; página de configuração distingue cobertura arquitetural.
- Confiança: alta para existência e escopo declarado da prova; cada garantia exige confirmar configuração/artefato exatos.
- Implicação potencial para ORION: qualquer future claim precisa nomear teorema, build/configuração, premissas e componentes fora da TCB/prova; DMA requer hipótese explícita.
- Pesquisa adicional necessária: revisar artefatos de prova e mapear hardware/modelo do ORION se uma meta formal for aprovada.

### EVD-025 — Microbenchmark público de IPC do seL4
- Questão investigada: há dado público com condições recuperáveis para IPC?
- Afirmação sustentada: uma linha do resultado PC99 Haswell3 x86-64 de 2026, datada 2026-01-27, reporta `seL4_Call` cross-address-space, zero words, média de 782 ciclos, desvio padrão 2.9 ciclos e n=16. Definições do repositório identificam a métrica como IPC cross-address-space sem payload; o repositório descreve microbenchmarks com hot cache.
- Fonte primária: seL4 Foundation, `sel4bench-results` e `sel4bench`.
- URLs fixados por commit: https://github.com/seL4/sel4bench-results/blob/883899eeca5bd1d4a9bec25969ea44281d5d4556/2026/pc99-haswell3/PC99_64_haswell3.jsonl ; https://github.com/seL4/sel4bench-results/blob/883899eeca5bd1d4a9bec25969ea44281d5d4556/metrics.yml ; https://github.com/seL4/sel4bench
- Data do run: 2026-01-27. Data de acesso: 2026-10-16.
- Tipo de evidência: resultado publicado pelo projeto.
- Limitações: microbenchmark do projeto, não comparação independente nem workload de aplicação; record `PC99_64_haswell3` não identifica SKU/clock exato neste registro; ciclos não são wall-clock e n=16 é a amostra declarada.
- Confiança: alta para o valor/estatística publicados; baixa para inferir performance geral ou comparação com outros kernels.
- Implicação potencial para ORION: especificar payload, process/address-space boundary, cache state, statistic e sample count; não usar 782 cycles como target ORION ou comparação com Linux.
- Pesquisa adicional necessária: validar hardware/SKU e metodologia no artefato completo e repetir workload equivalente em implementações atuais.

### EVD-026 — MINIX 3 Reincarnation Server e recuperação documentada
- Questão investigada: existe mecanismo concreto de monitoramento/restart de processos no MINIX 3?
- Afirmação sustentada: o artigo de 2006 descreve MINIX 3 como self-repairing/reliable design; source snapshot do Reincarnation Server define heartbeat checks, monitoramento periódico e restart/replica manager, com contagem de restart/backoff para falhas repetidas.
- Fontes primárias: Herder et al., “MINIX 3: A Highly Reliable, Self-Repairing Operating System”, ACM SIGOPS OSR 40(3), July 2006; source snapshot MINIX RS.
- URLs: https://doi.org/10.1145/1151374.1151391 ; https://research.vu.nl/en/publications/minix-3-a-highly-reliable-self-repairing-operating-system/ ; https://github.com/Stichting-MINIX-Research-Foundation/minix/blob/4db99f4012570a577414fe2a43697b2f239b699e/minix/servers/rs/main.c ; https://github.com/Stichting-MINIX-Research-Foundation/minix/blob/4db99f4012570a577414fe2a43697b2f239b699e/minix/servers/rs/manager.c ; https://github.com/Stichting-MINIX-Research-Foundation/minix/blob/4db99f4012570a577414fe2a43697b2f239b699e/minix/servers/rs/type.h
- Data de publicação: 2006-07. Data de acesso: 2026-10-16.
- Tipo de evidência: artigo publicado e mecanismo documentado em código versionado.
- Limitações: código/documentação demonstram mecanismo, não taxa de disponibilidade, taxa de restart bem-sucedido ou cobertura completa de falhas; recuperação manager/driver/hardware e persistência não são automaticamente garantidos.
- Confiança: alta para comportamento expresso no source snapshot; média para conexão entre mecanismos do source e resultados práticos sem estudo de execução.
- Implicação potencial para ORION: restart monitorado requer heartbeat/health state, backoff, restart policy, replica strategy e validação de serviço antes de reabrir clientes.
- Pesquisa adicional necessária: obter estudo controlado de MINIX com injeção de falhas, sucesso/tempo de recuperação e integridade de dados.

### EVD-027 — Papers históricos de Liedtke sobre IPC/microkernel
- Questão investigada: que resultados históricos publicados são pertinentes a IPC de microkernel?
- Afirmação sustentada: há artigos primários de Jochen Liedtke sobre melhorar IPC por design de kernel (SOSP 1993) e construção de microkernel (OSR 1995). DOI e metadados foram confirmados; ACM retornou HTTP 403 para acesso ao corpo/PDF nesta consulta, então não foi possível validar aqui valores ou metodologia numérica.
- Fonte primária: Liedtke, “Improving IPC by kernel design”, ACM SOSP 1993; Liedtke, “On micro-kernel construction”, ACM SIGOPS Operating Systems Review 29(5), 1995.
- URLs: https://doi.org/10.1145/168619.168633 ; https://doi.org/10.1145/224057.224075
- Datas: 1993 e 1995. Data de acesso: 2026-10-16.
- Tipo de evidência: referência bibliográfica confirmada; disponibilidade do texto integral indisponível.
- Limitações: nenhum número de benchmark, CPU, versão ou protocolo foi citado sem inspeção do artigo completo.
- Confiança: alta para metadados bibliográficos; nenhuma para métricas não acessadas.
- Implicação potencial para ORION: manter estudos históricos como contexto conceitual, não evidência quantitativa até validar metodologia original.
- Pesquisa adicional necessária: obter acesso autorizado ao texto e extrair platform, build, IPC semantics, repetitions e estatística.

## Conclusão do registro

A pesquisa da Fase 2 encontrou evidência documentada para distinguir:

- monolithic hardening from fault isolation,
- kernel verification from full-system assurance,
- driver isolation by process boundary from superficial usermode execution,
- architectural comparisons from statements of market positioning.

O registro distingue fatos documentados, resultados publicados, inferências e
hipóteses. Nenhuma alegação de superioridade do ORION é sustentada por estas
fontes. Resultados de desempenho, quando não vêm acompanhados de hardware,
versão, configuração, workload e método, não são combinados nem tratados como
comparação. Os documentos de análise citam estes IDs para manter cada
afirmação rastreável.
