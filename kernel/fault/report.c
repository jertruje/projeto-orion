#include "report.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt_state.h"

static orion_fault_record reports[ORION_FAULT_REPORT_CAPACITY];
static uint32_t next_slot;
static uint32_t report_count;
static uint64_t next_event_id = 1;
static uint64_t overwritten_count;
static bool event_ids_exhausted;
static bool report_busy;

static bool record_is_valid(const orion_fault_record *record)
{
    return record != NULL &&
           (unsigned int)record->category <= ORION_FAULT_KERNEL_PANIC &&
           (unsigned int)record->context <= ORION_FAULT_CONTEXT_USER &&
           (unsigned int)record->action <= ORION_FAULT_ACTION_HALT_KERNEL &&
           (unsigned int)record->recovery_result <=
               ORION_FAULT_RECOVERY_NOT_SUPPORTED &&
           record->vector <= UINT8_MAX;
}

static void copy_record(orion_fault_record *destination,
                        const orion_fault_record *source,
                        uint64_t event_id)
{
    destination->event_id = event_id;
    destination->instruction_pointer = source->instruction_pointer;
    destination->process_id = source->process_id;
    destination->thread_id = source->thread_id;
    destination->error_code = source->error_code;
    destination->address = source->address;
    destination->detail = source->detail;
    destination->vector = source->vector;
    destination->category = source->category;
    destination->context = source->context;
    destination->action = source->action;
    destination->recovery_result = source->recovery_result;
}

orion_fault_status orion_fault_report_record(
    const orion_fault_record *record, uint64_t *event_id)
{
    if (!record_is_valid(record) || event_id == NULL) {
        return ORION_FAULT_STATUS_INVALID_ARGUMENT;
    }

    const bool interrupts_were_enabled =
        orion_x86_64_interrupts_save_disable();
    if (report_busy) {
        orion_x86_64_interrupts_restore(interrupts_were_enabled);
        return ORION_FAULT_STATUS_CORRUPTION;
    }
    report_busy = true;

    if (event_ids_exhausted) {
        report_busy = false;
        orion_x86_64_interrupts_restore(interrupts_were_enabled);
        return ORION_FAULT_STATUS_RESOURCE_EXHAUSTED;
    }

    const uint64_t assigned_id = next_event_id;
    if (next_event_id == UINT64_MAX) {
        event_ids_exhausted = true;
    } else {
        ++next_event_id;
    }

    copy_record(&reports[next_slot], record, assigned_id);
    next_slot = (next_slot + 1U) % ORION_FAULT_REPORT_CAPACITY;
    if (report_count < ORION_FAULT_REPORT_CAPACITY) {
        ++report_count;
    } else if (overwritten_count != UINT64_MAX) {
        ++overwritten_count;
    }
    *event_id = assigned_id;

    report_busy = false;
    orion_x86_64_interrupts_restore(interrupts_were_enabled);
    return ORION_FAULT_STATUS_OK;
}

orion_fault_status orion_fault_report_get(uint64_t event_id,
                                          orion_fault_record *record)
{
    if (event_id == 0 || record == NULL) {
        return ORION_FAULT_STATUS_INVALID_ARGUMENT;
    }

    const bool interrupts_were_enabled =
        orion_x86_64_interrupts_save_disable();
    if (report_busy) {
        orion_x86_64_interrupts_restore(interrupts_were_enabled);
        return ORION_FAULT_STATUS_CORRUPTION;
    }
    report_busy = true;

    for (uint32_t index = 0; index < ORION_FAULT_REPORT_CAPACITY; ++index) {
        if (reports[index].event_id == event_id) {
            copy_record(record, &reports[index], reports[index].event_id);
            report_busy = false;
            orion_x86_64_interrupts_restore(interrupts_were_enabled);
            return ORION_FAULT_STATUS_OK;
        }
    }

    report_busy = false;
    orion_x86_64_interrupts_restore(interrupts_were_enabled);
    return ORION_FAULT_STATUS_NOT_FOUND;
}

uint32_t orion_fault_report_count(void)
{
    const bool interrupts_were_enabled =
        orion_x86_64_interrupts_save_disable();
    const uint32_t count = report_count;
    orion_x86_64_interrupts_restore(interrupts_were_enabled);
    return count;
}

uint64_t orion_fault_report_overwritten_count(void)
{
    const bool interrupts_were_enabled =
        orion_x86_64_interrupts_save_disable();
    const uint64_t count = overwritten_count;
    orion_x86_64_interrupts_restore(interrupts_were_enabled);
    return count;
}
