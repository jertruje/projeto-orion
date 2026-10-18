#include "channel.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt_state.h"
#include "../task/scheduler.h"
#include "../task/thread.h"

typedef struct {
    uint32_t size;
    uint8_t bytes[ORION_IPC_MESSAGE_MAX_SIZE];
} ipc_message;

typedef struct {
    ipc_message messages[ORION_IPC_QUEUE_CAPACITY];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} ipc_queue;

typedef struct {
    uint32_t generation;
    bool occupied;
    uint32_t channel_index;
    uint32_t channel_generation;
    uint8_t side;
} ipc_endpoint;

typedef struct {
    uint32_t generation;
    bool occupied;
    bool retired;
    uint32_t endpoint_references;
    bool endpoint_open[2];
    ipc_queue incoming[2];
} ipc_channel;

static ipc_channel channels[ORION_IPC_CHANNEL_LIMIT];
static ipc_endpoint endpoints[ORION_IPC_CHANNEL_LIMIT * 2U];
static bool ipc_initialized;
static bool ipc_corruption_detected;

static void clear_bytes(void *memory, size_t size)
{
    volatile uint8_t *bytes = memory;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static orion_process *current_process(void)
{
    orion_thread *thread = orion_scheduler_current();
    if (thread == NULL || thread->state != ORION_THREAD_RUNNING ||
        thread->process == NULL) {
        return NULL;
    }
    return thread->process;
}

static orion_ipc_status map_capability_status(orion_capability_status status)
{
    switch (status) {
    case ORION_CAPABILITY_OK:
        return ORION_IPC_OK;
    case ORION_CAPABILITY_INVALID_HANDLE:
    case ORION_CAPABILITY_INVALID_ARGUMENT:
        return ORION_IPC_INVALID_CAPABILITY;
    case ORION_CAPABILITY_RIGHTS_DENIED:
        return ORION_IPC_PERMISSION_DENIED;
    case ORION_CAPABILITY_TABLE_FULL:
    case ORION_CAPABILITY_PROCESS_LIMIT_REACHED:
    case ORION_CAPABILITY_OBJECT_LIMIT_REACHED:
        return ORION_IPC_RESOURCE_EXHAUSTED;
    case ORION_CAPABILITY_CORRUPTION:
    case ORION_CAPABILITY_REVOKED:
    case ORION_CAPABILITY_OBJECT_BUSY:
    case ORION_CAPABILITY_STALE_LEASE:
        return ORION_IPC_CORRUPTION;
    default:
        return ORION_IPC_INVALID_ARGUMENT;
    }
}

static bool queue_is_valid(const ipc_queue *queue)
{
    return queue->head < ORION_IPC_QUEUE_CAPACITY &&
           queue->tail < ORION_IPC_QUEUE_CAPACITY &&
           queue->count <= ORION_IPC_QUEUE_CAPACITY &&
           queue->tail ==
               (queue->head + queue->count) % ORION_IPC_QUEUE_CAPACITY;
}

static ipc_channel *get_channel(const ipc_endpoint *endpoint)
{
    if (endpoint == NULL || !endpoint->occupied ||
        endpoint->channel_index >= ORION_IPC_CHANNEL_LIMIT ||
        endpoint->side > 1U) {
        return NULL;
    }
    ipc_channel *channel = &channels[endpoint->channel_index];
    if (!channel->occupied ||
        channel->generation != endpoint->channel_generation) {
        return NULL;
    }
    return channel;
}

static void clear_message(ipc_message *message)
{
    message->size = 0;
    for (uint32_t index = 0; index < ORION_IPC_MESSAGE_MAX_SIZE; ++index) {
        message->bytes[index] = 0;
    }
}

static void release_channel_if_unused(ipc_channel *channel)
{
    if (channel->endpoint_references != 0) {
        return;
    }
    channel->occupied = false;
    channel->endpoint_open[0] = false;
    channel->endpoint_open[1] = false;
    clear_bytes(channel->incoming, sizeof(channel->incoming));
    if (channel->generation == UINT32_MAX) {
        channel->retired = true;
    } else {
        ++channel->generation;
    }
}

static void endpoint_destructor(void *object)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    ipc_endpoint *endpoint = object;
    ipc_channel *channel = get_channel(endpoint);
    if (channel == NULL || channel->endpoint_references == 0) {
        ipc_corruption_detected = true;
        orion_x86_64_interrupts_restore(was_enabled);
        return;
    }

    channel->endpoint_open[endpoint->side] = false;
    ipc_queue *incoming = &channel->incoming[endpoint->side];
    for (uint32_t index = 0; index < ORION_IPC_QUEUE_CAPACITY; ++index) {
        clear_message(&incoming->messages[index]);
    }
    incoming->head = 0;
    incoming->tail = 0;
    incoming->count = 0;
    endpoint->occupied = false;
    endpoint->channel_index = 0;
    endpoint->channel_generation = 0;
    endpoint->side = 0;
    --channel->endpoint_references;
    release_channel_if_unused(channel);
    orion_x86_64_interrupts_restore(was_enabled);
}

static ipc_channel *allocate_channel(uint32_t *channel_index)
{
    for (uint32_t index = 0; index < ORION_IPC_CHANNEL_LIMIT; ++index) {
        ipc_channel *channel = &channels[index];
        if (channel->occupied || channel->retired) {
            continue;
        }
        if (channel->generation == 0) {
            channel->generation = 1;
        }
        channel->occupied = true;
        channel->endpoint_references = 0;
        channel->endpoint_open[0] = true;
        channel->endpoint_open[1] = true;
        clear_bytes(channel->incoming, sizeof(channel->incoming));
        *channel_index = index;
        return channel;
    }
    return NULL;
}

static ipc_endpoint *allocate_endpoint(uint32_t channel_index,
                                      uint32_t channel_generation,
                                      uint8_t side)
{
    for (uint32_t index = 0; index < ORION_IPC_CHANNEL_LIMIT * 2U; ++index) {
        ipc_endpoint *endpoint = &endpoints[index];
        if (!endpoint->occupied) {
            if (endpoint->generation == UINT32_MAX) {
                continue;
            }
            ++endpoint->generation;
            if (endpoint->generation == 0) {
                ++endpoint->generation;
            }
            endpoint->occupied = true;
            endpoint->channel_index = channel_index;
            endpoint->channel_generation = channel_generation;
            endpoint->side = side;
            ++channels[channel_index].endpoint_references;
            return endpoint;
        }
    }
    return NULL;
}

static void abandon_endpoint(ipc_endpoint *endpoint)
{
    ipc_channel *channel = get_channel(endpoint);
    if (channel == NULL || channel->endpoint_references == 0) {
        ipc_corruption_detected = true;
        return;
    }
    channel->endpoint_open[endpoint->side] = false;
    endpoint->occupied = false;
    endpoint->channel_index = 0;
    endpoint->channel_generation = 0;
    endpoint->side = 0;
    --channel->endpoint_references;
    release_channel_if_unused(channel);
}

bool orion_ipc_system_init(void)
{
    if (ipc_initialized || orion_x86_64_interrupts_enabled() ||
        !orion_capability_check_invariants()) {
        return false;
    }
    for (uint32_t index = 0; index < ORION_IPC_CHANNEL_LIMIT; ++index) {
        clear_bytes(&channels[index], sizeof(channels[index]));
    }
    for (uint32_t index = 0; index < ORION_IPC_CHANNEL_LIMIT * 2U; ++index) {
        clear_bytes(&endpoints[index], sizeof(endpoints[index]));
    }
    ipc_corruption_detected = false;
    ipc_initialized = true;
    return true;
}

orion_ipc_status orion_ipc_channel_create(
    orion_capability_handle *first_endpoint,
    orion_capability_handle *second_endpoint)
{
    if (first_endpoint != NULL) {
        *first_endpoint = 0;
    }
    if (second_endpoint != NULL) {
        *second_endpoint = 0;
    }
    if (!ipc_initialized) {
        return ORION_IPC_NOT_INITIALIZED;
    }
    orion_process *owner = current_process();
    if (owner == NULL || first_endpoint == NULL || second_endpoint == NULL) {
        return ORION_IPC_INVALID_ARGUMENT;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    uint32_t channel_index;
    ipc_channel *channel = allocate_channel(&channel_index);
    if (channel == NULL) {
        orion_x86_64_interrupts_restore(was_enabled);
        return ORION_IPC_RESOURCE_EXHAUSTED;
    }
    ipc_endpoint *first =
        allocate_endpoint(channel_index, channel->generation, 0);
    ipc_endpoint *second =
        allocate_endpoint(channel_index, channel->generation, 1);
    if (first == NULL || second == NULL) {
        if (first != NULL) {
            abandon_endpoint(first);
        }
        if (second != NULL) {
            abandon_endpoint(second);
        }
        release_channel_if_unused(channel);
        orion_x86_64_interrupts_restore(was_enabled);
        return ORION_IPC_RESOURCE_EXHAUSTED;
    }

    const uint32_t rights =
        ORION_RIGHT_QUERY | ORION_RIGHT_DERIVE | ORION_RIGHT_TRANSFER |
        ORION_RIGHT_REVOKE | ORION_RIGHT_SEND | ORION_RIGHT_RECEIVE |
        ORION_RIGHT_CLOSE;
    orion_capability_status capability_status =
        orion_capability_create_kernel_object(
            owner, ORION_OBJECT_CHANNEL, first, rights, endpoint_destructor,
            first_endpoint);
    if (capability_status == ORION_CAPABILITY_OK) {
        capability_status = orion_capability_create_kernel_object(
            owner, ORION_OBJECT_CHANNEL, second, rights, endpoint_destructor,
            second_endpoint);
    }
    if (capability_status != ORION_CAPABILITY_OK) {
        if (*first_endpoint != 0) {
            (void)orion_capability_revoke(owner, *first_endpoint);
        } else {
            abandon_endpoint(first);
        }
        if (*second_endpoint == 0 && second->occupied) {
            abandon_endpoint(second);
        }
        *first_endpoint = 0;
        *second_endpoint = 0;
        orion_x86_64_interrupts_restore(was_enabled);
        return map_capability_status(capability_status);
    }

    orion_x86_64_interrupts_restore(was_enabled);
    return ORION_IPC_OK;
}

static orion_ipc_status acquire_endpoint(
    orion_capability_handle handle, uint32_t rights,
    orion_capability_lease *lease)
{
    if (!ipc_initialized) {
        return ORION_IPC_NOT_INITIALIZED;
    }
    orion_process *owner = current_process();
    if (owner == NULL) {
        return ORION_IPC_INVALID_ARGUMENT;
    }
    return map_capability_status(orion_capability_acquire(
        owner, handle, ORION_OBJECT_CHANNEL, rights, lease));
}

static orion_ipc_status release_lease(orion_capability_lease *lease,
                                      orion_ipc_status status)
{
    const orion_capability_status release_status =
        orion_capability_lease_release(lease->owner, lease);
    if (release_status != ORION_CAPABILITY_OK) {
        return ORION_IPC_CORRUPTION;
    }
    return status;
}

orion_ipc_status orion_ipc_send(orion_capability_handle endpoint_handle,
                                const void *message, uint32_t message_size)
{
    orion_capability_lease lease = {0};
    orion_ipc_status status =
        acquire_endpoint(endpoint_handle, ORION_RIGHT_SEND, &lease);
    if (status != ORION_IPC_OK) {
        return status;
    }
    if (message_size > ORION_IPC_MESSAGE_MAX_SIZE ||
        (message_size != 0 && message == NULL)) {
        return release_lease(&lease, message_size > ORION_IPC_MESSAGE_MAX_SIZE
                                         ? ORION_IPC_MESSAGE_TOO_LARGE
                                         : ORION_IPC_INVALID_ARGUMENT);
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    ipc_endpoint *endpoint = lease.object;
    ipc_channel *channel = get_channel(endpoint);
    if (channel == NULL || ipc_corruption_detected) {
        status = ORION_IPC_CORRUPTION;
    } else if (!channel->endpoint_open[endpoint->side]) {
        status = ORION_IPC_ENDPOINT_CLOSED;
    } else {
        const uint8_t peer_side = (uint8_t)(1U - endpoint->side);
        ipc_queue *queue = &channel->incoming[peer_side];
        if (!queue_is_valid(queue)) {
            ipc_corruption_detected = true;
            status = ORION_IPC_CORRUPTION;
        } else if (!channel->endpoint_open[peer_side]) {
            status = ORION_IPC_PEER_CLOSED;
        } else if (queue->count == ORION_IPC_QUEUE_CAPACITY) {
            status = ORION_IPC_QUEUE_FULL;
        } else {
            ipc_message *slot = &queue->messages[queue->tail];
            slot->size = message_size;
            const uint8_t *source = message;
            for (uint32_t index = 0; index < message_size; ++index) {
                slot->bytes[index] = source[index];
            }
            queue->tail = (queue->tail + 1U) % ORION_IPC_QUEUE_CAPACITY;
            ++queue->count;
            status = ORION_IPC_OK;
        }
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return release_lease(&lease, status);
}

orion_ipc_status orion_ipc_receive(orion_capability_handle endpoint_handle,
                                   void *buffer, uint32_t buffer_capacity,
                                   uint32_t *received_size)
{
    if (received_size == NULL) {
        return ORION_IPC_INVALID_ARGUMENT;
    }
    *received_size = 0;
    orion_capability_lease lease = {0};
    orion_ipc_status status =
        acquire_endpoint(endpoint_handle, ORION_RIGHT_RECEIVE, &lease);
    if (status != ORION_IPC_OK) {
        return status;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    ipc_endpoint *endpoint = lease.object;
    ipc_channel *channel = get_channel(endpoint);
    if (channel == NULL || ipc_corruption_detected) {
        status = ORION_IPC_CORRUPTION;
    } else if (!channel->endpoint_open[endpoint->side]) {
        status = ORION_IPC_ENDPOINT_CLOSED;
    } else {
        ipc_queue *queue = &channel->incoming[endpoint->side];
        if (!queue_is_valid(queue)) {
            ipc_corruption_detected = true;
            status = ORION_IPC_CORRUPTION;
        } else if (queue->count == 0) {
            const uint8_t peer_side = (uint8_t)(1U - endpoint->side);
            status = channel->endpoint_open[peer_side]
                         ? ORION_IPC_QUEUE_EMPTY
                         : ORION_IPC_PEER_CLOSED;
        } else {
            ipc_message *slot = &queue->messages[queue->head];
            if (slot->size > ORION_IPC_MESSAGE_MAX_SIZE) {
                ipc_corruption_detected = true;
                status = ORION_IPC_CORRUPTION;
            } else {
                const uint32_t message_size = slot->size;
                if (buffer_capacity < message_size) {
                    *received_size = message_size;
                    status = ORION_IPC_BUFFER_TOO_SMALL;
                } else if (message_size != 0 && buffer == NULL) {
                    status = ORION_IPC_INVALID_ARGUMENT;
                } else {
                    uint8_t *destination = buffer;
                    for (uint32_t index = 0; index < message_size; ++index) {
                        destination[index] = slot->bytes[index];
                    }
                    clear_message(slot);
                    queue->head =
                        (queue->head + 1U) % ORION_IPC_QUEUE_CAPACITY;
                    --queue->count;
                    *received_size = message_size;
                    status = ORION_IPC_OK;
                }
            }
        }
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return release_lease(&lease, status);
}

orion_ipc_status orion_ipc_query(orion_capability_handle endpoint_handle,
                                 orion_ipc_endpoint_info *info)
{
    if (info == NULL) {
        return ORION_IPC_INVALID_ARGUMENT;
    }
    info->incoming_messages = 0;
    info->incoming_capacity = 0;
    info->endpoint_open = false;
    info->peer_open = false;
    orion_capability_lease lease = {0};
    orion_ipc_status status =
        acquire_endpoint(endpoint_handle, ORION_RIGHT_QUERY, &lease);
    if (status != ORION_IPC_OK) {
        return status;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    ipc_endpoint *endpoint = lease.object;
    ipc_channel *channel = get_channel(endpoint);
    if (channel == NULL || ipc_corruption_detected) {
        status = ORION_IPC_CORRUPTION;
    } else {
        ipc_queue *queue = &channel->incoming[endpoint->side];
        if (!queue_is_valid(queue)) {
            ipc_corruption_detected = true;
            status = ORION_IPC_CORRUPTION;
        } else {
            const uint8_t peer_side = (uint8_t)(1U - endpoint->side);
            *info = (orion_ipc_endpoint_info){
                .incoming_messages = queue->count,
                .incoming_capacity = ORION_IPC_QUEUE_CAPACITY,
                .endpoint_open = channel->endpoint_open[endpoint->side],
                .peer_open = channel->endpoint_open[peer_side]
            };
            status = ORION_IPC_OK;
        }
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return release_lease(&lease, status);
}

orion_ipc_status orion_ipc_close(orion_capability_handle endpoint_handle)
{
    orion_capability_lease lease = {0};
    orion_ipc_status status =
        acquire_endpoint(endpoint_handle, ORION_RIGHT_CLOSE, &lease);
    if (status != ORION_IPC_OK) {
        return status;
    }

    bool should_revoke = false;
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    ipc_endpoint *endpoint = lease.object;
    ipc_channel *channel = get_channel(endpoint);
    if (channel == NULL || ipc_corruption_detected) {
        status = ORION_IPC_CORRUPTION;
    } else if (!channel->endpoint_open[endpoint->side]) {
        status = ORION_IPC_ENDPOINT_CLOSED;
    } else {
        channel->endpoint_open[endpoint->side] = false;
        ipc_queue *queue = &channel->incoming[endpoint->side];
        for (uint32_t index = 0; index < ORION_IPC_QUEUE_CAPACITY; ++index) {
            clear_message(&queue->messages[index]);
        }
        queue->head = 0;
        queue->tail = 0;
        queue->count = 0;
        should_revoke = true;
        status = ORION_IPC_OK;
    }
    orion_x86_64_interrupts_restore(was_enabled);

    if (should_revoke) {
        status = map_capability_status(orion_capability_revoke(
            lease.owner, endpoint_handle));
    }
    return release_lease(&lease, status);
}

static bool check_invariants_disabled(void)
{
    if (!ipc_initialized || ipc_corruption_detected) {
        return false;
    }
    uint32_t references[ORION_IPC_CHANNEL_LIMIT] = {0};
    for (uint32_t index = 0; index < ORION_IPC_CHANNEL_LIMIT * 2U; ++index) {
        const ipc_endpoint *endpoint = &endpoints[index];
        if (!endpoint->occupied) {
            continue;
        }
        if (endpoint->channel_index >= ORION_IPC_CHANNEL_LIMIT ||
            endpoint->side > 1U ||
            !channels[endpoint->channel_index].occupied ||
            channels[endpoint->channel_index].generation !=
                endpoint->channel_generation ||
            references[endpoint->channel_index] == UINT32_MAX) {
            return false;
        }
        ++references[endpoint->channel_index];
    }
    for (uint32_t index = 0; index < ORION_IPC_CHANNEL_LIMIT; ++index) {
        const ipc_channel *channel = &channels[index];
        if (channel->occupied) {
            if (channel->generation == 0 ||
                channel->endpoint_references != references[index] ||
                channel->endpoint_references == 0 ||
                !queue_is_valid(&channel->incoming[0]) ||
                !queue_is_valid(&channel->incoming[1])) {
                return false;
            }
            for (uint32_t side = 0; side < 2; ++side) {
                const ipc_queue *queue = &channel->incoming[side];
                for (uint32_t item = 0; item < queue->count; ++item) {
                    const uint32_t slot =
                        (queue->head + item) % ORION_IPC_QUEUE_CAPACITY;
                    if (queue->messages[slot].size >
                        ORION_IPC_MESSAGE_MAX_SIZE) {
                        return false;
                    }
                }
            }
        } else if (references[index] != 0) {
            return false;
        }
    }
    return true;
}

bool orion_ipc_check_invariants(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const bool result = check_invariants_disabled();
    orion_x86_64_interrupts_restore(was_enabled);
    return result;
}

const char *orion_ipc_status_name(orion_ipc_status status)
{
    switch (status) {
    case ORION_IPC_OK: return "ok";
    case ORION_IPC_INVALID_ARGUMENT: return "invalid argument";
    case ORION_IPC_INVALID_CAPABILITY: return "invalid capability";
    case ORION_IPC_PERMISSION_DENIED: return "permission denied";
    case ORION_IPC_MESSAGE_TOO_LARGE: return "message too large";
    case ORION_IPC_QUEUE_EMPTY: return "queue empty";
    case ORION_IPC_QUEUE_FULL: return "queue full";
    case ORION_IPC_PEER_CLOSED: return "peer closed";
    case ORION_IPC_ENDPOINT_CLOSED: return "endpoint closed";
    case ORION_IPC_BUFFER_TOO_SMALL: return "buffer too small";
    case ORION_IPC_RESOURCE_EXHAUSTED: return "resource exhausted";
    case ORION_IPC_NOT_INITIALIZED: return "not initialized";
    case ORION_IPC_CORRUPTION: return "IPC state corruption";
    default: return "unknown IPC error";
    }
}
