#include <orion/syscall.h>

#include <stddef.h>

#define TEST_UNKNOWN_SYSCALL UINT64_C(0xffffffffffffffff)

static int64_t invoke_unknown_syscall(void)
{
    register uint64_t result __asm__("rax") = TEST_UNKNOWN_SYSCALL;
    register uint64_t first __asm__("rdi") = 0;
    register uint64_t second __asm__("rsi") = 0;
    register uint64_t third __asm__("rdx") = 0;
    __asm__ volatile("int $0x80"
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}

int orion_app_main(void)
{
    static const char greeting[] = "ORION independent CPL3 application\n";
    static const char passed[] =
        "ORION syscall ABI, info, and invalid-input checks passed\n";
    orion_lib_system_info information = {0};

    if (orion_write(greeting, sizeof(greeting) - 1U) !=
            (int64_t)(sizeof(greeting) - 1U) ||
        orion_system_info(&information) != 0 ||
        information.abi_version != ORION_LIB_ABI_VERSION ||
        information.structure_size != sizeof(information) ||
        information.process_id != 2 ||
        information.page_size != 4096 ||
        information.maximum_write_bytes != 256 ||
        orion_write(NULL, 1) != ORION_LIB_ERROR_BAD_ADDRESS ||
        orion_write((const void *)(uintptr_t)UINT64_C(0xffff800000000000),
                    1) != ORION_LIB_ERROR_BAD_ADDRESS ||
        invoke_unknown_syscall() != ORION_LIB_ERROR_NOT_SUPPORTED ||
        orion_write(NULL, 0) != 0) {
        return 1;
    }
    if (orion_write(passed, sizeof(passed) - 1U) !=
        (int64_t)(sizeof(passed) - 1U)) {
        return 2;
    }
    return 0;
}
