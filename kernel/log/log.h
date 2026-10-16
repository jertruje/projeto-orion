#ifndef ORION_LOG_H
#define ORION_LOG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ORION_LOG_DEBUG,
    ORION_LOG_INFO,
    ORION_LOG_WARN,
    ORION_LOG_ERROR
} orion_log_level;

#define ORION_LOG_MODULE_LIMIT 32U
#define ORION_LOG_TEXT_LIMIT 192U

bool orion_log(orion_log_level level, const char *module, const char *message);
bool orion_log_u64(orion_log_level level, const char *module,
                   const char *message, uint64_t value);

#endif
