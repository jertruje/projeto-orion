#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "channel.h"
#include "process.h"
#include "scheduler.h"

static uint8_t bootstrap_stack[32768] __attribute__((aligned(16)));

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

static void test_messages_and_limits(void)
{
    orion_capability_handle sender;
    orion_capability_handle receiver;
    assert(orion_ipc_channel_create(&sender, &receiver) == ORION_IPC_OK);
    assert(orion_ipc_check_invariants());

    uint8_t source[ORION_IPC_MESSAGE_MAX_SIZE];
    uint8_t destination[ORION_IPC_MESSAGE_MAX_SIZE];
    for (uint32_t index = 0; index < sizeof(source); ++index) {
        source[index] = (uint8_t)(index * 17U + 3U);
    }

    uint32_t received_size = UINT32_MAX;
    assert(orion_ipc_receive(receiver, destination, sizeof(destination),
                             &received_size) == ORION_IPC_QUEUE_EMPTY);
    assert(received_size == 0);
    assert(orion_ipc_send(sender, source,
                          ORION_IPC_MESSAGE_MAX_SIZE + 1U) ==
           ORION_IPC_MESSAGE_TOO_LARGE);

    for (uint32_t index = 0; index < ORION_IPC_QUEUE_CAPACITY; ++index) {
        source[0] = (uint8_t)index;
        assert(orion_ipc_send(sender, source, sizeof(source)) == ORION_IPC_OK);
    }
    assert(orion_ipc_send(sender, source, sizeof(source)) ==
           ORION_IPC_QUEUE_FULL);
    assert(orion_ipc_receive(receiver, NULL, sizeof(destination),
                             &received_size) == ORION_IPC_INVALID_ARGUMENT);
    assert(received_size == 0);

    orion_ipc_endpoint_info info;
    assert(orion_ipc_query(receiver, &info) == ORION_IPC_OK);
    assert(info.incoming_messages == ORION_IPC_QUEUE_CAPACITY);
    assert(info.incoming_capacity == ORION_IPC_QUEUE_CAPACITY);
    assert(info.endpoint_open && info.peer_open);

    assert(orion_ipc_receive(receiver, destination, 1U, &received_size) ==
           ORION_IPC_BUFFER_TOO_SMALL);
    assert(received_size == sizeof(destination));
    for (uint32_t index = 0; index < ORION_IPC_QUEUE_CAPACITY; ++index) {
        assert(orion_ipc_receive(receiver, destination, sizeof(destination),
                                 &received_size) == ORION_IPC_OK);
        assert(received_size == sizeof(destination));
        assert(destination[0] == (uint8_t)index);
        for (uint32_t byte = 1; byte < sizeof(destination); ++byte) {
            assert(destination[byte] == (uint8_t)(byte * 17U + 3U));
        }
    }

    assert(orion_ipc_send(sender, NULL, 0) == ORION_IPC_OK);
    assert(orion_ipc_receive(receiver, NULL, 0, &received_size) ==
           ORION_IPC_OK);
    assert(received_size == 0);
    assert(orion_ipc_receive(receiver, NULL, 0, NULL) ==
           ORION_IPC_INVALID_ARGUMENT);
    assert(orion_ipc_check_invariants());
    assert(orion_ipc_close(sender) == ORION_IPC_OK);
    assert(orion_ipc_send(sender, source, 1U) ==
           ORION_IPC_INVALID_CAPABILITY);
    assert(orion_ipc_receive(receiver, destination, sizeof(destination),
                             &received_size) == ORION_IPC_PEER_CLOSED);
    assert(orion_ipc_close(receiver) == ORION_IPC_OK);
    assert(orion_ipc_check_invariants());
}

static void test_capability_authorization(void)
{
    orion_capability_handle sender;
    orion_capability_handle receiver;
    assert(orion_ipc_channel_create(&sender, &receiver) == ORION_IPC_OK);

    orion_process *owner = orion_process_kernel();
    assert(owner != NULL);
    orion_capability_handle query_only;
    assert(orion_capability_derive(owner, sender, ORION_RIGHT_QUERY,
               &query_only) == ORION_CAPABILITY_OK);
    assert(orion_ipc_send(query_only, "x", 1U) ==
           ORION_IPC_PERMISSION_DENIED);
    assert(orion_ipc_query(query_only, &(orion_ipc_endpoint_info){0}) ==
           ORION_IPC_OK);
    assert(orion_capability_close(owner, query_only) == ORION_CAPABILITY_OK);

    assert(orion_ipc_send(sender ^ (UINT64_C(1) << 40), "x", 1U) ==
           ORION_IPC_INVALID_CAPABILITY);
    assert(orion_ipc_send(0, "x", 1U) == ORION_IPC_INVALID_CAPABILITY);
    assert(orion_ipc_close(sender) == ORION_IPC_OK);
    assert(orion_ipc_close(receiver) == ORION_IPC_OK);
}

static void test_isolation_and_resource_exhaustion(void)
{
    orion_capability_handle first_sender;
    orion_capability_handle first_receiver;
    orion_capability_handle second_sender;
    orion_capability_handle second_receiver;
    assert(orion_ipc_channel_create(&first_sender, &first_receiver) ==
           ORION_IPC_OK);
    assert(orion_ipc_channel_create(&second_sender, &second_receiver) ==
           ORION_IPC_OK);
    assert(orion_ipc_send(first_sender, "a", 1U) == ORION_IPC_OK);
    assert(orion_ipc_send(second_sender, "b", 1U) == ORION_IPC_OK);
    assert(orion_ipc_close(first_sender) == ORION_IPC_OK);

    char byte = 0;
    uint32_t received_size;
    assert(orion_ipc_receive(first_receiver, &byte, sizeof(byte),
                             &received_size) == ORION_IPC_OK);
    assert(byte == 'a' && received_size == 1U);
    assert(orion_ipc_receive(first_receiver, &byte, sizeof(byte),
                             &received_size) == ORION_IPC_PEER_CLOSED);
    assert(orion_ipc_receive(second_receiver, &byte, sizeof(byte),
                             &received_size) == ORION_IPC_OK);
    assert(byte == 'b' && received_size == 1U);
    assert(orion_ipc_close(first_receiver) == ORION_IPC_OK);
    assert(orion_ipc_close(second_sender) == ORION_IPC_OK);
    assert(orion_ipc_close(second_receiver) == ORION_IPC_OK);
    assert(orion_ipc_check_invariants());

    orion_capability_handle channels[ORION_IPC_CHANNEL_LIMIT][2];
    for (uint32_t index = 0; index < ORION_IPC_CHANNEL_LIMIT; ++index) {
        assert(orion_ipc_channel_create(&channels[index][0],
                                        &channels[index][1]) == ORION_IPC_OK);
    }
    orion_capability_handle exhausted_a = UINT64_MAX;
    orion_capability_handle exhausted_b = UINT64_MAX;
    assert(orion_ipc_channel_create(&exhausted_a, &exhausted_b) ==
           ORION_IPC_RESOURCE_EXHAUSTED);
    assert(exhausted_a == 0 && exhausted_b == 0);
    for (uint32_t index = 0; index < ORION_IPC_CHANNEL_LIMIT; ++index) {
        assert(orion_ipc_close(channels[index][0]) == ORION_IPC_OK);
        assert(orion_ipc_close(channels[index][1]) == ORION_IPC_OK);
    }
    assert(orion_ipc_check_invariants());
}

int main(void)
{
    assert(orion_scheduler_init((uintptr_t)bootstrap_stack,
                                (uintptr_t)(bootstrap_stack +
                                            sizeof(bootstrap_stack))));
    assert(orion_ipc_system_init());
    test_messages_and_limits();
    test_capability_authorization();
    test_isolation_and_resource_exhaustion();
    assert(orion_ipc_check_invariants());
    return 0;
}
