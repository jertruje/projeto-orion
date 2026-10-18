# Modelo de capabilities

Uma capability é um handle opaco, local ao processo, que seleciona um slot da
tabela e autoriza um conjunto limitado de operações sobre um objeto do kernel.
O valor não contém um endereço e, sozinho, não concede autoridade: o kernel
confere o proprietário, geração da tabela, índice, geração do slot, estado e
direitos antes de usar o objeto.

## Formato e validação

O handle de 64 bits codifica a geração da tabela do processo nos 32 bits altos,
a geração do slot em 24 bits e `índice + 1` nos 8 bits baixos. Índice zero,
índices fora de 1–32, geração zero, tabela/proprietário divergentes, slot
vazio, objeto revogado ou geração do objeto divergente são rejeitados.

Handles são nomes verificáveis, não segredos criptográficos. A segurança vem
da tabela vinculada ao proprietário e da validação integral; não somente da
dificuldade de adivinhar o número. Slots que atingem a geração máxima são
aposentados. A geração da tabela impede revalidar handles quando uma tabela
liberada é atribuída a outro ciclo de vida. A geração do objeto e leases também
são validados.

## Autoridade

- `acquire` exige tipo esperado e todos os direitos solicitados:
  `(granted & required) == required`; direitos são cumulativos.
- Direitos desconhecidos, conjuntos vazios e direitos não permitidos para o
  tipo são rejeitados.
- `derive` exige `DERIVE` e uma máscara não vazia que seja subconjunto dos
  direitos do pai.
- `transfer` exige `TRANSFER`, tabela-alvo ativa e direitos concedidos como
  subconjunto do handle de origem.
- A criação de um objeto é explicitamente privilegiada: nesta versão, somente
  `orion_process_kernel()` pode usar `create_kernel_object`.
- `describe` revela apenas tipo e direitos do handle validado.
- `close` remove somente a referência daquele slot; `revoke` exige `REVOKE` e
  invalida todas as capacidades para o objeto.

Não há criação de capabilities por processo não privilegiado nem por IPC. A
Fase 12 deve reutilizar as regras de transferência e acrescentar validação de
mensagens, endpoints e quotas; não deve aceitar handles fornecidos em payloads
como autoridade.

## Sincronização e escopo

As mutações e validações das estruturas usam salvar/desabilitar/restaurar IF,
adequado ao kernel single-core observado. Não é lock SMP nem proteção contra
outro processador. Operações externas ainda não chamam este módulo como uma
fronteira de segurança; testes estão descritos em
[security-testing.md](security-testing.md).
