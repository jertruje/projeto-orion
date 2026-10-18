#ifndef ORION_KERNEL_TASK_PROCESS_H
#define ORION_KERNEL_TASK_PROCESS_H

#include <stdbool.h>
#include <stdint.h>

struct orion_thread;

typedef enum {
    ORION_PROCESS_ACTIVE,
    ORION_PROCESS_TERMINATED
} orion_process_state;

typedef struct orion_process {
    uint64_t id;
    orion_process_state state;
    struct orion_thread *threads;
    uint32_t thread_count;
    void *address_space;
    void *resources;
} orion_process;

bool orion_process_system_init(void);
orion_process *orion_process_kernel(void);
bool orion_process_attach_thread(orion_process *process,
                                 struct orion_thread *thread);
bool orion_process_detach_thread(orion_process *process,
                                 struct orion_thread *thread);
bool orion_process_check_invariants(const orion_process *process);

#endif
