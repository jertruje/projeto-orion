#ifndef ORION_LIB_SYSCALL_H
#define ORION_LIB_SYSCALL_H

#include <stdint.h>

#define ORION_LIB_ABI_VERSION 1U
#define ORION_LIB_SYSCALL_CONSOLE_WRITE 1U
#define ORION_LIB_SYSCALL_SYSTEM_INFO 2U
#define ORION_LIB_SYSCALL_PROCESS_EXIT 3U

typedef struct {
    uint32_t abi_version;
    uint32_t structure_size;
    uint64_t process_id;
    uint64_t page_size;
    uint64_t maximum_write_bytes;
    uint64_t feature_flags;
} orion_lib_system_info;

_Static_assert(sizeof(orion_lib_system_info) == 40,
               "public system information ABI changed");

enum {
    ORION_LIB_ERROR_INVALID_ARGUMENT = -1,
    ORION_LIB_ERROR_BAD_ADDRESS = -2,
    ORION_LIB_ERROR_NOT_SUPPORTED = -3,
    ORION_LIB_ERROR_DEVICE = -4,
    ORION_LIB_ERROR_PERMISSION = -5,
    ORION_LIB_ERROR_NO_MEMORY = -6
};

int64_t orion_write(const void *buffer, uint64_t bytes);
int64_t orion_system_info(orion_lib_system_info *information);
_Noreturn void orion_exit(uint64_t exit_code);

#endif
