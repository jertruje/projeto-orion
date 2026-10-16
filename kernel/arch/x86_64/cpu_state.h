#ifndef ORION_X86_64_CPU_STATE_H
#define ORION_X86_64_CPU_STATE_H

#include <stdbool.h>
#include <stdint.h>

bool orion_x86_64_validate_entry_state(uintptr_t stack_bottom,
                                       uintptr_t stack_top);
bool orion_x86_64_nx_enabled(void);

#endif
