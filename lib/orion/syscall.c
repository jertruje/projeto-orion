#include "syscall.h"

static int64_t invoke(uint64_t number, uint64_t argument0,
                      uint64_t argument1, uint64_t argument2)
{
    register uint64_t result __asm__("rax") = number;
    register uint64_t first __asm__("rdi") = argument0;
    register uint64_t second __asm__("rsi") = argument1;
    register uint64_t third __asm__("rdx") = argument2;
    __asm__ volatile("int $0x80"
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}

int64_t orion_write(const void *buffer, uint64_t bytes)
{
    return invoke(ORION_LIB_SYSCALL_CONSOLE_WRITE,
                  (uint64_t)(uintptr_t)buffer, bytes, 0);
}

int64_t orion_system_info(orion_lib_system_info *information)
{
    return invoke(ORION_LIB_SYSCALL_SYSTEM_INFO,
                  (uint64_t)(uintptr_t)information, 0, 0);
}

_Noreturn void orion_exit(uint64_t exit_code)
{
    (void)invoke(ORION_LIB_SYSCALL_PROCESS_EXIT, exit_code, 0, 0);
    for (;;) {
        __asm__ volatile("ud2");
    }
}
