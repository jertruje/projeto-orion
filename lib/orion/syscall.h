#ifndef ORION_LIB_SYSCALL_H
#define ORION_LIB_SYSCALL_H

#include <stdint.h>

#define ORION_LIB_ABI_VERSION 1U
#define ORION_LIB_SYSCALL_CONSOLE_WRITE 1U
#define ORION_LIB_SYSCALL_SYSTEM_INFO 2U
#define ORION_LIB_SYSCALL_PROCESS_EXIT 3U
#define ORION_LIB_SYSCALL_CAP_ACQUIRE 4U
#define ORION_LIB_SYSCALL_CAP_DERIVE 5U
#define ORION_LIB_SYSCALL_CAP_TRANSFER 6U
#define ORION_LIB_SYSCALL_CAP_CLOSE 7U

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
int64_t orion_capability_acquire(orion_capability_handle handle, orion_object_type expected_type, uint32_t required_rights, orion_capability_lease *lease);
int64_t orion_capability_derive(orion_capability_handle handle, uint32_t reduced_rights, orion_capability_handle *derived_handle);
int64_t orion_capability_transfer(orion_capability_handle handle, uint64_t target_process_id, uint32_t reduced_rights, orion_capability_handle *transferred_handle);
int64_t orion_capability_close(orion_capability_handle handle);
