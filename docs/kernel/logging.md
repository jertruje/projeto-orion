# Kernel logging — Phase 6

## Record format

Each record has the form:

```text
[ORION][MODULE][LEVEL] message
```

Levels are `DEBUG`, `INFO`, `WARN`, and `ERROR`. The serial backend emits CRLF
line endings. The module name must be non-empty and at most 32 bytes. Messages
are limited to 192 bytes; a message that reaches the limit receives the
`...[truncated]` suffix and `orion_log` returns `false`. The caller must treat
that result as a logging failure; core initialization panics rather than
continuing as if the diagnostic succeeded.

`orion_log_u64` supports a bounded decimal `uint64_t` field without libc
formatting. Neither interface allocates memory or calls `printf`.

## Failure and concurrency

Null inputs, invalid levels, empty/overlong module names, truncation, and serial
write failures return `false`. Invalid module names are rejected before any
record bytes are emitted. A serial failure may leave a partial record.

The implementation is synchronous, polling, and has no lock. It is only used
during the current single-CPU initialization path with interrupts disabled.
Concurrent callers, interrupt handlers, and SMP are unsupported until
synchronization and reentrancy contracts are designed and tested.

## Tests

`tests/kernel/log_test.c` exercises record format, CRLF, `UINT64_MAX`, message
limit behavior, invalid/overlong inputs, and serial failure through a host-side
serial mock. It does not validate physical UART availability or timing.
