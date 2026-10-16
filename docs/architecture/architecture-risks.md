# Architecture risks and unresolved decisions

| ID | Risk/open question | Current proposal | Evidence or decision needed | Gate/owner |
|---|---|---|---|---|
| AR-01 | Fase 2 evidence is documentary and uneven across projects; no common performance comparison | Use microkernel/capability as architecture baseline without superiority claims | Expand primary-paper/spec review; record assumptions; revise ADR-0009 only if implementation evidence contradicts baseline | Fases 11–14 and before release claims |
| AR-02 | IPC cost unknown | Start bounded copy IPC | Prototype round-trip, throughput, percentiles, CPU cost and failure behavior | Fases 9–12 |
| AR-03 | Capability model has complex derivation/revocation | Process-local generational handles; explicit transfer; rights attenuation only; object-wide revocation invalidates that object generation | Property-test stale handles, revocation races, rights subsets and slot reuse | Fase 11 |
| AR-04 | Boot trust root absent | Verified boot is target, not current guarantee | Decide keys, enrollment/recovery, rollback counter and firmware dependency | Fase 18 |
| AR-05 | No user-mode loader/bootstrap bundle | Carry the initial task and minimum supervisor in a boot bundle loaded by the loader; do not depend on a filesystem service to load itself | Define bundle format, signing root, versioning and bootstrap authority; test rejection/failure paths | Fases 5, 18, 19 |
| AR-06 | DMA isolation unavailable on some machines | IOMMU when verified; otherwise restrict device trust | Determine target hardware and fail-closed policy | Fase 14 |
| AR-07 | Physical allocator performance is unmeasured | Begin with a reserved-by-default bitmap allocator for page ownership; no contiguous-allocation guarantee | Test map scale, exhaustion, fragmentation and metadata budget; replace only on measured need | Fase 8 |
| AR-08 | SMP multiplies correctness burden | Single CPU first; APs later | IRQ-safe locking, shootdown, ownership and stress test evidence | Fase 10 |
| AR-09 | Service restart may corrupt durable state | Restart only on idempotent/validated service contract | Storage transaction semantics and crash injection | Fases 13, 16 |
| AR-10 | Platform and compatibility scope undefined | QEMU/EDK2 development target only | Human selection of physical platform, Linux reference distro and app ABI goals | Before release |
| AR-11 | Driver placement may conflict with performance/hardware constraints | User mode by default, explicit trusted exception | Compare device protocol overhead, IRQ latency and DMA model | Fase 14 |
| AR-12 | Naming says phases 1–24 but file contains phase 25 and an unnumbered design brief | Preserve source specification; keep phase mapping documented | Human confirmation of total phase numbering and target release scope | Before batch completion |

No row is resolved merely by adding documentation. Each decision needs the
experiment or policy described.
