# Tipos de objeto e direitos

Tipos e máscaras permitidas implementados em `kernel/security/capability.h`
e `capability.c`:

| Tipo | Direitos permitidos |
|---|---|
| `PROCESS` | `QUERY`, `ADMIN`, `DERIVE`, `TRANSFER`, `REVOKE` |
| `THREAD` | `QUERY`, `USE`, `ADMIN`, `DERIVE`, `TRANSFER`, `REVOKE` |
| `ADDRESS_SPACE` | `QUERY`, `USE`, `MODIFY`, `DERIVE`, `TRANSFER`, `REVOKE` |
| `MEMORY_REGION` | `QUERY`, `USE`, `MODIFY`, `DERIVE`, `TRANSFER`, `REVOKE` |
| `TIMER` | `QUERY`, `USE`, `MODIFY`, `DERIVE`, `TRANSFER`, `REVOKE` |
| `DEVICE` | `QUERY`, `USE`, `MODIFY`, `ADMIN`, `DERIVE`, `TRANSFER`, `REVOKE` |
| `CHANNEL` | `QUERY`, `DERIVE`, `TRANSFER`, `REVOKE`, `SEND`, `RECEIVE`, `CLOSE` |
| `STORAGE` | `QUERY`, `USE`, `MODIFY`, `ADMIN`, `DERIVE`, `TRANSFER`, `REVOKE` |

Significado geral: `QUERY` permite consulta autorizada; `USE` permite usar o
objeto; `MODIFY` altera estado; `ADMIN` executa operações administrativas;
`DERIVE` permite derivar uma capability atenuada; `TRANSFER` permite transferir
direitos selecionados; `REVOKE` permite revogar o objeto e todas as suas
capabilities. O significado operacional específico de `USE`, `MODIFY` e
`ADMIN` ainda deve ser definido por cada subsistema quando ele passar a usar o
tipo.

O catálogo tipado existe, mas `CHANNEL`, `DEVICE`, `STORAGE`, processo/thread,
address-space e memória não estão ainda expostos como objetos funcionais com
operações protegidas por capability. A máscara permitida não declara suporte
ao dispositivo ou à operação correspondente.

Todos os bits necessários numa operação são cumulativos. Direitos concedidos,
derivados ou transferidos com bits não permitidos para o tipo são rejeitados;
não há concessão implícita de todos os direitos ao destinatário.

`SEND`, `RECEIVE` e `CLOSE` foram acrescentados para endpoints IPC na Fase 12.
As máscaras dos outros tipos não incluem esses direitos.
