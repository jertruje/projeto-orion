# Language decisions

## Selected baseline

Use freestanding C for most kernel and loader logic, with Assembly only for
architecture-specific entry, privileged transitions, exception/interrupt
stubs and context switching. Rust is not required for the initial kernel
baseline. This refines ADR-0002 and is formalized in ADR-0015.

## Evaluation

| Language | Benefits | Risks/costs | Decision |
|---|---|---|---|
| C | Broad compiler support, direct ABI control, small runtime assumptions, compatible with current code and cross-target Clang. | Undefined behavior and memory-safety errors; requires checked arithmetic, bounds discipline, warnings, tests, static analysis and review. | Primary implementation language for the initial loader and kernel. Do not equate C freestanding with memory safety. |
| Assembly | Exact control over CPU state and ABI at entry/interrupt/context boundaries. | Architecture-specific, difficult to audit/maintain and unsuitable for large policy code. | Restrict to small x86-64 transition routines with documented register, stack, clobber and return contracts. |
| Rust | Ownership/type system can prevent classes of memory errors in safe code. `no_std` can support constrained environments. | Toolchain/target availability, panic/runtime/allocator requirements, FFI boundaries, generated code and debugging need deliberate validation; `unsafe` remains necessary for hardware and CPU state. Rust toolchain was not found in the inspected host. | Optional future evaluation, not a Phase 4 dependency and not selected for current kernel implementation. A proposal requires a measured component-specific benefit, supported target/runtime, ABI/FFI audit and CI/test coverage. |

## Constraints

- Keep public interfaces in fixed-width types and document calling convention,
  alignment, ownership and error behavior.
- Do not expose Rust or C layout-dependent structures across a binary/service
  ABI without explicit representation and versioning.
- Any future language change needs an ADR amendment/new ADR, build validation,
  debugger support and maintenance rationale.
- Tests and static checks complement review; language selection does not
  prove security.
