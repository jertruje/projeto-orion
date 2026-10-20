#ifndef ORION_KERNEL_SECURITY_CAPABILITY_H
#define ORION_KERNEL_SECURITY_CAPABILITY_H

#include <stdbool.h>
#include <stdint.h>

struct orion_process;

#define ORION_CAPABILITY_PROCESS_LIMIT 8U
#define ORION_CAPABILITY_SLOT_LIMIT 32U
#define ORION_CAPABILITY_OBJECT_LIMIT 64U
#define ORION_CAPABILITY_LEASE_LIMIT 64U

typedef uint64_t orion_capability_handle;

typedef enum {
    ORION_OBJECT_PROCESS = 1,
    ORION_OBJECT_THREAD,
    ORION_OBJECT_ADDRESS_SPACE,
    ORION_OBJECT_MEMORY_REGION,
    ORION_OBJECT_TIMER,
    ORION_OBJECT_DEVICE,
    ORION_OBJECT_CHANNEL,
    ORION_OBJECT_STORAGE
} orion_object_type;

typedef enum {
    ORION_RIGHT_QUERY = 1U << 0,
    ORION_RIGHT_USE = 1U << 1,
    ORION_RIGHT_MODIFY = 1U << 2,
    ORION_RIGHT_ADMIN = 1U << 3,
    ORION_RIGHT_DERIVE = 1U << 4,
    ORION_RIGHT_TRANSFER = 1U << 5,
    ORION_RIGHT_REVOKE = 1U << 6,
    ORION_RIGHT_SEND = 1U << 7,
    ORION_RIGHT_RECEIVE = 1U << 8,
    ORION_RIGHT_CLOSE = 1U << 9
} orion_capability_right;

#define ORION_RIGHT_ALL \
    (ORION_RIGHT_QUERY | ORION_RIGHT_USE | ORION_RIGHT_MODIFY | \
     ORION_RIGHT_ADMIN | ORION_RIGHT_DERIVE | ORION_RIGHT_TRANSFER | \
     ORION_RIGHT_REVOKE | ORION_RIGHT_SEND | ORION_RIGHT_RECEIVE | \
     ORION_RIGHT_CLOSE)

typedef enum {
    ORION_CAPABILITY_OK,
    ORION_CAPABILITY_INVALID_ARGUMENT,
    ORION_CAPABILITY_INVALID_HANDLE,
    ORION_CAPABILITY_WRONG_TYPE,
    ORION_CAPABILITY_RIGHTS_DENIED,
    ORION_CAPABILITY_TABLE_FULL,
    ORION_CAPABILITY_PROCESS_LIMIT_REACHED,
    ORION_CAPABILITY_OBJECT_LIMIT_REACHED,
    ORION_CAPABILITY_REVOKED,
    ORION_CAPABILITY_OBJECT_BUSY,
    ORION_CAPABILITY_STALE_LEASE,
    ORION_CAPABILITY_CORRUPTION
} orion_capability_status;

typedef void (*orion_capability_object_destructor)(void *object);

typedef struct {
    orion_capability_handle token;
    struct orion_process *owner;
    void *object;
    bool active;
} orion_capability_lease;

typedef struct {
    orion_object_type type;
    uint32_t rights;
    struct orion_process *owner;
} orion_capability_info;
bool orion_capability_system_init(void);
bool orion_capability_process_init(struct orion_process *process);
orion_capability_status orion_capability_process_release(
    struct orion_process *process);
bool orion_capability_process_check(const struct orion_process *process);
bool orion_capability_check_invariants(void);

orion_capability_status orion_capability_create_kernel_object(
    struct orion_process *owner, orion_object_type type, void *object,
    uint32_t rights, orion_capability_object_destructor destructor,
    orion_capability_handle *handle);
orion_capability_status orion_capability_derive(
    struct orion_process *owner, orion_capability_handle source,
    uint32_t reduced_rights, orion_capability_handle *derived);
orion_capability_status orion_capability_transfer(
    struct orion_process *source_owner, orion_capability_handle source,
    struct orion_process *target_owner, uint32_t reduced_rights,
    orion_capability_handle *transferred);
orion_capability_status orion_capability_describe(
    struct orion_process *owner, orion_capability_handle handle,
    orion_capability_info *info);
orion_capability_status orion_capability_acquire(
    struct orion_process *owner, orion_capability_handle handle,
    orion_object_type expected_type, uint32_t required_rights,
    orion_capability_lease *lease);
orion_capability_status orion_capability_lease_release(
    struct orion_process *owner, orion_capability_lease *lease);
orion_capability_status orion_capability_close(
    struct orion_process *owner, orion_capability_handle handle);
orion_capability_status orion_capability_revoke(
    struct orion_process *owner, orion_capability_handle handle);

const char *orion_capability_status_name(orion_capability_status status);
bool orion_object_rights_are_valid(orion_object_type type, uint32_t rights);

#endif
