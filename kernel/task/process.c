#include "process.h"

#include <stddef.h>

#include "../security/capability.h"
#include "thread.h"

static orion_process kernel_process;
static bool process_system_initialized;

bool orion_process_system_init(void)
{
    if (process_system_initialized || !orion_capability_system_init()) {
        return false;
    }
    kernel_process = (orion_process){
        .id = 1,
        .state = ORION_PROCESS_ACTIVE,
        .threads = NULL,
        .thread_count = 0,
        .address_space = NULL,
        .resources = NULL
    };
    if (!orion_capability_process_init(&kernel_process)) {
        kernel_process = (orion_process){0};
        return false;
    }
    process_system_initialized = true;
    return true;
}

orion_process *orion_process_kernel(void)
{
    return process_system_initialized ? &kernel_process : NULL;
}

bool orion_process_attach_thread(orion_process *process,
                                 struct orion_thread *thread)
{
    if (!process_system_initialized || process != &kernel_process ||
        process->state != ORION_PROCESS_ACTIVE || thread == NULL ||
        thread->process != NULL || process->thread_count == UINT32_MAX) {
        return false;
    }
    thread->process_next = process->threads;
    process->threads = thread;
    thread->process = process;
    ++process->thread_count;
    return true;
}

bool orion_process_detach_thread(orion_process *process,
                                 struct orion_thread *thread)
{
    if (!process_system_initialized || process != &kernel_process ||
        thread == NULL || thread->process != process ||
        process->thread_count == 0) {
        return false;
    }

    struct orion_thread **link = &process->threads;
    while (*link != NULL && *link != thread) {
        link = &(*link)->process_next;
    }
    if (*link != thread) {
        return false;
    }
    *link = thread->process_next;
    thread->process = NULL;
    thread->process_next = NULL;
    --process->thread_count;
    return true;
}

bool orion_process_check_invariants(const orion_process *process)
{
    if (!process_system_initialized || process != &kernel_process ||
        process->id != 1 || process->state != ORION_PROCESS_ACTIVE) {
        return false;
    }

    uint32_t count = 0;
    for (const struct orion_thread *thread = process->threads;
         thread != NULL; thread = thread->process_next) {
        if (thread->process != process ||
            ++count > ORION_THREAD_MAX_COUNT) {
            return false;
        }
    }
    return count == process->thread_count &&
           orion_capability_process_check(process);
}
