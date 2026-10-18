#ifndef ORION_X86_64_INTERRUPT_STATE_H
#define ORION_X86_64_INTERRUPT_STATE_H

#include <stdbool.h>

bool orion_x86_64_interrupts_enabled(void);
bool orion_x86_64_interrupts_save_disable(void);
void orion_x86_64_interrupts_restore(bool was_enabled);
void orion_x86_64_interrupts_disable(void);
void orion_x86_64_interrupts_enable(void);

#endif
