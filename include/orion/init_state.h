#ifndef ORION_INIT_STATE_H
#define ORION_INIT_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ORION_INIT_ENTRY,
    ORION_INIT_DIAGNOSTICS_CONFIGURED,
    ORION_INIT_BOOT_INFO_VALID,
    ORION_INIT_CPU_STATE_VALID,
    ORION_INIT_ARCH_TABLES_READY,
    ORION_INIT_CORE_READY,
    ORION_INIT_IDLE,
    ORION_INIT_STAGE_COUNT
} orion_init_stage;

static inline bool orion_init_stage_is_valid(orion_init_stage stage)
{
    return stage >= ORION_INIT_ENTRY && stage < ORION_INIT_STAGE_COUNT;
}

static inline const char *orion_init_stage_name(orion_init_stage stage)
{
    switch (stage) {
    case ORION_INIT_ENTRY:
        return "ENTRY";
    case ORION_INIT_DIAGNOSTICS_CONFIGURED:
        return "DIAGNOSTICS_CONFIGURED";
    case ORION_INIT_BOOT_INFO_VALID:
        return "BOOT_INFO_VALID";
    case ORION_INIT_CPU_STATE_VALID:
        return "CPU_STATE_VALID";
    case ORION_INIT_ARCH_TABLES_READY:
        return "ARCH_TABLES_READY";
    case ORION_INIT_CORE_READY:
        return "CORE_READY";
    case ORION_INIT_IDLE:
        return "IDLE";
    default:
        return "INVALID";
    }
}

static inline bool orion_init_advance(orion_init_stage *current,
                                      orion_init_stage expected,
                                      orion_init_stage next)
{
    if (current == 0 || !orion_init_stage_is_valid(*current) ||
        !orion_init_stage_is_valid(expected) ||
        !orion_init_stage_is_valid(next) || *current != expected ||
        expected >= ORION_INIT_IDLE || next != expected + 1) {
        return false;
    }
    *current = next;
    return true;
}

#endif
