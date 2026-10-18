#include "panic.h"

#include "../arch/x86_64/fatal.h"
#include "../fault/report.h"
#include "log.h"

static volatile unsigned int panic_active;

_Noreturn void orion_panic_at(const char *module, const char *reason)
{
    orion_arch_disable_interrupts();
    if (panic_active != 0) {
        orion_arch_halt();
    }
    panic_active = 1;
    orion_fault_record fault = {
        .category = ORION_FAULT_KERNEL_PANIC,
        .context = ORION_FAULT_CONTEXT_UNKNOWN,
        .action = ORION_FAULT_ACTION_HALT_KERNEL,
        .recovery_result = ORION_FAULT_RECOVERY_NOT_ATTEMPTED
    };
    uint64_t event_id = 0;
    if (orion_fault_report_record(&fault, &event_id) == ORION_FAULT_STATUS_OK) {
        (void)orion_log_u64(ORION_LOG_ERROR, "PANIC",
                            "Structured fault event ID", event_id);
    } else {
        (void)orion_log(ORION_LOG_ERROR, "PANIC",
                        "Structured fault report unavailable");
    }
    (void)orion_log(ORION_LOG_ERROR,
                    module != 0 && module[0] != '\0' ? module : "PANIC",
                    reason != 0 ? reason : "unspecified fatal failure");
    orion_arch_halt();
}

_Noreturn void orion_panic(const char *reason)
{
    orion_panic_at("CORE", reason);
}
