#include "panic.h"

#include "../arch/x86_64/fatal.h"
#include "log.h"

static volatile unsigned int panic_active;

_Noreturn void orion_panic_at(const char *module, const char *reason)
{
    orion_arch_disable_interrupts();
    if (panic_active != 0) {
        orion_arch_halt();
    }
    panic_active = 1;
    (void)orion_log(ORION_LOG_ERROR,
                    module != 0 && module[0] != '\0' ? module : "PANIC",
                    reason != 0 ? reason : "unspecified fatal failure");
    orion_arch_halt();
}

_Noreturn void orion_panic(const char *reason)
{
    orion_panic_at("CORE", reason);
}
