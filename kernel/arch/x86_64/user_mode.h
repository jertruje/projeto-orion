#ifndef ORION_X86_64_USER_MODE_H
#define ORION_X86_64_USER_MODE_H

#include <stdint.h>

uint64_t orion_x86_64_enter_user(uint64_t entry, uint64_t user_stack,
                                 uint64_t user_cr3);
extern uint8_t orion_x86_64_syscall_stack_bottom[];
extern uint8_t orion_x86_64_syscall_stack_top[];

#endif
