#include "user_program.h"

#include "../arch/x86_64/gdt.h"
#include "../arch/x86_64/interrupt_state.h"
#include "../arch/x86_64/user_mode.h"
#include "../exec/elf64.h"
#include "../log/log.h"
#include "../mm/physical.h"
#include "../syscall/syscall.h"
#include "../task/process.h"

#define ORION_USER_PROCESS_ID UINT64_C(2)

extern char kernel_stack_top[];

bool orion_user_program_run(const orion_boot_info *boot_info,
                            uint64_t *exit_code)
{
    if (!orion_boot_info_is_valid(boot_info) ||
        boot_info->application_image == NULL ||
        boot_info->application_image_size == 0 || exit_code == NULL) {
        (void)orion_log(ORION_LOG_ERROR, "USER",
                        "Invalid user application handoff");
        return false;
    }
    const bool interrupts_were_enabled =
        orion_x86_64_interrupts_save_disable();
    orion_elf64_image image;
    const orion_elf64_status image_status = orion_elf64_validate(
        boot_info->application_image, boot_info->application_image_size,
        &image);
    if (image_status != ORION_ELF64_OK) {
        (void)orion_log_u64(ORION_LOG_ERROR, "USER",
                            "User ELF validation status:", image_status);
        orion_x86_64_interrupts_restore(interrupts_were_enabled);
        return false;
    }

    orion_user_address_space address_space = {0};
    uint64_t entry = 0;
    uint64_t stack = 0;
    if (!orion_elf64_load_user(&image, &address_space, &entry, &stack)) {
        (void)orion_log(ORION_LOG_ERROR, "USER",
                        "User ELF mapping or load failed");
        orion_x86_64_interrupts_restore(interrupts_were_enabled);
        return false;
    }
    orion_process process = {
        .id = ORION_USER_PROCESS_ID,
        .state = ORION_PROCESS_ACTIVE,
        .threads = NULL,
        .thread_count = 0,
        .address_space = &address_space,
        .resources = NULL
    };
    if (!orion_syscall_set_process(&process)) {
        (void)orion_log(ORION_LOG_ERROR, "USER",
                        "Could not register the user process for syscalls");
        (void)orion_elf64_destroy_user(&address_space);
        orion_x86_64_interrupts_restore(interrupts_were_enabled);
        return false;
    }
    if (!orion_x86_64_tss_set_kernel_stack(
            (uintptr_t)orion_x86_64_syscall_stack_top)) {
        (void)orion_log(ORION_LOG_ERROR, "USER",
                        "Could not configure the user syscall stack");
        (void)orion_syscall_set_process(NULL);
        (void)orion_elf64_destroy_user(&address_space);
        orion_x86_64_interrupts_restore(interrupts_were_enabled);
        return false;
    }

    *exit_code = orion_x86_64_enter_user(
        entry, stack, address_space.root_physical.value);
    const bool process_exited =
        process.state == ORION_PROCESS_TERMINATED;
    const bool process_cleared = orion_syscall_set_process(NULL);
    const bool stack_restored =
        orion_x86_64_tss_set_kernel_stack((uintptr_t)kernel_stack_top);
    const bool pages_released =
        orion_elf64_destroy_user(&address_space);
    if (!process_exited || !process_cleared || !stack_restored ||
        !pages_released) {
        (void)orion_log_u64(ORION_LOG_ERROR, "USER",
                            "User process exit code:", *exit_code);
        (void)orion_log_u64(ORION_LOG_ERROR, "USER",
                            "User address-space cleanup status:",
                            pages_released);
    }
    const bool memory_valid = orion_physical_check_invariants();
    orion_x86_64_interrupts_restore(interrupts_were_enabled);
    return process_exited && process_cleared && stack_restored &&
           pages_released && memory_valid;
}
