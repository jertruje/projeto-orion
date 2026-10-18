#include "../../kernel/panic/panic.h"
#include "../../kernel/log/log.h"
#include "../../kernel/arch/x86_64/interrupt_state.h"
#include "../../kernel/fault/report.h"

#include <setjmp.h>
#include <stdlib.h>

static jmp_buf halt_environment;
static volatile bool halt_armed;
static bool recursive_log;
static bool log_result = true;
static uint32_t disable_calls;
static uint32_t halt_calls;
static uint32_t log_calls;
static orion_log_level last_level;
static const char *last_module;
static const char *last_message;
static bool interrupts_enabled = true;

static bool strings_equal(const char *left, const char *right)
{
    uint32_t index = 0;
    if (left == 0 || right == 0) {
        return left == right;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return false;
        }
        ++index;
    }
    return left[index] == right[index];
}

void orion_arch_disable_interrupts(void)
{
    interrupts_enabled = false;
    ++disable_calls;
}

bool orion_x86_64_interrupts_save_disable(void)
{
    const bool previous = interrupts_enabled;
    interrupts_enabled = false;
    return previous;
}

void orion_x86_64_interrupts_restore(bool enabled)
{
    interrupts_enabled = enabled;
}

_Noreturn void orion_arch_halt(void)
{
    ++halt_calls;
    if (!halt_armed) {
        abort();
    }
    longjmp(halt_environment, 1);
}

bool orion_log(orion_log_level level, const char *module, const char *message)
{
    ++log_calls;
    last_level = level;
    last_module = module;
    last_message = message;
    if (recursive_log) {
        recursive_log = false;
        orion_panic_at("NESTED", "recursive panic");
    }
    return log_result;
}

bool orion_log_u64(orion_log_level level, const char *module,
                   const char *message, uint64_t value)
{
    (void)level;
    (void)module;
    (void)message;
    (void)value;
    return false;
}

int main(int argc, char **argv)
{
    bool recursive_case;
    bool failed_log_case;
    bool defaults_case;
    bool wrapper_case;
    if (argc != 2) {
        return 2;
    }
    recursive_case = strings_equal(argv[1], "recursive");
    failed_log_case = strings_equal(argv[1], "log-failure");
    defaults_case = strings_equal(argv[1], "defaults");
    wrapper_case = strings_equal(argv[1], "wrapper");
    if (!recursive_case && !failed_log_case &&
        !defaults_case && !wrapper_case && !strings_equal(argv[1], "normal")) {
        return 2;
    }

    recursive_log = recursive_case;
    log_result = !failed_log_case;
    halt_armed = true;
    if (setjmp(halt_environment) == 0) {
        if (recursive_case || failed_log_case) {
            orion_panic_at("TEST", "fatal test reason");
        } else if (defaults_case) {
            orion_panic_at(0, 0);
        } else if (wrapper_case) {
            orion_panic("wrapper reason");
        } else {
            orion_panic_at("TEST", "fatal test reason");
        }
        return 3;
    }
    halt_armed = false;

    {
        orion_fault_record report = {0};
        if (orion_fault_report_count() != 1U ||
            orion_fault_report_get(1, &report) != ORION_FAULT_STATUS_OK ||
            report.category != ORION_FAULT_KERNEL_PANIC ||
            report.action != ORION_FAULT_ACTION_HALT_KERNEL ||
            report.recovery_result !=
                ORION_FAULT_RECOVERY_NOT_ATTEMPTED) {
            return 1;
        }
    }
    if (disable_calls != (recursive_case ? 2U : 1U) ||
        halt_calls != 1U || log_calls != 1U) {
        return 2;
    }
    if (recursive_case) {
        return 0;
    }
    if (defaults_case) {
        return strings_equal(last_module, "PANIC") &&
                       strings_equal(last_message,
                                     "unspecified fatal failure") &&
                       last_level == ORION_LOG_ERROR
                   ? 0
                       : 3;
    }
    if (wrapper_case) {
        return strings_equal(last_module, "CORE") &&
                       strings_equal(last_message, "wrapper reason") &&
                       last_level == ORION_LOG_ERROR
                   ? 0
                   : 4;
    }
    return strings_equal(last_module, "TEST") &&
                   strings_equal(last_message, "fatal test reason") &&
                   last_level == ORION_LOG_ERROR &&
                   (failed_log_case || log_result)
               ? 0
               : 5;
}
