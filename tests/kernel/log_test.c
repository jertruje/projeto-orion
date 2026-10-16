#include "../../kernel/log/log.h"
#include "../../kernel/console/serial.h"

#define OUTPUT_CAPACITY 512U
#define NO_FAILURE UINT32_MAX

static char output[OUTPUT_CAPACITY];
static uint32_t output_length;
static uint32_t fail_after = NO_FAILURE;
static uint32_t failures;

static void expect(bool condition)
{
    if (!condition) {
        ++failures;
    }
}

static bool output_equals(const char *expected)
{
    uint32_t index = 0;
    while (expected[index] != '\0' && index < OUTPUT_CAPACITY) {
        if (index >= output_length || output[index] != expected[index]) {
            return false;
        }
        ++index;
    }
    return index == output_length && expected[index] == '\0';
}

static bool output_ends_with(const char *expected)
{
    uint32_t length = 0;
    while (expected[length] != '\0') {
        ++length;
    }
    if (length > output_length) {
        return false;
    }
    for (uint32_t index = 0; index < length; ++index) {
        if (output[output_length - length + index] != expected[index]) {
            return false;
        }
    }
    return true;
}

static void reset_output(void)
{
    output_length = 0;
    fail_after = NO_FAILURE;
}

void orion_serial_init(void)
{
}

bool orion_serial_putc(char value)
{
    if (output_length == fail_after || output_length >= OUTPUT_CAPACITY) {
        return false;
    }
    output[output_length++] = value;
    return true;
}

bool orion_serial_write(const char *text, uint32_t limit)
{
    if (text == 0) {
        return false;
    }
    for (uint32_t index = 0; index < limit && text[index] != '\0'; ++index) {
        if (text[index] == '\n' && !orion_serial_putc('\r')) {
            return false;
        }
        if (!orion_serial_putc(text[index])) {
            return false;
        }
    }
    return true;
}

int main(void)
{
    expect(orion_log(ORION_LOG_INFO, "TEST", "hello"));
    expect(output_equals("[ORION][TEST][INFO] hello\r\n"));

    char boundary_module[ORION_LOG_MODULE_LIMIT + 1U];
    for (uint32_t index = 0; index < ORION_LOG_MODULE_LIMIT; ++index) {
        boundary_module[index] = 'M';
    }
    boundary_module[ORION_LOG_MODULE_LIMIT] = '\0';
    reset_output();
    expect(orion_log(ORION_LOG_INFO, boundary_module, ""));
    expect(output_length == 50U && output[8] == 'M' &&
           output[39] == 'M' && output[40] == ']');

    reset_output();
    expect(orion_log_u64(ORION_LOG_DEBUG, "TEST", "value",
                         UINT64_MAX));
    expect(output_equals("[ORION][TEST][DEBUG] value 18446744073709551615\r\n"));

    char long_message[ORION_LOG_TEXT_LIMIT + 1U];
    for (uint32_t index = 0; index < ORION_LOG_TEXT_LIMIT; ++index) {
        long_message[index] = 'x';
    }
    long_message[ORION_LOG_TEXT_LIMIT] = '\0';
    reset_output();
    expect(!orion_log(ORION_LOG_WARN, "TEST", long_message));
    expect(output_ends_with("...[truncated]\r\n"));

    char long_module[ORION_LOG_MODULE_LIMIT + 2U];
    for (uint32_t index = 0; index < ORION_LOG_MODULE_LIMIT + 1U; ++index) {
        long_module[index] = 'M';
    }
    long_module[ORION_LOG_MODULE_LIMIT + 1U] = '\0';
    reset_output();
    expect(!orion_log(ORION_LOG_INFO, long_module, "message"));
    expect(output_length == 0);

    reset_output();
    expect(!orion_log(ORION_LOG_INFO, "", "message"));
    expect(!orion_log((orion_log_level)99, "TEST", "message"));
    expect(!orion_log(ORION_LOG_INFO, 0, "message"));
    expect(!orion_log(ORION_LOG_INFO, "TEST", 0));
    expect(output_length == 0);

    reset_output();
    fail_after = 10;
    expect(!orion_log(ORION_LOG_ERROR, "TEST", "serial failure"));

    return failures == 0 ? 0 : 1;
}
