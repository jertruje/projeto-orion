# Fronteiras de privilégio x86-64

## Estado implementado

O kernel usa CPL0/Ring 0. GDT, TSS, IDT e tratamento de exceções estão
instalados; o timer preemptivo testado opera com uma vCPU. Threads e o único
processo organizacional atual executam em Ring 0. O modelo de capabilities
protege decisões internas nas APIs testadas, não separa código kernel de
código hostil.

## Ring 3 e transições: não implementados

Não há execução CPL3, seletor de código/dados de usuário, caminho seguro de
`syscall`/`sysret` ou `iretq` para user mode, validação de retorno, nem ABI de
syscalls. A TSS atual não estabelece a pilha por processo para entrada de
usuário. Não há processo com CR3/address space próprio nem cópia/validação de
buffers de usuário. As page tables de bootstrap não demonstram permissão U/S
por processo.

Antes de executar código não confiável em Ring 3, implementar e testar:

- seletores e descritores de usuário, TSS/RSP0 e pilhas de kernel isoladas;
- transição de entrada/saída que preserve contexto e valide CS, RIP, RSP,
  flags e endereços canônicos;
- raízes de page tables independentes, user mappings validados e kernel
  supervisor-only, com W^X/NX conforme suporte habilitado;
- tratamento de exceções originadas em CPL3 e terminação segura do processo;
- validação de argumentos e cópia segura entre espaços;
- ABI e testes negativos QEMU que provem falha ao acessar páginas protegidas.

Não executar salto para Ring 3 até que esses requisitos e o caminho de falha
estejam implementados. A Fase 11 não habilita syscalls nem declara isolamento.
