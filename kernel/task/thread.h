#ifndef ORION_KERNEL_TASK_THREAD_H
#define ORION_KERNEL_TASK_THREAD_H

#include <stdbool.h>
#include <stdint.h>

#include "process.h"

#define ORION_THREAD_MAX_COUNT 8U
#define ORION_THREAD_STACK_SIZE 16384U
#define ORION_THREAD_STACK_GUARD_SIZE 64U

typedef enum {
    ORION_THREAD_NEW,
    ORION_THREAD_READY,
    ORION_THREAD_RUNNING,
    ORION_THREAD_BLOCKED,
    ORION_THREAD_TERMINATED
} orion_thread_state;

typedef void (*orion_kernel_thread_entry)(void *argument);

typedef struct orion_thread {
    uint64_t id;
    orion_thread_state state;
    uint64_t *saved_stack_pointer;
    uintptr_t stack_bottom;
    uintptr_t stack_top;
    orion_kernel_thread_entry entry;
    void *argument;
    orion_process *process;
    struct orion_thread *process_next;
    struct orion_thread *ready_next;
    uint64_t dispatch_count;
    uint64_t yield_count;
    uint64_t runtime_ticks;
    uint64_t exit_code;
    bool bootstrap_thread;
    bool owns_stack;
} orion_thread;

bool orion_thread_system_init(orion_process *process,
                              uintptr_t bootstrap_stack_bottom,
                              uintptr_t bootstrap_stack_top,
                              orion_thread **bootstrap_thread);
bool orion_thread_create_kernel(orion_process *process,
                                orion_kernel_thread_entry entry,
                                void *argument, orion_thread **thread);
bool orion_thread_destroy(orion_thread *thread);
bool orion_thread_transition(orion_thread *thread,
                             orion_thread_state next_state);
bool orion_thread_is_managed(const orion_thread *thread);
bool orion_thread_stack_is_valid(const orion_thread *thread);
bool orion_thread_check_all_stacks(void);
void orion_thread_record_dispatch(orion_thread *thread);
bool orion_thread_check_invariants(void);
_Noreturn void orion_thread_start(orion_thread *thread);

#endif
