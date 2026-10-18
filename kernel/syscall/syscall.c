#include "syscall.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt_state.h"
#include "../console/serial.h"
#include "../mm/user_space.h"

#define ORION_USER_CODE_RPL3 UINT64_C(0x1b)
#define ORION_USER_SEGMENT_MASK UINT64_C(0xffff)

static orion_process *active_process;
static char write_buffer[ORION_SYSCALL_MAX_WRITE_BYTES + 1U];

static orion_user_address_space *active_user_space(void)
{
    return active_process == NULL
               ? NULL
               : (orion_user_address_space *)active_process->address_space;
}

static bool copy_from_user(void *destination, uint64_t source,
                           uint64_t length)
{
    orion_user_address_space *space = active_user_space();
    if (destination == NULL || space == NULL ||
        !orion_user_space_check(space, source, length, ORION_VM_READ)) {
        return false;
    }
    uint8_t *output = destination;
    const uint8_t *input = (const uint8_t *)(uintptr_t)source;
    for (uint64_t index = 0; index < length; ++index) {
        output[index] = input[index];
    }
    return true;
}

static bool copy_to_user(uint64_t destination, const void *source,
                         uint64_t length)
{
    orion_user_address_space *space = active_user_space();
    if (source == NULL || space == NULL ||
        !orion_user_space_check(space, destination, length, ORION_VM_WRITE)) {
        return false;
    }
    uint8_t *output = (uint8_t *)(uintptr_t)destination;
    const uint8_t *input = source;
    for (uint64_t index = 0; index < length; ++index) {
        output[index] = input[index];
    }
    return true;
}

bool orion_syscall_set_process(orion_process *process)
{
    if (orion_x86_64_interrupts_enabled() ||
        (process == NULL && active_process == NULL) ||
        (process != NULL &&
         (active_process != NULL || process->id == 0 ||
          process->state != ORION_PROCESS_ACTIVE ||
          process->address_space == NULL))) {
        return false;
    }
    active_process = process;
    return true;
}

uint64_t orion_syscall_current_process_id(void)
{
    return active_process == NULL ? 0 : active_process->id;
}

static int64_t dispatch_console_write(uint64_t address, uint64_t bytes)
{
    if (bytes > ORION_SYSCALL_MAX_WRITE_BYTES) {
        return ORION_ERROR_INVALID_ARGUMENT;
    }
    if (bytes == 0) {
        return 0;
    }
    if (!copy_from_user(write_buffer, address, bytes)) {
        return ORION_ERROR_BAD_ADDRESS;
    }
    write_buffer[bytes] = '\0';
    return orion_serial_write(write_buffer, (uint32_t)bytes)
               ? (int64_t)bytes
               : ORION_ERROR_DEVICE;
}

static int64_t dispatch_system_info(uint64_t address)
{
    const orion_system_info information = {
        .abi_version = ORION_SYSCALL_ABI_VERSION,
        .structure_size = sizeof(orion_system_info),
        .process_id = active_process->id,
        .page_size = UINT64_C(4096),
        .maximum_write_bytes = ORION_SYSCALL_MAX_WRITE_BYTES,
        .feature_flags = 0
    };
    return copy_to_user(address, &information, sizeof(information))
               ? 0
               : ORION_ERROR_BAD_ADDRESS;
}

void orion_syscall_dispatch(orion_x86_64_exception_frame *frame)
{
    if (frame == NULL) {
        return;
    }
    if ((frame->cs & UINT64_C(3)) != 3U || active_process == NULL ||
        active_process->state != ORION_PROCESS_ACTIVE) {
        frame->rax = (uint64_t)ORION_ERROR_PERMISSION;
        return;
    }
    if ((frame->cs & ORION_USER_SEGMENT_MASK) != ORION_USER_CODE_RPL3 ||
        (frame->vector != ORION_X86_64_SYSCALL_VECTOR)) {
        frame->rax = (uint64_t)ORION_ERROR_PERMISSION;
        return;
    }

    switch (frame->rax) {
    case ORION_SYSCALL_CONSOLE_WRITE:
        frame->rax = (uint64_t)dispatch_console_write(frame->rdi,
                                                       frame->rsi);
        break;
    case ORION_SYSCALL_SYSTEM_INFO:
        frame->rax = (uint64_t)dispatch_system_info(frame->rdi);
        break;
    case ORION_SYSCALL_PROCESS_EXIT:
        active_process->state = ORION_PROCESS_TERMINATED;
        frame->rax = frame->rdi;
        frame->vector = ORION_SYSCALL_RETURN_TO_KERNEL;
        break;
    default:
        frame->rax = (uint64_t)ORION_ERROR_NOT_SUPPORTED;
        break;
    }
}

bool orion_syscall_terminate_user_exception(
    orion_x86_64_exception_frame *frame)
{
    if (frame == NULL || (frame->cs & UINT64_C(3)) != 3U ||
        active_process == NULL ||
        active_process->state != ORION_PROCESS_ACTIVE) {
        return false;
    }
    active_process->state = ORION_PROCESS_TERMINATED;
    frame->rax = (uint64_t)ORION_ERROR_BAD_ADDRESS;
    frame->vector = ORION_SYSCALL_RETURN_TO_KERNEL;
    return true;
}
