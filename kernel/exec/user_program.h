#ifndef ORION_KERNEL_EXEC_USER_PROGRAM_H
#define ORION_KERNEL_EXEC_USER_PROGRAM_H

#include <stdbool.h>
#include <stdint.h>

#include <orion/boot_info.h>

bool orion_user_program_run(const orion_boot_info *boot_info,
                            uint64_t *exit_code);

#endif
