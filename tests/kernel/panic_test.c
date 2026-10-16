#include "../../kernel/panic/panic.h"
#include "../../kernel/log/log.h"

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
    ++disable_calls;
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

    if (disable_calls != (recursive_case ? 2U : 1U) ||
        halt_calls != 1U || log_calls != 1U) {
        return 1;
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
                   : 1;
    }
    if (wrapper_case) {
        return strings_equal(last_module, "CORE") &&
                       strings_equal(last_message, "wrapper reason") &&
                       last_level == ORION_LOG_ERROR
                   ? 0
                   : 1;
    }
    return strings_equal(last_module, "TEST") &&
                   strings_equal(last_message, "fatal test reason") &&
                   last_level == ORION_LOG_ERROR &&
                   (failed_log_case || log_result)
               ? 0
               : 1;
}
