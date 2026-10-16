#ifndef ORION_X86_64_INTERRUPT_STATE_H
#define ORION_X86_64_INTERRUPT_STATE_H

#include <stdbool.h>

bool orion_x86_64_interrupts_enabled(void);
void orion_x86_64_interrupts_disable(void);
void orion_x86_64_interrupts_enable(void);

#endif
