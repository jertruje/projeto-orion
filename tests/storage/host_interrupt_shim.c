#include <stdbool.h>

bool orion_x86_64_interrupts_enabled(void)
{
    return false;
}

bool orion_x86_64_interrupts_save_disable(void)
{
    return false;
}

void orion_x86_64_interrupts_restore(bool was_enabled)
{
    (void)was_enabled;
}

void orion_x86_64_interrupts_disable(void)
{
}

void orion_x86_64_interrupts_enable(void)
{
}
