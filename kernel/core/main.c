#include <orion/boot_info.h>
#include <orion/init_state.h>

#include "../arch/x86_64/cpu_state.h"
#include "../arch/x86_64/gdt.h"
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/interrupt_state.h"
#include "../arch/x86_64/timer.h"
#include "../drivers/bus/pci.h"
#include "../drivers/core/driver.h"
#include "../drivers/platform/uart16550.h"
#include "../drivers/net/ne2k_pci.h"
#include "../drivers/storage/ata_pio.h"
#include "../exec/user_program.h"
#include "../fs/orionfs.h"
#include "../power/acpi.h"
#include "../power/acpi.h"
#include "../power/acpi.h"
#include "../mm/active_page_tables.h"
#include "../mm/memory_map.h"
#include "../mm/physical.h"
#include "../ipc/channel.h"
#include "../security/capability.h"
#include "../task/scheduler.h"
#include "../time/clocksource.h"
#include "../time/timer.h"
#include "../net/network.h"
#include "../vfs/vfs.h"
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
static volatile uint32_t timer_qemu_callback_count;

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

static bool timer_qemu_callback(orion_timer_handle handle, void *context)
{
    (void)handle;
    if (context != (void *)&timer_qemu_callback_count ||
        timer_qemu_callback_count == UINT32_MAX) {
        return false;
    }
    ++timer_qemu_callback_count;
    return true;
}

static bool timekeeping_self_test(void)
{
    if (!orion_x86_64_timer_start_timekeeping() ||
        !orion_clocksource_init_pit() || !orion_timer_system_init()) {
        return false;
    }

    uint64_t first_read = 0;
    uint64_t second_read = 0;
    if (!orion_clocksource_now_nanoseconds(&first_read) ||
        !orion_clocksource_now_nanoseconds(&second_read) ||
        second_read < first_read) {
        return false;
    }

    timer_qemu_callback_count = 0;
    orion_timer_handle handle;
    if (orion_timer_arm_after(UINT64_C(30000000), 0,
                              timer_qemu_callback,
                              (void *)&timer_qemu_callback_count,
                              &handle) != ORION_TIMER_OK) {
        return false;
    }

    uint64_t start = 0;
    if (!orion_clocksource_now_nanoseconds(&start) ||
        start > UINT64_MAX - UINT64_C(1000000000)) {
        return false;
    }
    const uint64_t timeout = start + UINT64_C(1000000000);
    while (timer_qemu_callback_count == 0) {
        __asm__ volatile("sti; hlt" : : : "memory");
        uint64_t now = 0;
        uint32_t callbacks_run = 0;
        if (!orion_clocksource_now_nanoseconds(&now) ||
            now >= timeout ||
            !orion_timer_run_expired(ORION_TIMER_CAPACITY,
                                     &callbacks_run)) {
            return false;
        }
    }

    orion_clocksource_stats clock_stats;
    orion_timer_stats timer_stats;
    orion_x86_64_timer_stats hardware_timer_stats;
    uint64_t pit_quantum_nanoseconds = 0;
    const bool interrupts_were_enabled =
        orion_x86_64_interrupts_save_disable();
    const bool hardware_stats_valid =
        orion_x86_64_timer_get_stats(&hardware_timer_stats);
    orion_x86_64_interrupts_restore(interrupts_were_enabled);
    if (timer_qemu_callback_count != 1 ||
        !hardware_stats_valid ||
        !orion_clocksource_get_stats(&clock_stats) ||
        !orion_timer_get_stats(&timer_stats) ||
        !orion_timer_check_invariants() || timer_stats.active != 0 ||
        !orion_acpi_check_invariants() ||
        timer_stats.expirations != 1 ||
        !orion_clocksource_ticks_to_nanoseconds(
            1, ORION_X86_64_PIT_DIVISOR, ORION_X86_64_PIT_INPUT_HZ,
            &pit_quantum_nanoseconds) ||
        !orion_log_u64(ORION_LOG_INFO, "TIME",
                       "PIT IRQ0 ticks in timekeeping test:",
                       hardware_timer_stats.ticks) ||
        !orion_log_u64(ORION_LOG_INFO, "TIME",
                       "Clocksource reads in timekeeping test:",
                       clock_stats.reads) ||
        !orion_log_u64(ORION_LOG_INFO, "TIME",
                       "PIT nominal quantum ns:", pit_quantum_nanoseconds) ||
        !orion_log_u64(ORION_LOG_INFO, "TIME",
                       "One-shot timer maximum lateness ns:",
                       timer_stats.max_lateness_nanoseconds)) {
        return false;
    }
    return orion_log(ORION_LOG_INFO, "TIME",
                     "Hardware-backed PIT deadline timer test passed");
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

static bool pci_device_discovered(void *context,
                                 const orion_pci_device *pci_device)
{
    (void)context;
    orion_device device = {
        .bus = ORION_DEVICE_PCI,
        .bus_number = pci_device->bus,
        .device_number = pci_device->device,
        .function_number = pci_device->function,
        .vendor_id = pci_device->vendor_id,
        .device_id = pci_device->device_id,
        .class_code = pci_device->class_code,
        .subclass = pci_device->subclass,
        .programming_interface = pci_device->programming_interface,
        .revision = pci_device->revision,
        .bar_count = pci_device->bar_count
    };
    for (uint32_t index = 0; index < pci_device->bar_count; ++index) {
        device.bars[index] = pci_device->bars[index];
    }
    if (!orion_driver_add_device(&device)) {
        return false;
    }
    return orion_log_u64(ORION_LOG_INFO, "PCI",
                         "Device vendor:device",
                         ((uint64_t)pci_device->vendor_id << 16) |
                             pci_device->device_id) &&
           orion_log_u64(ORION_LOG_INFO, "PCI",
                         "Device bus:device:function",
                         ((uint64_t)pci_device->bus << 16) |
                             ((uint64_t)pci_device->device << 8) |
                             pci_device->function);
}

#if defined(ORION_STORAGE_QEMU_TEST)
static bool storage_filesystem_self_test(orion_block_device *device)
{
    static const char path[] = "/phase16-persistence-proof";
    static const char payload[] = "ORION_PHASE16_PERSISTENCE_V1";
    if (orion_vfs_mount(device) != ORIONFS_OK) {
        return false;
    }

    orion_vfs_info info;
    orion_vfs_status status = orion_vfs_stat(path, &info);
    if (status == ORIONFS_NOT_FOUND) {
        if (orion_vfs_create(path, ORIONFS_NODE_FILE) != ORIONFS_OK) {
            return false;
        }
        orion_vfs_handle handle;
        uint64_t bytes_written = 0;
        if (orion_vfs_open(path, &handle) != ORIONFS_OK ||
            orion_vfs_write(handle, 0, payload, sizeof(payload) - 1U,
                            &bytes_written) != ORIONFS_OK ||
            bytes_written != sizeof(payload) - 1U ||
            orion_vfs_close(handle) != ORIONFS_OK ||
            orion_block_sync(device) != ORION_BLOCK_OK ||
            orion_vfs_unmount() != ORIONFS_OK) {
            return false;
        }
        return orion_log(ORION_LOG_INFO, "STORAGE",
                         "Persistence proof committed; reboot to verify");
    }
    if (status != ORIONFS_OK || info.type != ORIONFS_NODE_FILE ||
        info.size != sizeof(payload) - 1U) {
        return false;
    }

    uint8_t actual[sizeof(payload)] = {0};
    uint64_t bytes_read = 0;
    orion_vfs_handle handle;
    if (orion_vfs_open(path, &handle) != ORIONFS_OK ||
        orion_vfs_read(handle, 0, actual, sizeof(payload) - 1U,
                       &bytes_read) != ORIONFS_OK ||
        bytes_read != sizeof(payload) - 1U ||
        orion_vfs_close(handle) != ORIONFS_OK ||
        orion_vfs_unmount() != ORIONFS_OK) {
        return false;
    }
    for (uint32_t index = 0; index < sizeof(payload) - 1U; ++index) {
        if (actual[index] != (uint8_t)payload[index]) {
            return false;
        }
    }
    return orion_log(ORION_LOG_INFO, "STORAGE",
                     "Persistence proof recovered across QEMU boots");
}
#endif

#if defined(ORION_NETWORK_QEMU_TEST)
static bool network_clock_read(void *context, uint64_t *now_nanoseconds)
{
    (void)context;
    return orion_clocksource_now_nanoseconds(now_nanoseconds);
}

static void network_test_udp_echo(void *context, uint32_t source_ipv4,
                                  uint16_t source_port,
                                  const uint8_t *payload,
                                  uint16_t payload_bytes)
{
    static bool stats_reported;
    (void)context;
    (void)orion_log(ORION_LOG_INFO, "NET",
                    "UDP echo request received");
    if (orion_net_udp_send(40171, source_ipv4, source_port,
                           payload, payload_bytes) == ORION_NET_OK) {
        (void)orion_log(ORION_LOG_INFO, "NET",
                        "UDP echo response transmitted");
    }
    if (!stats_reported) {
        orion_net_stats stats;
        stats_reported = true;
        if (orion_net_get_stats(&stats)) {
            (void)orion_log_u64(ORION_LOG_INFO, "NET", "TX frames:",
                                stats.tx_frames);
            (void)orion_log_u64(ORION_LOG_INFO, "NET", "RX frames:",
                                stats.rx_frames);
            (void)orion_log_u64(ORION_LOG_INFO, "NET", "RX dropped:",
                                stats.rx_dropped);
            (void)orion_log_u64(ORION_LOG_INFO, "NET", "TX errors:",
                                stats.tx_errors);
            (void)orion_log_u64(ORION_LOG_INFO, "NET", "RX errors:",
                                stats.rx_errors);
        }
    }
}
#endif

static bool driver_framework_self_test(void)
{
    static const orion_device serial_device = {
        .bus = ORION_DEVICE_PLATFORM,
        .platform_name = "uart16550-com1"
    };
    orion_driver_binding binding;
    orion_pci_scan_result pci_result;

    if (!orion_driver_system_init() ||
        !orion_driver_register(orion_uart16550_driver()) ||
        !orion_driver_register(orion_ne2k_pci_driver()) ||
        !orion_driver_add_device(&serial_device) ||
        !orion_driver_bind_device(0) ||
        !orion_driver_get_binding(0, &binding) ||
        binding.state != ORION_DRIVER_ONLINE ||
        binding.driver != orion_uart16550_driver() ||
        !orion_pci_scan_bus(0, orion_pci_read_config_legacy,
                            pci_device_discovered, NULL, &pci_result)) {
        return false;
    }
    bool network_device_found = false;
    for (uint32_t index = 1; index < orion_driver_device_count(); ++index) {
        orion_driver_binding pci_binding;
        if (!orion_driver_get_binding(index, &pci_binding)) {
            return false;
        }
        if (pci_binding.device.vendor_id != UINT16_C(0x10ec) ||
            pci_binding.device.device_id != UINT16_C(0x8029)) {
            continue;
        }
        network_device_found = true;
        if (!orion_driver_bind_device(index) ||
            !orion_driver_get_binding(index, &pci_binding) ||
            pci_binding.state != ORION_DRIVER_ONLINE ||
            pci_binding.driver != orion_ne2k_pci_driver()) {
            (void)orion_log(ORION_LOG_WARN, "NET",
                            "NE2000 PCI device found but initialization failed");
            continue;
        }
        const orion_net_interface *network =
            orion_ne2k_pci_interface();
        if (network == NULL ||
            !orion_log(ORION_LOG_INFO, "NET",
                       "NE2000 PCI interface online; MAC available") ||
            !orion_log_u64(ORION_LOG_INFO, "NET", "Interface MTU:",
                           network->mtu)) {
            return false;
        }
        for (uint32_t mac_index = 0; mac_index < ORION_NET_MAC_BYTES;
             ++mac_index) {
            if (!orion_log_u64(ORION_LOG_INFO, "NET", "MAC byte:",
                               network->mac[mac_index])) {
                return false;
            }
        }
    }
    if (!network_device_found &&
        !orion_log(ORION_LOG_WARN, "NET",
                   "No supported NE2000 PCI network interface detected")) {
        return false;
    }
#if defined(ORION_NETWORK_QEMU_TEST)
    if (!orion_net_is_available()) {
        return false;
    }
#endif
    if (!orion_log_u64(ORION_LOG_INFO, "DRIVER",
                       "PCI functions discovered on bus 0:",
                       pci_result.discovered) ||
        !orion_log_u64(ORION_LOG_WARN, "DRIVER",
                       "PCI functions rejected by bounded inventory:",
                       pci_result.rejected) ||
        !orion_log_u64(ORION_LOG_INFO, "DRIVER",
                       "Devices recorded in bounded inventory:",
                       orion_driver_device_count()) ||
        !orion_log(ORION_LOG_INFO, "DRIVER",
                   "16550-compatible COM1 lifecycle passed")) {
        return false;
    }

    if (!orion_ata_pio_prepare()) {
#if defined(ORION_STORAGE_QEMU_TEST)
        return false;
#else
        return orion_log(ORION_LOG_WARN, "STORAGE",
                         "No supported primary-master ATA disk detected");
#endif
    }
    const uint32_t ata_device_index = orion_driver_device_count();
    static const orion_device ata_device = {
        .bus = ORION_DEVICE_PLATFORM,
        .platform_name = "ata-primary-master"
    };
    orion_driver_binding ata_binding;
    if (!orion_driver_register(orion_ata_pio_driver()) ||
        !orion_driver_add_device(&ata_device) ||
        !orion_driver_bind_device(ata_device_index) ||
        !orion_driver_get_binding(ata_device_index, &ata_binding) ||
        ata_binding.state != ORION_DRIVER_ONLINE ||
        ata_binding.driver != orion_ata_pio_driver()) {
        return false;
    }
    orion_block_device *block_device = orion_ata_pio_block_device();
    if (block_device == NULL ||
        !orion_block_device_is_valid(block_device) ||
        !orion_log_u64(ORION_LOG_INFO, "STORAGE",
                       "ATA primary-master sectors:",
                       block_device->block_count) ||
        !orion_log(ORION_LOG_INFO, "STORAGE",
                   "ATA PIO block device online")) {
        return false;
    }
#if defined(ORION_STORAGE_QEMU_TEST)
    if (!storage_filesystem_self_test(block_device)) {
        return false;
    }
#endif
    return orion_driver_check_invariants();
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

    if (!driver_framework_self_test()) {
        orion_panic_at("DRIVER", "driver or PCI discovery self-test failed");
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
    if (!timekeeping_self_test()) {
        orion_panic_at("TIME", "monotonic clock or timer self-test failed");
    }
#if defined(ORION_USER_QEMU_TEST)
    uint64_t user_exit_code = UINT64_MAX;
    if (!orion_user_program_run(boot_info, &user_exit_code) ||
        user_exit_code != 0 ||
        !orion_log(ORION_LOG_INFO, "USER",
                   "CPL3 application syscalls and teardown passed")) {
        orion_panic_at("USER", "CPL3 application integration test failed");
    }
#endif
#if defined(ORION_NETWORK_QEMU_TEST)
    if (orion_net_configure(UINT32_C(0x0a00020f),
                            UINT32_C(0xffffff00),
                            UINT32_C(0x0a000202),
                            network_clock_read, NULL) != ORION_NET_OK ||
        orion_net_udp_bind(40171, network_test_udp_echo, NULL) !=
            ORION_NET_OK) {
        orion_panic_at("NET", "network test configuration failed");
    }
    (void)orion_log(ORION_LOG_INFO, "NET",
                    "IPv4 10.0.2.15/24 configured; UDP echo port 40171 ready");
#endif

    advance_stage(ORION_INIT_CORE_READY, ORION_INIT_IDLE);
    if (!orion_acpi_init(boot_info)) {
        orion_panic_at("POWER", "ACPI initialization failed");
    }
    if (!orion_log(ORION_LOG_INFO, "POWER", "ACPI subsystem initialized")) {
        orion_panic_at("POWER", "failed to log ACPI initialization");
    }
    if (!orion_log(ORION_LOG_INFO, "CORE",
                   "Minimal kernel is running; entering idle loop")) {
        orion_panic_at("CORE", "serial logging failed before idle loop");
    }

    orion_x86_64_interrupts_enable();
#if defined(ORION_NETWORK_QEMU_TEST)
    orion_net_ping_result ping_result;
    const orion_net_status ping_status =
        orion_net_ping(UINT32_C(0x0a000202), 1000, &ping_result);
    (void)orion_log(
        ping_status == ORION_NET_OK ? ORION_LOG_INFO : ORION_LOG_WARN,
        "NET", ping_status == ORION_NET_OK
                   ? "ICMP ping to 10.0.2.2 succeeded"
                   : ping_status == ORION_NET_TIMEOUT
                         ? "ICMP ping to 10.0.2.2 timed out"
                         : "ICMP ping to 10.0.2.2 failed");
    (void)orion_log_u64(ORION_LOG_INFO, "NET", "Ping requests sent:",
                        ping_result.requests_sent);
    (void)orion_log_u64(ORION_LOG_INFO, "NET", "Ping replies received:",
                        ping_result.replies_received);
    (void)orion_log_u64(
        ORION_LOG_INFO, "NET", "Ping packets lost:",
        ping_result.requests_sent > ping_result.replies_received
            ? ping_result.requests_sent - ping_result.replies_received
            : 0);
    (void)orion_log_u64(ORION_LOG_INFO, "NET", "Ping RTT nanoseconds:",
                        ping_result.round_trip_nanoseconds);
    (void)orion_log(
        ORION_LOG_INFO, "NET",
        "Ping RTT uses coarse PIT ticks; zero means below clock resolution");
#endif
    for (;;) {
        uint32_t callbacks_run = 0;
        if (!orion_timer_run_expired(ORION_TIMER_CAPACITY, &callbacks_run)) {
            orion_panic_at("TIME", "timer callback dispatch failed");
        }
        if (orion_net_is_available() && orion_net_is_configured()) {
            uint32_t frames_received = 0;
            const orion_net_status net_status =
                orion_net_poll(8, &frames_received);
            static bool network_poll_failure_reported;
            if (net_status != ORION_NET_OK &&
                net_status != ORION_NET_BUSY &&
                !network_poll_failure_reported) {
                network_poll_failure_reported = true;
                (void)orion_log(ORION_LOG_WARN, "NET",
                                "Network receive polling reported an error");
            }
        }
        __asm__ volatile("hlt" : : : "memory");
    }
}
