# Memory tests

Run `mingw32-make test-unit build inspect test-boot test-exceptions-qemu`.

Host memory tests cover valid UEFI descriptors, overlapping and misaligned
ranges, conventional-memory accounting, reserved-page exclusion, aligned
allocation, exhaustion, invalid/double release, and bitmap invariants. The
page-table host test covers canonicality, map/lookup/unmap, permissions, W^X,
duplicate/unaligned mappings, OOM rollback, and table ownership/release.

The normal QEMU boot validates its actual UEFI map, initializes the physical
allocator, checks allocator invariants, and performs one allocate/release
smoke test. The isolated QEMU #PF test checks CR2/error decoding and fatal
handling. Page-table operations are not yet exercised against active hardware
page tables in QEMU; consequently Phase 8 remains incomplete despite passing
host tests and the physical-memory boot smoke test.
