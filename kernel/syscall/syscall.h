#ifndef ORION_KERNEL_SYSCALL_SYSCALL_H
#define ORION_KERNEL_SYSCALL_SYSCALL_H

#include <stdbool.h>
#include <stdint.h>

#include "../arch/x86_64/idt.h"
#include "../task/process.h"

#define ORION_SYSCALL_ABI_VERSION 1U
#define ORION_SYSCALL_CONSOLE_WRITE 1U
#define ORION_SYSCALL_SYSTEM_INFO 2U
#define ORION_SYSCALL_PROCESS_EXIT 3U
#define ORION_SYSCALL_CAP_ACQUIRE 4U
#define ORION_SYSCALL_CAP_DERIVE 5U
#define ORION_SYSCALL_CAP_TRANSFER 6U
#define ORION_SYSCALL_CAP_CLOSE 7U
#define ORION_SYSCALL_RETURN_TO_KERNEL UINT64_MAX
#define ORION_SYSCALL_MAX_WRITE_BYTES 256U

typedef enum {
    ORION_ERROR_INVALID_ARGUMENT = -1,
    ORION_ERROR_BAD_ADDRESS = -2,
    ORION_ERROR_NOT_SUPPORTED = -3,
    ORION_ERROR_DEVICE = -4,
    ORION_ERROR_PERMISSION = -5,
    ORION_ERROR_NO_MEMORY = -6
} orion_syscall_error;

typedef struct {
    uint32_t abi_version;
    uint32_t structure_size;
    uint64_t process_id;
    uint64_t page_size;
    uint64_t maximum_write_bytes;
    uint64_t feature_flags;
} orion_system_info;

_Static_assert(sizeof(orion_system_info) == 40,
               "system information ABI size changed");

bool orion_syscall_set_process(orion_process *process);
void orion_syscall_dispatch(orion_x86_64_exception_frame *frame);
bool orion_syscall_terminate_user_exception(
    orion_x86_64_exception_frame *frame);
uint64_t orion_syscall_current_process_id(void);

#endif
