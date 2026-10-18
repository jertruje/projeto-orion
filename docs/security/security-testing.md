# Testes de segurança da Fase 11

## Executados nesta revisão

| Classe | Comando | Resultado observado |
|---|---|---|
| Testes internos host | `mingw32-make test-unit` | PASS; testes de capability executados junto aos testes de tasking |
| Build/ELF | `mingw32-make build inspect` | PASS; kernel ELF x86-64 linkado com `--no-undefined`, inspeção concluída, sem símbolo indefinido |
| Inicialização QEMU e autoteste | `mingw32-make test-boot` | PASS; serial incluiu `Capability checks and revocation passed` e `UEFI-to-kernel boot verification: PASS` |
| Regressão de threads | `mingw32-make test-tasking-qemu` | PASS |
| Regressão de preempção | `mingw32-make test-preemption-qemu` | PASS; single-core |
| Regressão panic/exceções | `mingw32-make test-panic-qemu test-exceptions-qemu` | PASS |

O teste host cobre grants corretos, direitos insuficientes, tipo errado,
handles inválidos/forjados/de outro processo, derivação/transferência
atenuadas, tabela cheia, reuso de slot, lease, revogação, callback adiado,
invariantes e limites de processos/objetos. O autoteste QEMU valida uma
operação autorizada, rejeição de tipo e direito, derivação somente de leitura,
revogação e destruição após release do lease.

Os alvos QEMU observam a VM pelo período do harness e a encerram
deliberadamente; isso não é estabilidade longa. A saída de teste desta execução
foi gerada pelo alvo normal de boot e pelos alvos de regressão citados.

## Não executado ou não aplicável

- Concorrência real entre CPUs: não aplicável, kernel de uma CPU e sem SMP.
- Acesso cruzado entre processos ou a páginas do kernel vindo de Ring 3: não
  aplicável, CPL3 e address spaces de processo ausentes.
- IPC malformado, fuzzing de syscalls, quotas por processo, driver malicioso,
  DMA/IOMMU ou ataques de memória: não aplicável/sem subsistema integrado.
- Auditoria formal ou security review abrangente: não executada.

Os testes são evidência dos contratos internos cobertos; não certificam que o
sistema seja seguro. Ver [threat-model.md](threat-model.md) e
[address-space-isolation.md](address-space-isolation.md) para os limites.
