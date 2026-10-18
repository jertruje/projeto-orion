#ifndef ORION_KERNEL_IPC_CHANNEL_H
#define ORION_KERNEL_IPC_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

#include "../security/capability.h"

#define ORION_IPC_CHANNEL_LIMIT 8U
#define ORION_IPC_QUEUE_CAPACITY 8U
#define ORION_IPC_MESSAGE_MAX_SIZE 256U

typedef enum {
    ORION_IPC_OK,
    ORION_IPC_INVALID_ARGUMENT,
    ORION_IPC_INVALID_CAPABILITY,
    ORION_IPC_PERMISSION_DENIED,
    ORION_IPC_MESSAGE_TOO_LARGE,
    ORION_IPC_QUEUE_EMPTY,
    ORION_IPC_QUEUE_FULL,
    ORION_IPC_PEER_CLOSED,
    ORION_IPC_ENDPOINT_CLOSED,
    ORION_IPC_BUFFER_TOO_SMALL,
    ORION_IPC_RESOURCE_EXHAUSTED,
    ORION_IPC_NOT_INITIALIZED,
    ORION_IPC_CORRUPTION
} orion_ipc_status;

typedef struct {
    uint32_t incoming_messages;
    uint32_t incoming_capacity;
    bool endpoint_open;
    bool peer_open;
} orion_ipc_endpoint_info;

bool orion_ipc_system_init(void);
orion_ipc_status orion_ipc_channel_create(
    orion_capability_handle *first_endpoint,
    orion_capability_handle *second_endpoint);
orion_ipc_status orion_ipc_send(orion_capability_handle endpoint,
                                const void *message, uint32_t message_size);
orion_ipc_status orion_ipc_receive(orion_capability_handle endpoint,
                                   void *buffer, uint32_t buffer_capacity,
                                   uint32_t *received_size);
orion_ipc_status orion_ipc_query(orion_capability_handle endpoint,
                                 orion_ipc_endpoint_info *info);
orion_ipc_status orion_ipc_close(orion_capability_handle endpoint);
bool orion_ipc_check_invariants(void);
const char *orion_ipc_status_name(orion_ipc_status status);

#endif
