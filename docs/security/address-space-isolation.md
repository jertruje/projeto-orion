# Isolamento de espaço de endereçamento

## Evidência atual

O gerenciador de memória da Fase 8 valida operações de page table e exercita
um clone da PML4 herdada em QEMU, incluindo RO/NX. Isso é um teste do mecanismo
de bootstrap, não de um espaço de endereçamento por processo. O processo atual
é único e executa em Ring 0; não existe root CR3 independente associado a cada
processo nem permissão de usuário demonstrada.

**Conclusão:** isolamento entre processos e proteção efetiva Ring 3 permanecem
pendentes. A tabela de capabilities não altera permissões de páginas e não
deve ser tratada como substituta de isolamento de memória.

## Critérios para fechar o gate

1. Cada processo possui root de page tables e ownership/lifetime testados.
2. User mappings limitados ao intervalo autorizado, com checagem de overflow,
   canonicalidade, alinhamento e permissões em map/unmap.
3. Mapeamentos do kernel são supervisor-only; páginas RO e NX falham no
   acesso correspondente vindo de CPL3.
4. Troca de CR3 e TLB invalidation são seguras; política SMP definida antes
   de múltiplos processadores.
5. Teste QEMU executa duas tarefas em Ring 3 e confirma que uma não consegue
   ler/escrever a memória da outra ou do kernel.
6. Falhas de page fault de usuário terminam/isolam o processo sem corromper
   kernel ou outro processo; os testes verificam logs e invariantes.

Nenhum dos testes de isolamento entre processos foi possível nesta fase porque
Ring 3 e address spaces de processo estão ausentes.
