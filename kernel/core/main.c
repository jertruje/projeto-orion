#include <orion/boot_info.h>
#include <orion/init_state.h>

#include "../arch/x86_64/cpu_state.h"
#include "../arch/x86_64/gdt.h"
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/interrupt_state.h"
#include "../arch/x86_64/timer.h"
#include "../mm/active_page_tables.h"
#include "../mm/memory_map.h"
#include "../mm/physical.h"
#include "../ipc/channel.h"
#include "../security/capability.h"
#include "../task/scheduler.h"
#include "log.h"
#include "panic.h"
#include "serial.h"

static orion_init_stage current_stage = ORION_INIT_ENTRY;
extern char kernel_stack_bottom[];
extern char kernel_stack_top[];

#define TASKING_TEST_ROUNDS 4U
#define TASKING_TEST_EVENTS (TASKING_TEST_ROUNDS * 2U)
#define PREEMPTION_TEST_ITERATIONS UINT64_C(1000000)
#define CONTEXT_BENCHMARK_ROUNDS 1000U

typedef struct {
    uint32_t sequence;
    uint32_t guard;
    uint8_t bytes[8];
} ipc_qemu_message;

typedef struct {
    uint64_t expected_id;
    uint32_t completed_steps;
    bool argument_verified;
    bool local_state_preserved;
    bool ipc_sender;
    orion_capability_handle ipc_endpoint;
} tasking_worker_state;

static tasking_worker_state tasking_workers[2];
static uint64_t tasking_event_ids[TASKING_TEST_EVENTS];
static uint64_t tasking_event_steps[TASKING_TEST_EVENTS];
static uint32_t tasking_event_count;
static volatile uint64_t preemption_progress[2];
static volatile uint32_t context_benchmark_turn;
static uint32_t context_benchmark_steps[2];
static uint64_t capability_self_test_object;
static bool capability_self_test_destroyed;
static orion_capability_handle ipc_qemu_sender_endpoint;
static orion_capability_handle ipc_qemu_receiver_endpoint;
static volatile uint32_t ipc_qemu_sent;
static volatile uint32_t ipc_qemu_received;

static void capability_self_test_destructor(void *object)
{
    if (object != &capability_self_test_object) {
        orion_scheduler_fatal("capability object destructor mismatch");
    }
    capability_self_test_destroyed = true;
}

static uint64_t read_timestamp_counter(void)
{
    uint32_t low;
    uint32_t high;
    __asm__ volatile("lfence; rdtsc; lfence"
                     : "=a"(low), "=d"(high)
                     :
                     : "memory");
    return ((uint64_t)high << 32) | low;
}

static void tasking_test_worker(void *argument)
{
    tasking_worker_state *worker = argument;
    const orion_thread *thread = orion_scheduler_current();
    if (worker == NULL || thread == NULL ||
        thread->id != worker->expected_id) {
        orion_scheduler_fatal("kernel-thread argument or identity mismatch");
    }

    volatile uint64_t local_marker =
        UINT64_C(0x4f52494f4e544852) ^ thread->id;
    worker->argument_verified = true;
    worker->local_state_preserved = true;
    for (uint32_t step = 0; step < TASKING_TEST_ROUNDS; ++step) {
        if (tasking_event_count >= TASKING_TEST_EVENTS ||
            !orion_log_u64(ORION_LOG_INFO, "TASK", "thread run id:",
                           thread->id) ||
            !orion_log_u64(ORION_LOG_INFO, "TASK", "thread run iteration:",
                           step)) {
            orion_scheduler_fatal("kernel-thread QEMU trace failed");
        }
        tasking_event_ids[tasking_event_count] = thread->id;
        tasking_event_steps[tasking_event_count] = step;
        ++tasking_event_count;
        ++worker->completed_steps;
        if (worker->ipc_sender) {
            ipc_qemu_message message = {
                .sequence = step,
                .guard = UINT32_C(0x49504331) ^ step
            };
            for (uint32_t index = 0; index < sizeof(message.bytes); ++index) {
                message.bytes[index] = (uint8_t)(step * 13U + index);
            }
            if (orion_ipc_send(worker->ipc_endpoint, &message,
                               sizeof(message)) != ORION_IPC_OK) {
                orion_scheduler_fatal("QEMU IPC send failed");
            }
            ++ipc_qemu_sent;
        } else {
            ipc_qemu_message message = {0};
            uint32_t received_size = 0;
            if (orion_ipc_receive(worker->ipc_endpoint, &message,
                                  sizeof(message), &received_size) !=
                    ORION_IPC_OK ||
                received_size != sizeof(message) ||
                message.sequence != step ||
                message.guard != (UINT32_C(0x49504331) ^ step)) {
                orion_scheduler_fatal("QEMU IPC receive validation failed");
            }
            for (uint32_t index = 0; index < sizeof(message.bytes); ++index) {
                if (message.bytes[index] != (uint8_t)(step * 13U + index)) {
                    orion_scheduler_fatal("QEMU IPC payload was corrupted");
                }
            }
            ++ipc_qemu_received;
        }
        if (!orion_scheduler_yield()) {
            orion_scheduler_fatal("cooperative thread yield failed");
        }
        if (local_marker != (UINT64_C(0x4f52494f4e544852) ^ thread->id)) {
            worker->local_state_preserved = false;
            orion_scheduler_fatal("kernel-thread local state was corrupted");
        }
    }
}

static bool tasking_self_test(void)
{
    if (!orion_scheduler_init((uintptr_t)kernel_stack_bottom,
                              (uintptr_t)kernel_stack_top) ||
        !orion_ipc_system_init() ||
        !orion_scheduler_check_invariants()) {
        return false;
    }

    orion_thread *workers[2] = {NULL, NULL};
    ipc_qemu_sent = 0;
    ipc_qemu_received = 0;
    if (orion_ipc_channel_create(&ipc_qemu_sender_endpoint,
                                 &ipc_qemu_receiver_endpoint) != ORION_IPC_OK) {
        return false;
    }
    for (uint32_t index = 0; index < 2; ++index) {
        tasking_workers[index] = (tasking_worker_state){0};
    }
    tasking_workers[0].ipc_sender = true;
    tasking_workers[0].ipc_endpoint = ipc_qemu_sender_endpoint;
    tasking_workers[1].ipc_endpoint = ipc_qemu_receiver_endpoint;
    tasking_event_count = 0;
    if (!orion_scheduler_create_kernel_thread(
            tasking_test_worker, &tasking_workers[0], &workers[0]) ||
        !orion_scheduler_create_kernel_thread(
            tasking_test_worker, &tasking_workers[1], &workers[1])) {
        return false;
    }
    tasking_workers[0].expected_id = workers[0]->id;
    tasking_workers[1].expected_id = workers[1]->id;

    orion_thread *bootstrap = orion_scheduler_current();
    if (bootstrap == NULL || bootstrap->id != 1 ||
        !bootstrap->bootstrap_thread ||
        bootstrap->stack_bottom != (uintptr_t)kernel_stack_bottom ||
        bootstrap->stack_top != (uintptr_t)kernel_stack_top ||
        orion_scheduler_destroy_thread(bootstrap) ||
        orion_thread_transition(workers[0], ORION_THREAD_BLOCKED) ||
        workers[0]->state != ORION_THREAD_READY ||
        orion_scheduler_destroy_thread(workers[0]) ||
        !orion_scheduler_run_ready()) {
        return false;
    }

    for (uint32_t event = 0; event < TASKING_TEST_EVENTS; ++event) {
        const uint64_t expected_id =
            (event & 1U) == 0 ? workers[0]->id : workers[1]->id;
        const uint64_t expected_step = event / 2U;
        if (tasking_event_ids[event] != expected_id ||
            tasking_event_steps[event] != expected_step) {
            return false;
        }
    }
    for (uint32_t index = 0; index < 2; ++index) {
        if (workers[index]->state != ORION_THREAD_TERMINATED ||
            workers[index]->exit_code != 0 ||
            workers[index]->dispatch_count != TASKING_TEST_ROUNDS + 1U ||
            workers[index]->yield_count != TASKING_TEST_ROUNDS ||
            tasking_workers[index].completed_steps != TASKING_TEST_ROUNDS ||
            !tasking_workers[index].argument_verified ||
            !tasking_workers[index].local_state_preserved ||
            !orion_thread_stack_is_valid(workers[index])) {
            return false;
        }
    }

    orion_scheduler_stats stats;
    if (!orion_scheduler_get_stats(&stats) ||
        stats.context_switches < TASKING_TEST_EVENTS ||
        stats.yield_calls != TASKING_TEST_EVENTS ||
        stats.ready_threads != 0 || stats.managed_threads != 3 ||
        !orion_scheduler_yield() ||
        !orion_scheduler_check_invariants() ||
        !orion_scheduler_destroy_thread(workers[0]) ||
        !orion_scheduler_destroy_thread(workers[1]) ||
        !orion_scheduler_get_stats(&stats) ||
        stats.managed_threads != 1 || stats.ready_threads != 0 ||
        !orion_thread_check_invariants() ||
        ipc_qemu_sent != TASKING_TEST_ROUNDS ||
        ipc_qemu_received != TASKING_TEST_ROUNDS ||
        orion_ipc_close(ipc_qemu_sender_endpoint) != ORION_IPC_OK ||
        orion_ipc_close(ipc_qemu_receiver_endpoint) != ORION_IPC_OK ||
        !orion_ipc_check_invariants()) {
        return false;
    }
    return orion_log(ORION_LOG_INFO, "TASK",
                     "Cooperative round-robin alternation passed") &&
           orion_log(ORION_LOG_INFO, "IPC",
                     "Kernel-thread channel message exchange passed");
}

static void preemption_test_worker(void *argument)
{
    const uintptr_t index = (uintptr_t)argument;
    if (index >= 2U) {
        orion_scheduler_fatal("invalid preemption test worker index");
    }
    const uintptr_t peer = index ^ 1U;
    volatile uint64_t stack_marker =
        UINT64_C(0x505245454d505400) | (uint64_t)index;
    while (preemption_progress[index] < PREEMPTION_TEST_ITERATIONS ||
           preemption_progress[peer] < PREEMPTION_TEST_ITERATIONS) {
        ++preemption_progress[index];
    }
    if (stack_marker !=
        (UINT64_C(0x505245454d505400) | (uint64_t)index)) {
        orion_scheduler_fatal("preempted worker stack state was corrupted");
    }
}

static bool wait_for_timer_ticks(uint64_t additional_ticks)
{
    orion_x86_64_timer_stats stats;
    if (!orion_x86_64_timer_get_stats(&stats) ||
        additional_ticks > UINT64_MAX - stats.ticks) {
        return false;
    }
    const uint64_t target_tick = stats.ticks + additional_ticks;
    do {
        __asm__ volatile("sti; hlt; cli" : : : "memory");
        if (!orion_x86_64_timer_get_stats(&stats)) {
            return false;
        }
    } while (stats.ticks < target_tick);
    return true;
}

static bool preemption_self_test(void)
{
    preemption_progress[0] = 0;
    preemption_progress[1] = 0;
    if (!orion_x86_64_timer_init()) {
        return false;
    }

    orion_thread *workers[2] = {NULL, NULL};
    if (!orion_scheduler_create_kernel_thread(
            preemption_test_worker, (void *)(uintptr_t)0, &workers[0]) ||
        !orion_scheduler_create_kernel_thread(
            preemption_test_worker, (void *)(uintptr_t)1, &workers[1]) ||
        !orion_scheduler_run_ready() || !wait_for_timer_ticks(4)) {
        return false;
    }

    if (!orion_x86_64_timer_stop()) {
        return false;
    }
    orion_scheduler_stats scheduler_stats;
    orion_x86_64_timer_stats timer_stats;
    if (!orion_scheduler_get_stats(&scheduler_stats) ||
        !orion_x86_64_timer_get_stats(&timer_stats) ||
        scheduler_stats.timer_interrupts == 0 ||
    scheduler_stats.timer_interrupts != timer_stats.ticks ||
    scheduler_stats.timer_preemptions == 0 ||
    timer_stats.handler_samples < 4 ||
    timer_stats.handler_tsc_max == 0 ||
        preemption_progress[0] < PREEMPTION_TEST_ITERATIONS ||
        preemption_progress[1] < PREEMPTION_TEST_ITERATIONS ||
        workers[0]->state != ORION_THREAD_TERMINATED ||
        workers[1]->state != ORION_THREAD_TERMINATED ||
        workers[0]->yield_count != 0 || workers[1]->yield_count != 0 ||
        workers[0]->runtime_ticks + workers[1]->runtime_ticks == 0 ||
        !orion_thread_stack_is_valid(workers[0]) ||
        !orion_thread_stack_is_valid(workers[1])) {
        return false;
    }
    if (!orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "PIT timer interrupts:", timer_stats.ticks) ||
        !orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "Timer-driven preemptions:",
                       scheduler_stats.timer_preemptions) ||
        !orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "No-switch timer handler samples:",
                       timer_stats.handler_samples) ||
        !orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "Mean timer handler TSC cycles:",
                       timer_stats.handler_tsc_total /
                           timer_stats.handler_samples) ||
        !orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "Maximum timer handler TSC cycles:",
                       timer_stats.handler_tsc_max) ||
        !orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "CPU-bound worker 0 iterations:",
                       preemption_progress[0]) ||
        !orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "CPU-bound worker 1 iterations:",
                       preemption_progress[1]) ||
        !orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "Cooperative yields in preemption test:",
                       workers[0]->yield_count + workers[1]->yield_count) ||
        !orion_log(ORION_LOG_INFO, "SCHED",
                   "Timer preemption test passed")) {
        return false;
    }

    return orion_scheduler_destroy_thread(workers[0]) &&
           orion_scheduler_destroy_thread(workers[1]) &&
           orion_scheduler_check_invariants();
}

static void context_benchmark_worker(void *argument)
{
    const uintptr_t index = (uintptr_t)argument;
    if (index >= 2U) {
        orion_scheduler_fatal("invalid context benchmark worker index");
    }
    volatile uint64_t stack_marker =
        UINT64_C(0x4354585357495443) ^ (uint64_t)index;
    for (uint32_t round = 0; round < CONTEXT_BENCHMARK_ROUNDS; ++round) {
        while (context_benchmark_turn != index) {
        }
        context_benchmark_turn = (uint32_t)(index ^ 1U);
        ++context_benchmark_steps[index];
        if (!orion_scheduler_yield() ||
            stack_marker !=
                (UINT64_C(0x4354585357495443) ^ (uint64_t)index)) {
            orion_scheduler_fatal("context benchmark handoff failed");
        }
    }
}

static bool context_switch_benchmark(void)
{
    context_benchmark_turn = 0;
    context_benchmark_steps[0] = 0;
    context_benchmark_steps[1] = 0;
    orion_thread *workers[2] = {NULL, NULL};
    if (!orion_scheduler_create_kernel_thread(
            context_benchmark_worker, (void *)(uintptr_t)0, &workers[0]) ||
        !orion_scheduler_create_kernel_thread(
            context_benchmark_worker, (void *)(uintptr_t)1, &workers[1])) {
        return false;
    }

    orion_scheduler_stats before;
    if (!orion_scheduler_get_stats(&before)) {
        return false;
    }
    const uint64_t start_tsc = read_timestamp_counter();
    if (!orion_scheduler_run_ready()) {
        return false;
    }
    const uint64_t elapsed_tsc = read_timestamp_counter() - start_tsc;

    orion_scheduler_stats after;
    if (!orion_scheduler_get_stats(&after) ||
        after.context_switches <= before.context_switches ||
        after.yield_calls - before.yield_calls !=
            (uint64_t)CONTEXT_BENCHMARK_ROUNDS * 2U ||
        elapsed_tsc == 0 ||
        context_benchmark_steps[0] != CONTEXT_BENCHMARK_ROUNDS ||
        context_benchmark_steps[1] != CONTEXT_BENCHMARK_ROUNDS ||
        workers[0]->state != ORION_THREAD_TERMINATED ||
        workers[1]->state != ORION_THREAD_TERMINATED ||
        !orion_thread_stack_is_valid(workers[0]) ||
        !orion_thread_stack_is_valid(workers[1])) {
        return false;
    }
    const uint64_t switch_count =
        after.context_switches - before.context_switches;
    if (!orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "Context benchmark switches:", switch_count) ||
        !orion_log_u64(ORION_LOG_INFO, "SCHED",
                       "Mean scheduler handoff TSC cycles:",
                       elapsed_tsc / switch_count)) {
        return false;
    }
    return orion_scheduler_destroy_thread(workers[0]) &&
           orion_scheduler_destroy_thread(workers[1]) &&
           orion_scheduler_check_invariants();
}

static bool capability_self_test(void)
{
    orion_process *kernel_process = orion_process_kernel();
    orion_capability_handle root = 0;
    orion_capability_handle derived = 0;
    orion_capability_lease lease = {0};
    orion_capability_info info;
    capability_self_test_destroyed = false;

    const uint32_t rights =
        ORION_RIGHT_QUERY | ORION_RIGHT_USE | ORION_RIGHT_DERIVE |
        ORION_RIGHT_REVOKE;
    if (kernel_process == NULL ||
        orion_capability_create_kernel_object(
            kernel_process, ORION_OBJECT_MEMORY_REGION,
            &capability_self_test_object, rights,
            capability_self_test_destructor, &root) != ORION_CAPABILITY_OK ||
        orion_capability_acquire(kernel_process, root,
                                 ORION_OBJECT_MEMORY_REGION, ORION_RIGHT_USE,
                                 &lease) != ORION_CAPABILITY_OK ||
        lease.object != &capability_self_test_object ||
        orion_capability_acquire(kernel_process, root, ORION_OBJECT_DEVICE,
                                 ORION_RIGHT_USE, &lease) !=
            ORION_CAPABILITY_WRONG_TYPE ||
        orion_capability_acquire(kernel_process, root,
                                 ORION_OBJECT_MEMORY_REGION,
                                 ORION_RIGHT_MODIFY, &lease) !=
            ORION_CAPABILITY_RIGHTS_DENIED ||
        orion_capability_derive(kernel_process, root, ORION_RIGHT_QUERY,
                                &derived) != ORION_CAPABILITY_OK ||
        orion_capability_describe(kernel_process, derived, &info) !=
            ORION_CAPABILITY_OK ||
        info.type != ORION_OBJECT_MEMORY_REGION ||
        info.rights != ORION_RIGHT_QUERY ||
        orion_capability_revoke(kernel_process, root) !=
            ORION_CAPABILITY_OK ||
        capability_self_test_destroyed ||
        orion_capability_describe(kernel_process, derived, &info) !=
            ORION_CAPABILITY_INVALID_HANDLE ||
        orion_capability_lease_release(kernel_process, &lease) !=
            ORION_CAPABILITY_OK ||
        !capability_self_test_destroyed ||
        !orion_capability_check_invariants()) {
        return false;
    }

    return orion_log(ORION_LOG_INFO, "SECURITY",
                     "Capability checks and revocation passed");
}

static void advance_stage(orion_init_stage expected, orion_init_stage next)
{
    if (!orion_init_advance(&current_stage, expected, next)) {
        orion_panic_at("INIT", "invalid initialization stage transition");
    }
    if (!orion_log(ORION_LOG_INFO, "INIT", orion_init_stage_name(next))) {
        orion_panic_at("INIT", "failed to log initialization stage");
    }
}

void kernel_main(const orion_boot_info *boot_info)
{
    orion_serial_init();
    if (!orion_log(ORION_LOG_INFO, "BOOT", "Kernel entry reached")) {
        orion_panic_at("BOOT", "serial logging failed at kernel entry");
    }
    advance_stage(ORION_INIT_ENTRY, ORION_INIT_DIAGNOSTICS_CONFIGURED);

    if (!orion_boot_info_is_valid(boot_info)) {
        orion_panic_at("BOOT", "invalid UEFI boot information");
    }
    advance_stage(ORION_INIT_DIAGNOSTICS_CONFIGURED,
                  ORION_INIT_BOOT_INFO_VALID);
    if (!orion_log(ORION_LOG_INFO, "CORE", "Boot information validated")) {
        orion_panic_at("CORE", "serial logging failed after boot-info validation");
    }

    if (!orion_x86_64_validate_entry_state((uintptr_t)kernel_stack_bottom,
                                           (uintptr_t)kernel_stack_top)) {
        orion_panic_at("CPU", "unsupported CPU or kernel entry state");
    }
    const bool nx_enabled = orion_x86_64_nx_enabled();
    advance_stage(ORION_INIT_BOOT_INFO_VALID, ORION_INIT_CPU_STATE_VALID);
    if (!orion_log_u64(ORION_LOG_INFO, "CPU",
                       "NX feature and EFER.NXE enabled:", nx_enabled)) {
        orion_panic_at("CPU", "failed to log NX capability");
    }

    if (!orion_x86_64_gdt_init()) {
        orion_panic_at("GDT", "failed to configure kernel GDT and TSS");
    }
    if (!orion_x86_64_idt_init()) {
        orion_panic_at("IDT", "failed to configure kernel exception IDT");
    }
    if (orion_x86_64_interrupts_enabled()) {
        orion_panic_at("CPU", "maskable interrupts unexpectedly enabled");
    }
    advance_stage(ORION_INIT_CPU_STATE_VALID, ORION_INIT_ARCH_TABLES_READY);
    if (!orion_log(ORION_LOG_INFO, "CPU",
                   "Kernel GDT, TSS and CPU exception IDT installed; IRQs disabled")) {
        orion_panic_at("CPU", "serial logging failed after descriptor setup");
    }

    orion_memory_map_summary memory_summary;
    if (!orion_memory_map_validate(boot_info, &memory_summary)) {
        orion_panic_at("MEMORY", "invalid or unsupported UEFI memory map");
    }
    if (!orion_physical_init(boot_info)) {
        orion_panic_at("MEMORY", "physical page allocator initialization failed");
    }
    orion_physical_stats physical_stats;
    if (!orion_physical_get_stats(&physical_stats) ||
        !orion_physical_check_invariants()) {
        orion_panic_at("MEMORY", "physical page allocator invariant failure");
    }
    uint64_t probe_page = 0;
    if (!orion_physical_allocate(1, &probe_page) ||
        (probe_page & (ORION_MEMORY_PAGE_SIZE - 1U)) != 0 ||
        !orion_physical_release(probe_page) ||
        !orion_physical_check_invariants()) {
        orion_panic_at("MEMORY", "physical allocation smoke test failed");
    }
    if (!orion_log(ORION_LOG_INFO, "MEMORY",
                   "Physical allocate/release smoke test passed")) {
        orion_panic_at("MEMORY", "failed to log physical allocator test");
    }
    bool page_table_test_performed = false;
    if (!orion_x86_64_active_page_tables_smoke_test(
            boot_info, nx_enabled, &page_table_test_performed)) {
        orion_panic_at("MEMORY", "active page-table smoke test failed");
    }
    if (page_table_test_performed &&
        !orion_log(ORION_LOG_INFO, "MEMORY",
                   "Active CR3 map/write/lookup/unmap smoke test passed")) {
        orion_panic_at("MEMORY", "failed to log active page-table test");
    }
    if (!page_table_test_performed &&
        !orion_log(ORION_LOG_WARN, "MEMORY",
                   "Active writable non-executable map test skipped: NX unavailable")) {
        orion_panic_at("MEMORY", "failed to log skipped page-table test");
    }
    if (!orion_log_u64(ORION_LOG_INFO, "MEMORY",
                       "Validated UEFI memory descriptors:",
                       memory_summary.descriptor_count) ||
        !orion_log_u64(ORION_LOG_INFO, "MEMORY",
                       "Free physical pages below 4 GiB:",
                       physical_stats.free_pages)) {
        orion_panic_at("MEMORY", "failed to log memory initialization");
    }

    advance_stage(ORION_INIT_ARCH_TABLES_READY, ORION_INIT_CORE_READY);
    if (!orion_log_u64(ORION_LOG_INFO, "CORE", "Boot memory map bytes:",
                       boot_info->memory_map_size)) {
        orion_panic_at("CORE", "serial logging failed while reporting memory map");
    }
    if (!orion_log(ORION_LOG_INFO, "CORE",
                   "UEFI boot services have exited; interrupts remain disabled")) {
        orion_panic_at("CORE", "serial logging failed after core initialization");
    }

    if (!tasking_self_test()) {
        orion_panic_at("TASK", "cooperative tasking self-test failed");
    }
    if (!capability_self_test()) {
        orion_panic_at("SECURITY", "capability self-test failed");
    }
    if (!preemption_self_test()) {
        orion_panic_at("SCHED", "single-core timer preemption test failed");
    }
    if (!context_switch_benchmark()) {
        orion_panic_at("SCHED", "context-switch benchmark failed");
    }

    advance_stage(ORION_INIT_CORE_READY, ORION_INIT_IDLE);
    if (!orion_log(ORION_LOG_INFO, "CORE",
                   "Minimal kernel is running; entering idle loop")) {
        orion_panic_at("CORE", "serial logging failed before idle loop");
    }

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}