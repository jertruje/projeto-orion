# Kernel thread stacks

The bootstrap thread uses the existing 64-KiB kernel entry stack. Each
additional thread receives a distinct 16-KiB slot from a statically reserved,
page-aligned BSS array. A 64-byte bottom canary is checked by scheduler
invariant tests. This is diagnostic checking, not a hardware guard page; an
overflow can corrupt adjacent kernel data before detection.

The saved stack pointer must remain inside its declared bounds. A terminated
thread's stack is retained until execution has switched away and an explicit
destroy call succeeds. Because TCB slots and IDs are not reused during this
bring-up, the fixed pool imposes a lifetime limit even after destruction.
There is no physical allocator integration, stack growth, guard mapping,
user stack, or per-process stack.
