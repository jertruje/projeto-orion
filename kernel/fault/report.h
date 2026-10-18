#ifndef ORION_KERNEL_FAULT_REPORT_H
#define ORION_KERNEL_FAULT_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_FAULT_REPORT_CAPACITY 32U

typedef enum {
    ORION_FAULT_OPERATION_ERROR,
    ORION_FAULT_INVALID_INPUT,
    ORION_FAULT_THREAD_EXIT,
    ORION_FAULT_USER_PROCESS_FAILURE,
    ORION_FAULT_IPC_FAILURE,
    ORION_FAULT_RESOURCE_EXHAUSTION,
    ORION_FAULT_CPU_EXCEPTION,
    ORION_FAULT_DRIVER_FAILURE,
    ORION_FAULT_KERNEL_CORRUPTION,
    ORION_FAULT_KERNEL_PANIC
} orion_fault_category;

typedef enum {
    ORION_FAULT_CONTEXT_UNKNOWN,
    ORION_FAULT_CONTEXT_BOOT,
    ORION_FAULT_CONTEXT_KERNEL_THREAD,
    ORION_FAULT_CONTEXT_KERNEL_INTERRUPT,
    ORION_FAULT_CONTEXT_USER
} orion_fault_context;

typedef enum {
    ORION_FAULT_ACTION_NONE,
    ORION_FAULT_ACTION_CONTINUE,
    ORION_FAULT_ACTION_REJECT_OPERATION,
    ORION_FAULT_ACTION_TERMINATE_THREAD,
    ORION_FAULT_ACTION_TERMINATE_PROCESS,
    ORION_FAULT_ACTION_RESTART_SERVICE,
    ORION_FAULT_ACTION_HALT_KERNEL
} orion_fault_action;

typedef enum {
    ORION_FAULT_RECOVERY_NOT_ATTEMPTED,
    ORION_FAULT_RECOVERY_SUCCEEDED,
    ORION_FAULT_RECOVERY_FAILED,
    ORION_FAULT_RECOVERY_NOT_SUPPORTED
} orion_fault_recovery_result;

typedef enum {
    ORION_FAULT_STATUS_OK,
    ORION_FAULT_STATUS_INVALID_ARGUMENT,
    ORION_FAULT_STATUS_NOT_FOUND,
    ORION_FAULT_STATUS_RESOURCE_EXHAUSTED,
    ORION_FAULT_STATUS_CORRUPTION
} orion_fault_status;

typedef struct {
    uint64_t event_id;
    uint64_t instruction_pointer;
    uint64_t process_id;
    uint64_t thread_id;
    uint64_t error_code;
    uint64_t address;
    uint64_t detail;
    uint32_t vector;
    orion_fault_category category;
    orion_fault_context context;
    orion_fault_action action;
    orion_fault_recovery_result recovery_result;
} orion_fault_record;

orion_fault_status orion_fault_report_record(
    const orion_fault_record *record, uint64_t *event_id);
orion_fault_status orion_fault_report_get(uint64_t event_id,
                                          orion_fault_record *record);
uint32_t orion_fault_report_count(void);
uint64_t orion_fault_report_overwritten_count(void);

#endif
