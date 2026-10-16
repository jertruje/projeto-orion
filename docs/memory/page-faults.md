# Page-fault diagnostics

Vector 14 handling records CR2 and the architectural error code, then reports
the present/protection bit, write bit, user/supervisor bit, reserved-bit
violation bit, and, when NX+NXE is active, instruction-fetch bit. These fields
are diagnostic only. The current policy remains fail-stop through the kernel
panic path.

There is no demand paging, copy-on-write, stack growth, user-fault delivery,
or recovery from a page fault. Bits introduced by optional CPU features beyond
the reported five are not interpreted. QEMU's isolated not-present supervisor
read test checks CR2, error code, decoded fields, and fatal handling.
