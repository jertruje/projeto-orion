#include "log.h"

#include "serial.h"

static const char *level_name(orion_log_level level)
{
    switch (level) {
    case ORION_LOG_DEBUG:
        return "DEBUG";
    case ORION_LOG_INFO:
        return "INFO";
    case ORION_LOG_WARN:
        return "WARN";
    case ORION_LOG_ERROR:
        return "ERROR";
    default:
        return "INVALID";
    }
}

static bool begin_line(orion_log_level level, const char *module)
{
    uint32_t module_length = 0;
    while (module_length <= ORION_LOG_MODULE_LIMIT &&
           module[module_length] != '\0') {
        ++module_length;
    }
    if (module_length == 0 || module_length > ORION_LOG_MODULE_LIMIT) {
        return false;
    }

    return orion_serial_write("[ORION][", 8) &&
           orion_serial_write(module, ORION_LOG_MODULE_LIMIT) &&
           orion_serial_write("][", 2) &&
           orion_serial_write(level_name(level), 8) &&
           orion_serial_write("] ", 2);
}

static bool end_line(void)
{
    return orion_serial_write("\n", 1);
}

static bool write_message(const char *message, bool *truncated)
{
    uint32_t index = 0;
    while (index < ORION_LOG_TEXT_LIMIT && message[index] != '\0') {
        if (!orion_serial_putc(message[index++])) {
            return false;
        }
    }
    *truncated = index == ORION_LOG_TEXT_LIMIT;
    return !*truncated || orion_serial_write("...[truncated]", 14);
}

bool orion_log(orion_log_level level, const char *module, const char *message)
{
    bool truncated = false;

    if (module == 0 || message == 0 || level < ORION_LOG_DEBUG ||
        level > ORION_LOG_ERROR) {
        return false;
    }

    if (!begin_line(level, module)) {
        return false;
    }
    if (!write_message(message, &truncated) || !end_line()) {
        return false;
    }
    return !truncated;
}

bool orion_log_u64(orion_log_level level, const char *module,
                   const char *message, uint64_t value)
{
    char digits[20];
    uint32_t count = 0;
    bool truncated = false;

    if (module == 0 || message == 0 || level < ORION_LOG_DEBUG ||
        level > ORION_LOG_ERROR) {
        return false;
    }
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0 && count < sizeof(digits));

    if (!begin_line(level, module) || !write_message(message, &truncated) ||
        !orion_serial_putc(' ')) {
        return false;
    }
    while (count != 0) {
        if (!orion_serial_putc(digits[--count])) {
            return false;
        }
    }
    return end_line() && !truncated;
}
