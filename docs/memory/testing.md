# Memory tests

Run `mingw32-make test-unit build inspect test-boot test-exceptions-qemu`.

Host memory tests cover valid UEFI descriptors, overlapping and misaligned
ranges, conventional-memory accounting, reserved-page exclusion, aligned
allocation, exhaustion, invalid/double release, and bitmap invariants. The
page-table host test covers canonicality, map/lookup/unmap, permissions, W^X,
duplicate/unaligned mappings, OOM rollback, and table ownership/release.

The normal QEMU boot validates its actual UEFI map, initializes the physical
allocator, checks allocator invariants, and performs allocate/release plus an
active-CR3 map/write/lookup/unmap smoke test. Isolated QEMU exception images
verify the #PF CR2/error decoding and fatal handling, including a write to a
read-only page (error code 3) and execution from an NX page (error code 17).
The tested environment is QEMU 11.1.2 with EDK2; it does not establish
portability to other firmware or physical machines, nor does it test user
isolation, SMP, demand paging, or prolonged stability.
