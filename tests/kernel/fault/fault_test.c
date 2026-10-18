#include "../../../kernel/fault/report.h"
#include "../../../kernel/arch/x86_64/interrupt_state.h"

#include <stddef.h>

static bool interrupts_enabled = true;

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

static bool records_match(const orion_fault_record *left,
                          const orion_fault_record *right)
{
    return left->event_id == right->event_id &&
           left->instruction_pointer == right->instruction_pointer &&
           left->process_id == right->process_id &&
           left->thread_id == right->thread_id &&
           left->error_code == right->error_code &&
           left->address == right->address &&
           left->detail == right->detail &&
           left->vector == right->vector &&
           left->category == right->category &&
           left->context == right->context &&
           left->action == right->action &&
           left->recovery_result == right->recovery_result;
}

int main(void)
{
    if (orion_fault_report_count() != 0 ||
        orion_fault_report_overwritten_count() != 0) {
        return 1;
    }

    orion_fault_record input = {
        .instruction_pointer = UINT64_C(0x12345678),
        .process_id = 1,
        .thread_id = 7,
        .error_code = 14,
        .address = UINT64_C(0xfeed0000),
        .detail = UINT64_C(0x42),
        .vector = 14,
        .category = ORION_FAULT_CPU_EXCEPTION,
        .context = ORION_FAULT_CONTEXT_KERNEL_INTERRUPT,
        .action = ORION_FAULT_ACTION_HALT_KERNEL,
        .recovery_result = ORION_FAULT_RECOVERY_NOT_ATTEMPTED
    };
    uint64_t first_id = 0;
    if (orion_fault_report_record(&input, &first_id) != ORION_FAULT_STATUS_OK ||
        first_id != 1 || interrupts_enabled != true) {
        return 2;
    }

    orion_fault_record output = {0};
    orion_fault_record expected = input;
    expected.event_id = first_id;
    if (orion_fault_report_get(first_id, &output) != ORION_FAULT_STATUS_OK ||
        !records_match(&expected, &output)) {
        return 3;
    }
    if (orion_fault_report_get(0, &output) !=
            ORION_FAULT_STATUS_INVALID_ARGUMENT ||
        orion_fault_report_get(UINT64_MAX, &output) !=
            ORION_FAULT_STATUS_NOT_FOUND ||
        orion_fault_report_get(first_id, NULL) !=
            ORION_FAULT_STATUS_INVALID_ARGUMENT) {
        return 4;
    }

    const orion_fault_record invalid = {
        .category = (orion_fault_category)-1,
        .context = ORION_FAULT_CONTEXT_UNKNOWN,
        .action = ORION_FAULT_ACTION_NONE,
        .recovery_result = ORION_FAULT_RECOVERY_NOT_ATTEMPTED
    };
    if (orion_fault_report_record(&invalid, &first_id) !=
            ORION_FAULT_STATUS_INVALID_ARGUMENT ||
        orion_fault_report_record(&input, NULL) !=
            ORION_FAULT_STATUS_INVALID_ARGUMENT) {
        return 5;
    }

    uint64_t last_id = 0;
    for (uint32_t index = 0; index < ORION_FAULT_REPORT_CAPACITY + 3U;
         ++index) {
        input.detail = index;
        if (orion_fault_report_record(&input, &last_id) !=
            ORION_FAULT_STATUS_OK) {
            return 6;
        }
    }
    if (orion_fault_report_count() != ORION_FAULT_REPORT_CAPACITY ||
        orion_fault_report_overwritten_count() != 4 ||
        orion_fault_report_get(last_id, &output) != ORION_FAULT_STATUS_OK ||
        output.detail != ORION_FAULT_REPORT_CAPACITY + 2U ||
        orion_fault_report_get(1, &output) != ORION_FAULT_STATUS_NOT_FOUND ||
        !interrupts_enabled) {
        return 7;
    }
    return 0;
}
