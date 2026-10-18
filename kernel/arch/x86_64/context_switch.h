#ifndef ORION_X86_64_CONTEXT_SWITCH_H
#define ORION_X86_64_CONTEXT_SWITCH_H

#include <stdint.h>

void orion_x86_64_context_switch(uint64_t **old_stack_pointer,
                                 uint64_t *new_stack_pointer);

#endif
