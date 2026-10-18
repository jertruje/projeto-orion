# Ciclo de vida de capabilities e objetos

## Estados e operações

1. **Criação inicial:** somente o kernel cria o objeto e instala uma capability
   inicial em sua própria tabela. Tabelas de processo, objetos, slots e leases
   têm limites estáticos; esgotamento retorna status explícito.
2. **Derivação:** cria outro slot para o mesmo objeto apenas após validar
   `DERIVE`; a máscara resultante deve ser subset da origem.
3. **Transferência:** cria slot na tabela-alvo apenas após validar `TRANSFER`,
   processo-alvo registrado e subset dos direitos. A origem permanece válida.
4. **Consulta/uso:** `describe` e `acquire` validam handle e direitos; `acquire`
   retorna lease que mantém o objeto vivo.
5. **Fechamento:** `close` remove um slot, reduz a contagem de referências e
   incrementa a geração do slot. Não revoga outros slots.
6. **Revogação:** exige `REVOKE`, fecha todos os slots do objeto em todas as
   tabelas registradas e marca o objeto revogado. Novos lookups falham.
7. **Destruição:** o callback é chamado fora da seção crítica quando não há
   capability, lease nem referência inicial do kernel. Com leases ativos, a
   destruição espera pelo último `lease_release`.
8. **Liberação de tabela de processo:** fecha os seus slots; falha com
   `OBJECT_BUSY` se o processo ainda possui leases. A vida do objeto pode
   continuar se houver outros handles, referência inicial ou leases.

## Limitações e erros

Um lease já adquirido antes da revogação permanece válido até seu release;
revogação impede novos acquires, mas não cancela trabalho já em curso. A API
não fornece bloqueio de operações em andamento nem espera por quiescência.
Slots/recursos aposentados ou esgotados não são reutilizados sem segurança.
Gerações têm largura finita; slots são aposentados ao chegar ao limite para
evitar wrap, e tabelas/objetos também são aposentados ao limite de suas
gerações.

Um processo encerrado não pode ser liberado enquanto possuir leases. A
integração de lifecycle de processos ainda precisa chamar as operações de
limpeza no caminho real de término; no kernel atual existe apenas o processo
kernel. Erros de handle, tipo, direitos, revogação, objeto busy e corrupção
retornam estados explícitos e falham fechados.

Testes de encerramento, revogação, leases, reuso, exaustão e invariantes estão
detalhados em [security-testing.md](security-testing.md).
