# ORION OS — requisitos e critérios iniciais

**Estado:** baseline da Fase 1. Requisitos são objetivos; não constituem
afirmação de que o sistema já os satisfaz.

## 1. Visão, escopo e independência

O ORION OS é um sistema independente com kernel próprio. A primeira plataforma
alvo é x86-64 com inicialização UEFI e execução de desenvolvimento em QEMU.
C e Assembly são opções imediatas; Rust só deve ser adotado para componentes
futuros após avaliação de toolchain, ABI, runtime, código gerado, manutenção e
benefício verificável.

É proibido derivar ou copiar código de implementação de Linux ou de outro
kernel. É permitido estudar especificações, documentação pública, artigos,
testes e conceitos arquiteturais. Referências não são implementação. Toda
alegação de superioridade técnica sobre Linux exige dados reproduzíveis,
comparação equivalente e revisão independente.

### Objetivos de longo prazo

Reduzir impacto de falhas; aplicar autoridade mínima; permitir recuperação
segura; obter desempenho e latência previsíveis; controlar regressões e
consumo; proporcionar interfaces coerentes; ampliar hardware/aplicações
gradualmente. O projeto ainda não tem evidência para alegar que atinge qualquer
vantagem mensurável.

### Exclusões do protótipo atual

Compatibilidade Linux, drivers genéricos, hardware físico, isolamento de
usuário, segurança contra canais laterais, recuperação de falha de kernel e
interface gráfica não fazem parte do boot demonstrado. A cadeia atual limita-se
a loader UEFI próprio, kernel ELF x86-64 mínimo, QEMU/EDK2, saída serial e loop
idle.

## 2. Requisitos rastreáveis

| ID | Requisito verificável | Fase principal | Evidência de aceitação |
|---|---|---|---|
| OR-PLAT-01 | Compilar alvos x86-64 freestanding sem dependência acidental da libc hospedeira. | 4–6 | Build limpo, símbolos/relocações inspecionados e teste de dependências. |
| OR-BOOT-01 | Firmware UEFI QEMU inicia loader próprio e transfere controle ao entry ELF do kernel. | 5 | Teste automatizado com mensagens serial observáveis e artefatos inspecionados. |
| OR-BOOT-02 | Contrato de boot define ABI, modo, interrupções, stack, mapa e fim dos serviços UEFI. | 5, 8 | Contrato documentado e verificações no código/teste. |
| OR-CORE-01 | Inicialização é determinística; estados só avançam após sucesso real. | 6 | Testes de transição, duplicidade e falha. |
| OR-CORE-02 | Diagnóstico freestanding limitado tem níveis, origem, limites e caminho fatal previsível. | 6 | Testes dos buffers e execução QEMU. |
| OR-CPU-01 | GDT/IDT e exceções suportadas respeitam layouts/ABI x86-64 e permanecem controladas. | 7 | Static asserts, handlers e testes de exceção QEMU. |
| OR-MEM-01 | Alocador físico só reutiliza regiões validadas e mantém invariantes de reserva. | 8 | Testes de intervalo, alinhamento, exaustão, liberação inválida e QEMU. |
| OR-MEM-02 | Mapeamentos virtuais verificam canonicalidade, permissões e invalidam TLB quando exigido. | 8 | Testes de tabela/mapeamento em ambiente controlado. |
| OR-THR-01 | Thread kernel tem stack própria, estado consistente e troca conforme ABI documentada. | 9 | Testes determinísticos de criação, execução, yield e término. |
| OR-SCHED-01 | Round-robin cooperativo precede qualquer preempção; estados e filas preservam invariantes. | 9 | Teste de sequência de scheduling e contadores. |
| OR-TIME-01 | Timer e preempção só ativam após IDT, EOI, frame e retorno/interrupção validados. | 10 | Teste QEMU em uma vCPU com múltiplas threads e métricas. |
| OR-SMP-01 | Nenhuma thread pode ser executada simultaneamente por duas CPUs. | 10+ | Testes repetidos com múltiplas vCPUs, sincronização e identificação por CPU. |
| OR-SEC-01 | Privilégios, capacidades e acesso a hardware são concedidos explicitamente. | 11+ | Fase 11 valida capacidades internas; isolamento de usuário e authority de dispositivos ainda não implementados. |
| OR-IPC-01 | Comunicação de kernel ocorre por canais limitados, capability-protected e com falhas de fila/endpoint explícitas. | 12 | Testes host de payload, direitos, quotas e lifecycle; troca entre threads do kernel observada em QEMU. Ring 3 permanece fora do escopo atual. |
| OR-REL-01 | Recuperação só é alegada quando falha é contida, estado íntegro é verificado e serviço volta a responder. | 13+ | Injeção de falhas, taxa e tempo medidos, integridade verificada. |
| OR-COMP-01 | Suporte é declarado por modelo/revisão/configuração de dispositivo testado. | 14+ | Inventário de dispositivos e logs reproduzíveis. |
| OR-BUILD-01 | Builds têm versões/configuração registradas e resultados repetíveis. | 4+ | Builds repetidos, hashes quando aplicável e ambiente descrito. |
| OR-TOOL-01 | Harness independente compila um probe freestanding x86-64 ELF sem símbolos indefinidos, confirma arquitetura/formato/símbolo e compara duas saídas sob o mesmo ambiente. | 4 | `mingw32-make toolchain-test`; inspeção LLVM e SHA-256 idênticos. |

## 3. Modelo arquitetural e segurança

As alternativas foram comparadas na Fase 1 e a arquitetura-base foi selecionada
na Fase 3; decisão, justificativa e critérios de revisão constam em
[architecture-options.md](architecture-options.md) e
[decision-log.md](decision-log.md). A superfície de confiança, ameaças e
exclusões estão em [threat-model.md](threat-model.md). Não se presume que
microkernel, capacidades ou separação de serviços garantam segurança por si.

## 4. Métricas e comparação

Cada métrica tem definição e método em [success-metrics.md](success-metrics.md).
Valores observados do ORION OS são “não medidos” salvo quando a evidência do
teste de boot explicitamente se aplica. O protocolo comparativo está em
[linux-comparison.md](linux-comparison.md); nenhuma superioridade é inferida.

## 5. Recuperação e compatibilidade

As categorias de recuperação, perda possível, suporte progressivo a hardware e
as distinções entre conceitual/implementado/testado são definidas em
[recovery-compatibility.md](recovery-compatibility.md).

## 6. Critérios de sucesso da Fase 1

- Requisitos, escopo e exclusões estão claros e identificados.
- Alternativas são comparadas sem pontuação arbitrária.
- A recomendação da Fase 1 foi explicitamente provisória; a decisão-base
  posterior da Fase 3 está rastreada separadamente e não representa
  implementação ou prova das propriedades desejadas.
- Ameaças, recuperação, métricas, comparação Linux, riscos e questões em aberto
  têm documentos próprios.
- Não são inventados resultados ou metas de benchmark do ORION.
- Requisitos relacionam-se a fases e evidências.
- Validação documental automatizada passa após a criação do validador.

Qualquer falha nesses critérios mantém a fase INCOMPLETA. Os detalhes de
execução da validação ficam em [phase-status.md](phase-status.md).
