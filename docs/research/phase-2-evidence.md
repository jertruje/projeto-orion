# Fase 2 — registro inicial de fontes e limites da pesquisa

**Estado:** consulta documental da Fase 2 concluída com limites explícitos.
Estas notas registram fontes primárias e indisponibilidades; não fecham a
arquitetura nem substituem experimentos de implementação. A revisão inicial
foi acessada em **2026-10-15** e a complementação foi acessada em
**2026-10-16**.

| Fonte | Evidência consultada | Aplicação permitida | Limite |
|---|---|---|---|
| UEFI Forum, specifications — https://uefi.org/specifications | Tentativa automatizada desta sessão respondeu HTTP 403; a página oficial é o ponto de entrada para versões publicadas. | Consultar a especificação oficial, selecionar versão exata e verificar serviços/protocolos usados pelo loader. | Texto normativo não foi obtido nesta consulta; não atribuir cláusula/número sem abrir o PDF oficial manualmente. |
| Intel, Intel® 64 and IA-32 Architectures Software Developer Manuals — https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html | Página oficial de manuais retornou HTML, confirmando a fonte de referência dos SDMs. | Referência normativa para ABI/CPU, paging, exceções, interrupções e APIC, condicionada ao volume/versão correta. | Nesta consulta não foi analisado o texto integral dos volumes; verificar especificação AMD para diferenças. |
| QEMU System Emulation — https://www.qemu.org/docs/master/system/introduction.html | Documentação consultada declara suporte a emulação de CPU/máquina, TCG, aceleradores e dispositivos; opções dependem do target/machine. | Interpretar teste de boot como teste de ambiente emulado; fixar `machine`, accelerator e devices. | QEMU não é prova de compatibilidade com hardware físico; a página é “master” e pode mudar. |
| seL4 documentation — https://docs.sel4.systems/ | Página oficial consultada apresenta documentação mantida para tecnologia seL4, frameworks, linguagens e componentes. | Estudar conceitos e a documentação própria do sistema, sem copiar implementação. | Landing page não é estudo de propriedades, provas ou desempenho; fontes/papers específicos permanecem por analisar. |
| Fuchsia Zircon kernel concepts — https://fuchsia.dev/fuchsia-src/concepts/kernel | Página consultada descreve Zircon como kernel mais conjunto pequeno de serviços/driver/bibliotecas e lista processos, threads, VM, IPC e locking entre serviços do kernel. | Caso de estudo de interfaces entre kernel e serviços. | Não implica que ORION compartilhe esse desenho nem atesta comparação de desempenho/segurança. |
| NIST SP 800-218, Secure Software Development Framework — https://csrc.nist.gov/pubs/sp/800/218/final | Página NIST consultada registra publicação em fevereiro de 2022 e escopo de práticas de desenvolvimento seguro. | Usar como referência para processo de desenvolvimento, revisão e gestão de vulnerabilidade. | Não é especificação de arquitetura de kernel nem certificação de produto. |

## Fontes oficiais consultadas para a proposta da Fase 3

Estas consultas dão exemplos concretos de mecanismos e componentes, mas não
completam a pesquisa comparativa requerida pela Fase 2, nem provam que ORION
deva reproduzi-los.

| Organização/documento | Informação efetivamente observada | Uso e limite |
|---|---|---|
| Linux kernel documentation, [Kernel Self-Protection](https://docs.kernel.org/security/self-protection.html) | Descreve redução de superfície de ataque, permissões estritas de memória, separação kernel/user e metas de proteções testadas/ativas por padrão. | Requisitos de proteção úteis para comparar propriedades; não prova segurança global do Linux nem valida um desenho ORION. Página de documentação viva, acessada 2026-10-15. |
| Linux kernel documentation, [Linux Security Module Usage](https://docs.kernel.org/admin-guide/LSM/index.html) | Documenta hooks de segurança configuráveis em build/boot e distingue módulos MAC como SELinux, Smack, TOMOYO e AppArmor. | Exemplo de política/controle extensível dentro de um kernel monolítico; não é comparação de isolamento de processos de kernel. Acessada 2026-10-15. |
| Linux kernel documentation, [Device Model Overview](https://docs.kernel.org/driver-api/driver-model/overview.html) | Descreve modelo de dispositivo/bus hierárquico e callbacks comuns para probing, shutdown e power management; a própria página indica origem histórica de 2002, atualização em 2006. | Referência para lifecycle/inventário de dispositivos; página antiga e não suficiente para descrever todo driver API moderno. Acessada 2026-10-15. |
| Linux kernel documentation, [EEVDF Scheduler](https://docs.kernel.org/scheduler/sched-eevdf.html) e [CFS Scheduler](https://docs.kernel.org/scheduler/sched-design-CFS.html) | A página EEVDF descreve elegibilidade por lag e escolha por virtual deadline; registra transição do Linux a EEVDF a partir da versão 6.6. A página CFS identifica a mudança em andamento. | Evita tratar a descrição histórica de CFS como estado atual; não transfere parâmetros ou resultados de Linux ao ORION. Acessadas 2026-10-15. |
| Linux kernel documentation, [Kernel Testing Guide](https://docs.kernel.org/dev-tools/testing-overview.html) | Distingue KUnit (white-box/in-kernel) de kselftest (interfaces observáveis/end-to-end) e relaciona cobertura/fuzzing. | Informa que testes de unidade e de sistema provam camadas diferentes; não requer adoção de implementação Linux. Acessada 2026-10-15. |
| Linux kernel documentation, [KFENCE](https://docs.kernel.org/dev-tools/kfence.html) | Descreve detecção amostrada de erros de heap e compromisso entre precisão, overhead e frequência de amostragem. | Exemplo de compromisso de instrumentação; alegações de overhead do projeto não foram reproduzidas nem aplicadas ao ORION. Acessada 2026-10-15. |
| seL4 Foundation, [What is seL4?](https://sel4.systems/About/) | Página do projeto caracteriza seL4 como microkernel e declara verificação formal abrangente. | É declaração institucional; não foi lido nesta sessão o escopo formal completo, hipóteses da prova, hardware coberto ou correspondência de execução. Não usar como prova sem papers/artefatos específicos. Acessada 2026-10-15. |
| seL4 documentation, [Threads tutorial](https://docs.sel4.systems/Tutorials/threads) | Tutorial descreve TCB, prioridade/MCP e escalonamento por prioridade com round-robin entre threads elegíveis da mesma prioridade. | Fonte de conceitos e API do seL4; tutorial não é medição comparativa ou prova de segurança. Acessado 2026-10-15. |
| Fuchsia documentation, [Driver Framework](https://fuchsia.dev/fuchsia-src/concepts/drivers/driver_framework) | Descreve driver manager, index e driver hosts como componentes/processos; drivers co-localizados num host compartilham o mesmo address space. A página diz que a ABI do framework ainda não atingiu estabilidade. | Evidencia que isolamento de driver é por host, não necessariamente por driver, e há custo/compatibilidade de framework. Não implica que ORION consiga oferecer igual isolamento. Acessada 2026-10-15. |
| Fuchsia documentation, [Zircon kernel concepts](https://fuchsia.dev/fuchsia-src/concepts/kernel) | Descreve o kernel como núcleo acompanhado de serviços/drivers/libs para boot, dispositivos e processos; lista syscalls de processos, threads, VM, IPC e locking. | Caso de decomposição de sistema, não prova que esses limites são ideais para ORION. Acessada 2026-10-15. |
| QEMU documentation, [System Emulation Introduction](https://www.qemu.org/docs/master/system/introduction.html) | Explica que QEMU emula CPU/máquina, suporta TCG e aceleradores, e que opções dependem do target/machine. | Sustenta separar resultados de emulação e hardware; URL `master` muda e deve ser fixada por versão em experimentos. Acessada 2026-10-15. |

## Fontes tentadas sem conteúdo utilizável

- UEFI Forum specifications: a página oficial retornou HTTP 403 pela consulta
  automatizada. Não foram citadas cláusulas nem inferidas regras normativas.
- Link direto de paper seL4 e alguns links de Genode/MINIX responderam 404 ou
  retornaram índice sem conteúdo técnico. O paper acadêmico precisa ser obtido
  por URL/cópia oficial validada antes de descrever o escopo da prova.
- `https://fuchsia.dev/fuchsia-src/concepts/components/v2` só expôs metadados
  de licença/última atualização nesta consulta; não lhe foi atribuída
  conclusão técnica.

## Síntese relevante para a Fase 3

1. A documentação Linux mostra que autoproteção, LSM, modelo de dispositivos,
   scheduler e testes são mecanismos distintos; “monolítico” não significa
   ausência de modularidade ou isolamento de usuário.
2. Os materiais seL4 consultados mostram que capabilities e políticas de
   prioridade são parte de contratos de objetos/scheduler; o claim de
   verificação formal requer ler a prova e seu escopo, ainda pendente.
3. Fuchsia documenta driver hosts como processos, mas permite co-localização:
   o domínio real de falha depende da configuração do host.
4. QEMU é uma plataforma de emulação, então dados de boot/desempenho não
   equivalem a resultados em hardware físico.
5. Nenhuma dessas fontes decide a arquitetura ORION isoladamente. ADR-0009
   seleciona uma arquitetura-base a partir dos requisitos e trade-offs
   documentados; ela pode ser revista se implementação e medições contradisserem
   suas premissas. Isso não constitui alegação de superioridade.

## Análise ainda pendente

1. Obter e citar versões exatas de UEFI, Intel SDM e AMD64 Architecture
   Programmer’s Manual; mapear cláusulas realmente usadas pelo loader/kernel.
2. Estudar papers primários sobre microkernel IPC, isolamento de driver,
   capability systems e prova formal do seL4; registrar condições
   experimentais, hardware, trusted computing base e limites da prova.
3. Estudar documentação atual de Linux e de pelo menos dois OS de arquiteturas
   distintas sem tratar seus resultados como transferíveis ao ORION.
4. Comparar evidência formal (escopo da prova, assumptions, trusted code) com
   evidência empírica; não igualar uma prova de kernel a prova do sistema
   completo.
5. Expandir a pesquisa de especificações e estudos primários e usar os
   resultados para revisão da ADR-0009 caso alterem materialmente as premissas.

Não foi usado código de outro kernel no ORION. Esta pesquisa não conclui que
qualquer arquitetura seja superior, e as lacunas registradas não devem ser
interpretadas como resultados de implementação.

## Consulta e revisão das fontes em 2026-10-16

As URLs abaixo foram abertas nesta revisão (não se inferiu conteúdo de páginas
que não retornaram texto técnico). A identificação EVD correspondente e a
limitação de cada afirmação estão em `evidence-register.md`.

| Projeto/tema | Fonte efetivamente consultada | Resultado desta consulta |
|---|---|---|
| Linux autoproteção | https://docs.kernel.org/security/self-protection.html | Texto atual obtido. Confirma modelo de ameaça, metas de self-protection, strict memory permissions, restrição de entradas e módulos; é documentação de mecanismos, não estatística de segurança. |
| Linux LSM | https://docs.kernel.org/admin-guide/LSM/index.html | Texto obtido. Confirma hooks/políticas e configuração de LSM; não prova sistema seguro. |
| Linux drivers | https://docs.kernel.org/driver-api/driver-model/overview.html | Texto obtido e data explícita do conteúdo: rascunho 2002, atualizado 2006. Retido somente para o fato histórico do modelo unificado; insuficiente para descrever todo driver API contemporâneo. |
| Linux scheduler | https://docs.kernel.org/scheduler/sched-eevdf.html | Texto atual obtido. Registra EEVDF, transição a partir do Linux 6.6 e explicação qualitativa do algoritmo; sem resultado de benchmark. |
| Linux memória virtual | https://docs.kernel.org/admin-guide/mm/concepts.html | Texto conceitual obtido. Descreve page tables, VM, TLB, páginas grandes e variação por arquitetura; não fornece medição de desempenho. |
| Linux UIO | https://docs.kernel.org/driver-api/uio-howto.html | Texto obtido. Mostra mapping/IRQ para userspace e casos em que handler mínimo continua no módulo de kernel; não representa driver universalmente isolado. |
| Linux testing/fuzzing | https://docs.kernel.org/dev-tools/testing-overview.html | Texto obtido. Distingue KUnit/kselftest, coverage/KCOV e análise estática/dinâmica, inclusive falsos positivos. |
| seL4 | https://docs.sel4.systems/Tutorials/ipc e https://www.sigops.org/s/conferences/sosp/2009/papers/klein-sosp09.pdf | Tutorial IPC obtido; PDF oficial do paper 2009 respondeu como PDF e permitiu ver índice/títulos, mas não foi transformado em citação de resultados quantitativos nesta consulta. Alcance formal exige delimitar o paper e as hipóteses. |
| seL4 formal verification | https://docs.sel4.systems/projects/sel4/verified-configurations.html ; https://sel4.systems/About/FAQ.html#verification ; https://doi.org/10.1145/2560537 | Escopo por configuração, hipótese de DMA/MMU e paper TOCS 2014 verificados nesta revisão. Cobertura da prova é configuracional e não prova sistema completo; detalhes estão em EVD-024. |
| seL4 IPC performance | https://github.com/seL4/sel4bench-results/blob/883899eeca5bd1d4a9bec25969ea44281d5d4556/2026/pc99-haswell3/PC99_64_haswell3.jsonl | Série fixada por commit apresenta execução de 2026-01-27: cross-address-space, zero-word `seL4_Call`, mean 782 cycles, SD 2.9, n=16. SKU/clock exatos e comparação cross-OS não estabelecidos (EVD-025). |
| Microkernel IPC papers | https://doi.org/10.1145/168619.168633 ; https://doi.org/10.1145/224057.224075 | Metadados de papers de Liedtke (1993/1995) identificados; ACM respondeu HTTP 403 para texto integral. Nenhum valor de benchmark foi repetido sem examinar as condições (EVD-027). |
| Fuchsia kernel/drivers | https://fuchsia.dev/fuchsia-src/concepts/kernel e https://fuchsia.dev/fuchsia-src/concepts/drivers/driver_framework | Texto obtido. Confirma separação kernel/serviços e driver-host process boundary, compartilhamento de address space ao co-localizar drivers e runtime local; a documentação declara ABI de framework ainda não estável. |
| Fuchsia update | https://fuchsia.dev/fuchsia-src/concepts/packages/software_update_system e https://fuchsia.dev/fuchsia-src/concepts/packages/ota | Texto obtido. Descreve TUF 1.0, metadata/hash Merkle e fluxo de update; não foi interpretado como garantia contra todo power loss. |
| Fuchsia components v2 | https://fuchsia.dev/fuchsia-src/concepts/components/v2 | A extração desta ferramenta retornou apenas licença/data de atualização (2025-02-28), sem conteúdo técnico. Conteúdo de components v2 não foi usado para afirmação. |
| Genode | https://genode.org/documentation/architecture/index e https://genode.org/documentation/general-overview/index | Texto oficial obtido. Descreve componentes, recursos, comunicação e TCB específico por aplicação; são princípios arquiteturais, não medição de configuração concreta. |
| MINIX 3 | https://www.minix3.org/ e https://www.minix3.org/doc/ | Site oficial descreve microkernel/processos protegidos e compatibilidade declarada; documentação encaminha para livro publicado em 2006. Não é evidência de manutenção atual, benchmark comparável ou taxa de recuperação. |
| MINIX 3 recovery | https://doi.org/10.1145/1151374.1151391 ; https://github.com/Stichting-MINIX-Research-Foundation/minix/blob/4db99f4012570a577414fe2a43697b2f239b699e/minix/servers/rs/manager.c | Artigo de Herder et al. (2006) e source versionado registram objetivo self-repairing e implementação Reincarnation Server com heartbeat/restart/backoff. Mecanismo não equivale a taxa de recovery medida (EVD-026). |
| SerenityOS | https://raw.githubusercontent.com/SerenityOS/serenity/master/README.md | README oficial obtido; branch `master` é mutável e autodescrição não é auditoria de maturidade ou cobertura de hardware. |
| Fuzzing Linux | https://github.com/google/syzkaller/blob/master/docs/linux/setup.md | Guia oficial do projeto syzkaller obtido; descreve cobertura KCOV, kernel instrumentado, VM/dispositivo e requisitos do ambiente. Não fornece contagem comparável de defeitos. |
| Índices seL4/Genode | https://sel4.systems/Info/Docs/ e https://genode.org/documentation/index | Índice seL4 redireciona para Learn; índice Genode lista fundamentos, arquitetura, componentes, plataformas e releases. O redirecionamento não foi tratado como prova. |

As URLs `https://fuchsia.dev/fuchsia-src/concepts/drivers` e
`https://fuchsia.dev/fuchsia-src/concepts/components/v2` não foram usadas para
conclusões de detalhe: a primeira rota ampla não correspondeu ao documento de
driver utilizado; a segunda retornou somente metadados nesta extração. O
endereço de driver framework efetivamente usado está acima. Uma solicitação
anterior a `https://fuchsia.dev/fuchsia-src/concepts/packages/update` e um
artigo antigo de Genode retornaram 404; foram substituídos por caminhos atuais
que retornaram documentação pertinente. Nenhum código de projeto externo foi
copiado ou incorporado.
