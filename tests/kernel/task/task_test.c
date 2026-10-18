#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "capability.h"
#include "scheduler.h"

static uint8_t bootstrap_stack[32768] __attribute__((aligned(16)));
static uint32_t destructor_calls;
static uint64_t test_object;
static uint64_t object_payloads[ORION_CAPABILITY_OBJECT_LIMIT + 1U];

static void test_object_destructor(void *object)
{
    assert(object == &test_object);
    ++destructor_calls;
}

static void unused_entry(void *argument)
{
    (void)argument;
}

bool orion_x86_64_interrupts_enabled(void)
{
    return false;
}

bool orion_x86_64_interrupts_save_disable(void)
{
    return false;
}

void orion_x86_64_interrupts_restore(bool was_enabled)
{
    assert(!was_enabled);
}

void orion_x86_64_interrupts_disable(void)
{
}

void orion_x86_64_interrupts_enable(void)
{
    abort();
}

void orion_x86_64_context_switch(uint64_t **old_stack_pointer,
                                 uint64_t *new_stack_pointer)
{
    (void)old_stack_pointer;
    (void)new_stack_pointer;
    abort();
}

void orion_x86_64_thread_start_trampoline(void)
{
    abort();
}

void orion_x86_64_thread_returned(void)
{
    abort();
}

_Noreturn void orion_panic_at(const char *module, const char *reason)
{
    (void)module;
    (void)reason;
    abort();
}

int main(void)
{
    assert(orion_scheduler_init((uintptr_t)bootstrap_stack,
                                (uintptr_t)(bootstrap_stack +
                                            sizeof(bootstrap_stack))));
    orion_thread *bootstrap = orion_scheduler_current();
    assert(bootstrap != NULL);
    assert(bootstrap->id == 1);
    assert(bootstrap->state == ORION_THREAD_RUNNING);
    assert(bootstrap->bootstrap_thread);
    assert(!bootstrap->owns_stack);
    assert(!orion_scheduler_destroy_thread(bootstrap));
    assert(orion_scheduler_check_invariants());

    orion_thread *threads[ORION_THREAD_MAX_COUNT] = {0};
    for (uint32_t index = 0; index < ORION_THREAD_MAX_COUNT - 1U; ++index) {
        assert(orion_scheduler_create_kernel_thread(
            unused_entry, (void *)(uintptr_t)(index + 1U), &threads[index]));
        assert(threads[index]->state == ORION_THREAD_READY);
        assert(threads[index]->owns_stack);
        assert(orion_thread_stack_is_valid(threads[index]));
        assert(threads[index]->stack_top - threads[index]->stack_bottom ==
               ORION_THREAD_STACK_SIZE);
    }
    assert(!orion_scheduler_create_kernel_thread(
        unused_entry, NULL, &threads[ORION_THREAD_MAX_COUNT - 1U]));
    assert(!orion_scheduler_create_kernel_thread(NULL, NULL, &threads[0]));
    assert(!orion_thread_transition(threads[0], ORION_THREAD_BLOCKED));
    assert(threads[0]->state == ORION_THREAD_READY);
    assert(!orion_scheduler_destroy_thread(threads[0]));

    orion_scheduler_stats stats;
    assert(orion_scheduler_get_stats(&stats));
    assert(stats.ready_threads == ORION_THREAD_MAX_COUNT - 1U);
    assert(stats.managed_threads == ORION_THREAD_MAX_COUNT);
    assert(orion_scheduler_check_invariants());
    assert(orion_process_check_invariants(orion_process_kernel()));

    orion_process other_process = {
        .id = 2,
        .state = ORION_PROCESS_ACTIVE
    };
    assert(orion_capability_process_init(&other_process));
    assert(orion_capability_check_invariants());

    const uint32_t root_rights =
        ORION_RIGHT_QUERY | ORION_RIGHT_USE | ORION_RIGHT_MODIFY |
        ORION_RIGHT_DERIVE | ORION_RIGHT_TRANSFER | ORION_RIGHT_REVOKE;
    orion_capability_handle root = 0;
    assert(orion_capability_create_kernel_object(
               orion_process_kernel(), ORION_OBJECT_MEMORY_REGION,
               &test_object, root_rights, test_object_destructor, &root) ==
           ORION_CAPABILITY_OK);

    orion_capability_lease lease = {0};
    assert(orion_capability_acquire(orion_process_kernel(), root,
                                    ORION_OBJECT_MEMORY_REGION,
                                    ORION_RIGHT_USE, &lease) ==
           ORION_CAPABILITY_OK);
    assert(lease.object == &test_object);
    assert(orion_capability_acquire(orion_process_kernel(), root,
                                    ORION_OBJECT_DEVICE, ORION_RIGHT_USE,
                                    &lease) == ORION_CAPABILITY_WRONG_TYPE);
    assert(orion_capability_acquire(orion_process_kernel(), root,
                                    ORION_OBJECT_MEMORY_REGION,
                                    ORION_RIGHT_ADMIN, &lease) ==
           ORION_CAPABILITY_RIGHTS_DENIED);
    assert(orion_capability_lease_release(&other_process, &lease) ==
           ORION_CAPABILITY_STALE_LEASE);
    assert(orion_capability_lease_release(orion_process_kernel(), &lease) ==
           ORION_CAPABILITY_OK);

    orion_capability_handle read_only = 0;
    assert(orion_capability_derive(orion_process_kernel(), root,
                                   ORION_RIGHT_QUERY | ORION_RIGHT_USE,
                                   &read_only) == ORION_CAPABILITY_OK);
    orion_capability_handle forged = root ^ (UINT64_C(1) << 40);
    orion_capability_info info;
    assert(orion_capability_describe(orion_process_kernel(), forged, &info) ==
           ORION_CAPABILITY_INVALID_HANDLE);
    assert(orion_capability_describe(&other_process, root, &info) ==
           ORION_CAPABILITY_INVALID_HANDLE);
    assert(orion_capability_derive(orion_process_kernel(), read_only,
                                   ORION_RIGHT_MODIFY, &forged) ==
           ORION_CAPABILITY_RIGHTS_DENIED);
    assert(orion_capability_derive(orion_process_kernel(), read_only,
                                   ORION_RIGHT_QUERY | ORION_RIGHT_ADMIN,
                                   &forged) == ORION_CAPABILITY_RIGHTS_DENIED);
    assert(orion_capability_transfer(orion_process_kernel(), read_only,
                                     &other_process, ORION_RIGHT_QUERY,
                                     &forged) ==
           ORION_CAPABILITY_RIGHTS_DENIED);
    assert(orion_capability_transfer(orion_process_kernel(), root,
                                     &other_process,
                                     ORION_RIGHT_QUERY | ORION_RIGHT_USE,
                                     &forged) == ORION_CAPABILITY_OK);
    assert(orion_capability_describe(&other_process, forged, &info) ==
           ORION_CAPABILITY_OK);
    assert(info.type == ORION_OBJECT_MEMORY_REGION);
    assert(info.rights == (ORION_RIGHT_QUERY | ORION_RIGHT_USE));
    assert(orion_capability_acquire(&other_process, forged,
                                    ORION_OBJECT_MEMORY_REGION,
                                    ORION_RIGHT_USE, &lease) ==
           ORION_CAPABILITY_OK);
    assert(orion_capability_lease_release(orion_process_kernel(), &lease) ==
           ORION_CAPABILITY_STALE_LEASE);
    assert(orion_capability_lease_release(&other_process, &lease) ==
           ORION_CAPABILITY_OK);

    orion_capability_handle transferred[ORION_CAPABILITY_SLOT_LIMIT];
    transferred[0] = forged;
    for (uint32_t index = 1; index < ORION_CAPABILITY_SLOT_LIMIT; ++index) {
        assert(orion_capability_transfer(orion_process_kernel(), root,
                                         &other_process, ORION_RIGHT_QUERY,
                                         &transferred[index]) ==
               ORION_CAPABILITY_OK);
    }
    assert(orion_capability_transfer(orion_process_kernel(), root,
                                     &other_process, ORION_RIGHT_QUERY,
                                     &forged) == ORION_CAPABILITY_TABLE_FULL);
    assert(orion_capability_close(&other_process, transferred[0]) ==
           ORION_CAPABILITY_OK);
    assert(orion_capability_describe(&other_process, transferred[0], &info) ==
           ORION_CAPABILITY_INVALID_HANDLE);
    assert(orion_capability_transfer(orion_process_kernel(), root,
                                     &other_process, ORION_RIGHT_QUERY,
                                     &forged) == ORION_CAPABILITY_OK);
    assert(forged != transferred[0]);
    assert(orion_capability_describe(&other_process, transferred[0], &info) ==
           ORION_CAPABILITY_INVALID_HANDLE);

    orion_capability_lease leases[ORION_CAPABILITY_LEASE_LIMIT] = {0};
    for (uint32_t index = 0; index < ORION_CAPABILITY_LEASE_LIMIT; ++index) {
        assert(orion_capability_acquire(&other_process, transferred[1],
                                        ORION_OBJECT_MEMORY_REGION,
                                        ORION_RIGHT_QUERY, &leases[index]) ==
               ORION_CAPABILITY_OK);
    }
    assert(orion_capability_acquire(&other_process, transferred[1],
                                    ORION_OBJECT_MEMORY_REGION,
                                    ORION_RIGHT_QUERY, &lease) ==
           ORION_CAPABILITY_TABLE_FULL);
    assert(orion_capability_process_release(&other_process) ==
           ORION_CAPABILITY_OBJECT_BUSY);
    assert(orion_capability_revoke(orion_process_kernel(), read_only) ==
           ORION_CAPABILITY_RIGHTS_DENIED);
    assert(orion_capability_revoke(orion_process_kernel(), root) ==
           ORION_CAPABILITY_OK);
    assert(destructor_calls == 0);
    assert(orion_capability_describe(orion_process_kernel(), root, &info) ==
           ORION_CAPABILITY_INVALID_HANDLE);
    assert(orion_capability_describe(orion_process_kernel(), read_only, &info) ==
           ORION_CAPABILITY_INVALID_HANDLE);
    assert(orion_capability_describe(&other_process, transferred[1], &info) ==
           ORION_CAPABILITY_INVALID_HANDLE);
    for (uint32_t index = 0; index < ORION_CAPABILITY_LEASE_LIMIT; ++index) {
        assert(orion_capability_lease_release(&other_process,
                                              &leases[index]) ==
               ORION_CAPABILITY_OK);
    }
    assert(destructor_calls == 1);
    assert(orion_capability_lease_release(&other_process,
                                          &leases[0]) ==
           ORION_CAPABILITY_STALE_LEASE);
    assert(orion_capability_close(orion_process_kernel(), root) ==
           ORION_CAPABILITY_INVALID_HANDLE);
    assert(orion_capability_check_invariants());
    assert(orion_capability_process_release(&other_process) ==
           ORION_CAPABILITY_OK);

    assert(orion_capability_process_init(&other_process));
    assert(orion_capability_describe(&other_process, transferred[0], &info) ==
           ORION_CAPABILITY_INVALID_HANDLE);
    assert(orion_capability_create_kernel_object(
               &other_process, ORION_OBJECT_MEMORY_REGION, &test_object,
               ORION_RIGHT_QUERY, NULL, &root) ==
           ORION_CAPABILITY_INVALID_ARGUMENT);
    orion_capability_handle stale_handle = 0;
    assert(orion_capability_create_kernel_object(
               orion_process_kernel(), ORION_OBJECT_DEVICE, &test_object,
               ORION_RIGHT_QUERY | ORION_RIGHT_TRANSFER |
                   ORION_RIGHT_REVOKE,
               NULL, &root) == ORION_CAPABILITY_OK);
    assert(orion_capability_transfer(orion_process_kernel(), root,
                                     &other_process, ORION_RIGHT_QUERY,
                                     &stale_handle) == ORION_CAPABILITY_OK);
    assert(orion_capability_process_release(&other_process) ==
           ORION_CAPABILITY_OK);
    assert(orion_capability_process_init(&other_process));
    assert(orion_capability_describe(&other_process, stale_handle, &info) ==
           ORION_CAPABILITY_INVALID_HANDLE);
    assert(orion_capability_revoke(orion_process_kernel(), root) ==
           ORION_CAPABILITY_OK);
    assert(orion_capability_process_release(&other_process) ==
           ORION_CAPABILITY_OK);
    assert(orion_capability_process_init(&other_process));
    assert(orion_capability_process_release(&other_process) ==
           ORION_CAPABILITY_OK);

    orion_process process_pool[ORION_CAPABILITY_PROCESS_LIMIT] = {0};
    for (uint32_t index = 0;
         index < ORION_CAPABILITY_PROCESS_LIMIT - 1U; ++index) {
        process_pool[index].id = (uint64_t)index + 10U;
        process_pool[index].state = ORION_PROCESS_ACTIVE;
        assert(orion_capability_process_init(&process_pool[index]));
    }
    process_pool[ORION_CAPABILITY_PROCESS_LIMIT - 1U].id = 99;
    process_pool[ORION_CAPABILITY_PROCESS_LIMIT - 1U].state =
        ORION_PROCESS_ACTIVE;
    assert(!orion_capability_process_init(
        &process_pool[ORION_CAPABILITY_PROCESS_LIMIT - 1U]));
    for (uint32_t index = 0;
         index < ORION_CAPABILITY_PROCESS_LIMIT - 1U; ++index) {
        assert(orion_capability_process_release(&process_pool[index]) ==
               ORION_CAPABILITY_OK);
    }
    assert(orion_capability_process_init(&other_process));

    for (uint32_t index = 0; index < ORION_CAPABILITY_OBJECT_LIMIT; ++index) {
        orion_capability_handle object_handle = 0;
        orion_capability_handle child_handle = 0;
        assert(orion_capability_create_kernel_object(
                   orion_process_kernel(), ORION_OBJECT_STORAGE,
                   &object_payloads[index],
                   ORION_RIGHT_QUERY | ORION_RIGHT_TRANSFER |
                       ORION_RIGHT_REVOKE,
                   NULL, &object_handle) == ORION_CAPABILITY_OK);
        assert(orion_capability_transfer(orion_process_kernel(),
                                         object_handle, &other_process,
                                         ORION_RIGHT_QUERY,
                                         &child_handle) ==
               ORION_CAPABILITY_OK);
        assert(orion_capability_close(orion_process_kernel(), object_handle) ==
               ORION_CAPABILITY_OK);
        assert(orion_capability_close(&other_process, child_handle) ==
               ORION_CAPABILITY_OK);
    }
    assert(orion_capability_create_kernel_object(
               orion_process_kernel(), ORION_OBJECT_STORAGE,
               &object_payloads[ORION_CAPABILITY_OBJECT_LIMIT],
               ORION_RIGHT_QUERY, NULL, &root) ==
           ORION_CAPABILITY_OBJECT_LIMIT_REACHED);
    assert(orion_capability_check_invariants());
    return 0;
}
