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
int64_t orion_capability_acquire(orion_capability_handle handle, orion_object_type expected_type, uint32_t required_rights, orion_capability_lease *lease)
{
    orion_capability_acquire_args args = {
        .handle = handle,
        .expected_type = expected_type,
        .required_rights = required_rights
    };

    register uint64_t result __asm__("rax") = ORION_LIB_SYSCALL_CAP_ACQUIRE;
    register uint64_t first __asm__("rdi") = (uint64_t)(uintptr_t)&args;
    register uint64_t second __asm__("rsi") = (uint64_t)(uintptr_t)lease;
    register uint64_t third __asm__("rdx") = 0;
    __asm__ volatile("int "
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}

int64_t orion_capability_derive(orion_capability_handle handle, uint32_t reduced_rights, orion_capability_handle *derived_handle)
{
    orion_capability_derive_args args = {
        .handle = handle,
        .reduced_rights = reduced_rights
    };

    register uint64_t result __asm__("rax") = ORION_LIB_SYSCALL_CAP_DERIVE;
    register uint64_t first __asm__("rdi") = (uint64_t)(uintptr_t)&args;
    register uint64_t second __asm__("rsi") = (uint64_t)(uintptr_t)derived_handle;
    register uint64_t third __asm__("rdx") = 0;
    __asm__ volatile("int "
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}

int64_t orion_capability_transfer(orion_capability_handle handle, uint64_t target_process_id, uint32_t reduced_rights, orion_capability_handle *transferred_handle)
{
    orion_capability_transfer_args args = {
        .handle = handle,
        .target_process_id = target_process_id,
        .reduced_rights = reduced_rights
    };

    register uint64_t result __asm__("rax") = ORION_LIB_SYSCALL_CAP_TRANSFER;
    register uint64_t first __asm__("rdi") = (uint64_t)(uintptr_t)&args;
    register uint64_t second __asm__("rsi") = (uint64_t)(uintptr_t)transferred_handle;
    register uint64_t third __asm__("rdx") = 0;
    __asm__ volatile("int "
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}

int64_t orion_capability_close(orion_capability_handle handle)
{
    orion_capability_close_args args = {
        .handle = handle
    };

    register uint64_t result __asm__("rax") = ORION_LIB_SYSCALL_CAP_CLOSE;
    register uint64_t first __asm__("rdi") = (uint64_t)(uintptr_t)&args;
    register uint64_t second __asm__("rsi") = 0;
    register uint64_t third __asm__("rdx") = 0;
    __asm__ volatile("int "
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}

_Noreturn void orion_exit(uint64_t exit_code)
{
    (void)invoke(ORION_LIB_SYSCALL_PROCESS_EXIT, exit_code, 0, 0);
    for (;;) {
        __asm__ volatile("ud2");
    }
}
int64_t orion_capability_acquire(orion_capability_handle handle, orion_object_type expected_type, uint32_t required_rights, orion_capability_lease *lease)
{
    orion_capability_acquire_args args = {
        .handle = handle,
        .expected_type = expected_type,
        .required_rights = required_rights
    };

    register uint64_t result __asm__("rax") = ORION_LIB_SYSCALL_CAP_ACQUIRE;
    register uint64_t first __asm__("rdi") = (uint64_t)(uintptr_t)&args;
    register uint64_t second __asm__("rsi") = (uint64_t)(uintptr_t)lease;
    register uint64_t third __asm__("rdx") = 0;
    __asm__ volatile("int "
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}

int64_t orion_capability_derive(orion_capability_handle handle, uint32_t reduced_rights, orion_capability_handle *derived_handle)
{
    orion_capability_derive_args args = {
        .handle = handle,
        .reduced_rights = reduced_rights
    };

    register uint64_t result __asm__("rax") = ORION_LIB_SYSCALL_CAP_DERIVE;
    register uint64_t first __asm__("rdi") = (uint64_t)(uintptr_t)&args;
    register uint64_t second __asm__("rsi") = (uint64_t)(uintptr_t)derived_handle;
    register uint64_t third __asm__("rdx") = 0;
    __asm__ volatile("int "
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}

int64_t orion_capability_transfer(orion_capability_handle handle, uint64_t target_process_id, uint32_t reduced_rights, orion_capability_handle *transferred_handle)
{
    orion_capability_transfer_args args = {
        .handle = handle,
        .target_process_id = target_process_id,
        .reduced_rights = reduced_rights
    };

    register uint64_t result __asm__("rax") = ORION_LIB_SYSCALL_CAP_TRANSFER;
    register uint64_t first __asm__("rdi") = (uint64_t)(uintptr_t)&args;
    register uint64_t second __asm__("rsi") = (uint64_t)(uintptr_t)transferred_handle;
    register uint64_t third __asm__("rdx") = 0;
    __asm__ volatile("int "
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}

int64_t orion_capability_close(orion_capability_handle handle)
{
    orion_capability_close_args args = {
        .handle = handle
    };

    register uint64_t result __asm__("rax") = ORION_LIB_SYSCALL_CAP_CLOSE;
    register uint64_t first __asm__("rdi") = (uint64_t)(uintptr_t)&args;
    register uint64_t second __asm__("rsi") = 0;
    register uint64_t third __asm__("rdx") = 0;
    __asm__ volatile("int "
                     : "+a"(result)
                     : "D"(first), "S"(second), "d"(third)
                     : "memory", "cc");
    return (int64_t)result;
}
