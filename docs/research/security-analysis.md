# Análise de segurança da Fase 2

**Estado:** análise comparativa e de risco, não prova de segurança do ORION.

Fontes primárias desta revisão: EVD-001–005, EVD-009, EVD-014–017,
EVD-022–023 em `evidence-register.md`. A evidência é qualitativa salvo quando
um resultado publicado específico e suas condições são explicitamente
apresentados; esta nota não atribui uma pontuação de segurança aos sistemas.

## 1. Conceitos que não são equivalentes

### Kernel pequeno
- Significa reduzir o tamanho e o escopo do código privilegiado.
- Não garante que o sistema completo seja seguro.
- Exige que a lógica de drivers, políticas e serviços também seja isolada.

### Menos código privilegiado
- Reduz a TCB de código em ring 0.
- Ainda precisa de controles em dispositivos, DMA, memory map, syscall parsing e IPC.

### Isolamento de processos
- Protege aplicações e serviços quando os domínios de execução têm memória e objetos isolados.
- Não garante segurança se uma capability for concedida demais ou se o sistema tiver uma política de comunicação permissiva.

### Kernel baseado em capacidades
- Capabilities podem representar autoridade explícita e granular.
- Não são segurança por si só; dependem de implementação correta, revogação, limites de memória e proteção de objetos.

### Verificação formal
- Pode provar propriedades de um modelo específico, de um kernel ou de um conjunto de componentes formalmente especificados.
- Não prova segurança do firmware, do hardware, do software de usuáriospace, da política de boot nem das integrações.

### Sistema operacional inteiro seguro
- Requer: hardware confiável ou assumido confiável, driver correctness, boot integrity, package update policy, service isolation, memory isolation, user policies e gestão de atualização.
- Isso é um objetivo de sistema, não uma garantia por uma única arquitetura.

## 2. Ameaças consideradas relevantes

| Ameaça | Escopo inicial | Mitigação relevante | Observação |
|---|---|---|---|
| Processo ou serviço malicioso | Dentro do escopo | isolamento, policies, quotas, capabilities | Requer user mode e memória separada |
| Acesso não autorizado à memória | Dentro | MMU, page tables, page fault, capability binding | Não pode ser afirmado sem implementação |
| Escalada de privilégios | Dentro | capability checks, syscall validation, boundary controls | Principal objetivo da arquitetura |
| Driver defeituoso | Dentro | isolação por host/process, IOMMU, crash containment | driver bug em kernel monolítico é problema clássico |
| IPC malformado | Dentro | validation, bounds, quotas, versioning | interface complexa e fonte de bugs |
| Exaustão de recursos | Dentro | quotas, backpressure, deadlines | requer accounting e políticas |
| Atualização interrompida | Parcial | transactional update, verify image before switching | depende do storage e policy |
| Adulteração de boot | Parcial | chain of trust, measured boot, signatures | firmware e root of trust precisam ser definidos |
| Falha de serviço privilegiado | Dentro | service isolation, crash recovery, restart policy | requer reload consistente |
| Hardware comprometido | Parcial/fora do escopo inicial | attestation, IOMMU, secure boot, assumptions | não é resolvido apenas por software |
| Bugs de CPU, canais laterais | Fora do escopo inicial | mitigations, hardware support, formal limits | não há garantia de segurança absoluta |

## 3. Diferenciação entre risco e garantia

### Windows de ataque dentro do escopo
- Ameaças que o designer pode controlar com arquitetura, interfaces e políticas.
- Ex.: syscall validation, IPC malformed input, driver isolation, quotas, page table separation.

### Parcialmente mitigáveis
- Ameaças condicionadas por firmware, hardware, CPU ou configuração.
- Ex.: IOMMU, DMA, secure boot, side channels, microcode, modelo de plataforma.

### Fora do escopo inicial
- Ataques físicos, hardware comprometido, problemas de cadeia de fornecedor, compromissos de laboratório, etc.

## 4. O que a documentação oficial sugere

### Linux
- A documentação de self-protection e LSM mostra que o Linux tem um conjunto importante de defensivas, mas isso é um conjunto de mecanismos dentro de um sistema monolítico.
- Isso reduz certos riscos de execução, mas não elimina a superfície de código privilegiado.

### seL4
- A proposta de capability-based kernel e comprovada verificação formal é forte para a propriedade “kernel correto em um modelo especificado”.
- Isso não garante tudo do sistema e não substitui drivers, serviços e políticas.

### Fuchsia / Genode
- A arquitetura de driver host e de TCB específico por aplicação demonstra o uso deliberado de várias fronteiras de privilégio.
- Isso potencialmente reduz blast radius, mas exige gestão correta de IPC, direito, ambiente e resources.

## 5. Propriedades necessárias para provar segurança

A segurança não pode ser declarada apenas por desenho. É preciso evidência para:

1. isolamento de memória e objetos,
2. validação de syscall / IPC,
3. reprodução de falhas com testes negativos,
4. containment of driver or service crash,
5. rollback e integridade do boot,
6. quotas e regressão sob carga,
7. medição de risco do TCB.

### Propriedades formais versus evidência operacional

Klein et al. (SOSP 2009; TOCS 2014) e a página seL4 de configurações
verificadas (EVD-024) documentam resultados formais para propriedades de
kernel e configurações especificadas. A cobertura varia por arquitetura e
configuration; a FAQ registra a hipótese de que a MMU controle a memória
(DMA desabilitado ou restrito), e as tabelas atuais não devem ser lidas como
uma prova uniforme de startup, IOMMU ou cada binário. A prova não inclui
automaticamente firmware, boot chain, compilador, drivers, políticas,
aplicações ou outros componentes do sistema final.

Uma prova pode estabelecer, sob suas premissas, propriedades como preservação
de invariantes do kernel, correção de abstração ou controle de acesso de
objetos. Ela não substitui evidência empírica de comportamento em hardware,
testes de integração, revisão de configuração ou análise de canais laterais.
Para cada alegação formal futura, registrar:

1. teorema/propriedade exata e modelo;
2. versão/revisão e artefato coberto;
3. hipóteses de hardware, compiler e inicial state;
4. trusted computing base e dependências não provadas;
5. relação da prova com o código/binário entregue;
6. componentes e políticas fora do escopo.

O status da prova de um kernel não se propaga para o OS completo sem compor e
verificar também interfaces, componentes e hipóteses da composição.

## 6. Custos de segurança na arquitetura

- IPC: maior custo em validação, cópia, versionamento e quotas.
- Context switch: custo de preservação de state, address space e capability.
- Capabilities: custo em delegação, revogação, memória e checks.
- Drivers isolados: maior overhead em comunicação, DMA mediation e recovery.
- Shared memory: aumenta possibilidade de conflitos e manipulação indevida.
- Update policy: exige autenticidade, rollback, state consistency e serviços em quiescence.

Os custos são hipóteses de projeto, não valores medidos nesta pesquisa. A
documentação seL4 de IPC confirma mensagens síncronas limitadas e caminhos por
registradores/buffer, mas não fornece neste tutorial latência comparável
(EVD-014). A documentação Fuchsia descreve driver hosts isolados, co-location
com address space compartilhado e runtime local para comunicação mais eficiente
(EVD-015); isso evidencia trade-off arquitetural possível, não quantitativo.
Linux UIO mostra que o repasse de MMIO/interrupções a userspace pode continuar
dependendo de handling mínimo no kernel (EVD-022). DMA exige ainda validar
IOMMU e a possibilidade de hardware acessar memória fora do processo.

Para medir: separar overhead de syscall/IPC, cópia por tamanho, capability
lookup/transfer, troca de address space, fila/contenda, medição sem carga e sob
carga; manter funcionalidade e hardware idênticos quando comparando. Não
agregar estas métricas em um único “custo de segurança”.

## 7. Conclusões

O ORION deve tratar segurança como propriedade de projeto e de implementação, não como uma declaração gratuita. A pesquisa sugere que:

- reduzir TCB e separar serviços são passos promissores;
- capabilities ajudam a explicitarem autoridade, mas não representam segurança por si; 
- formal verification é valiosa, mas limitada ao escopo formalizado;
- testes negativos, isolamento de driver, boot integrity e update policy continuam sendo requisitos críticos.

É prudente tratar quaisquer ganhos de segurança do ORION como hipótese. A
comparação não seleciona arquitetura: essa decisão pertence à Fase 3 e deve
usar requisitos, evidências e gates separados.
