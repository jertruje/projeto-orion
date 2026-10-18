#include "capability.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt_state.h"
#include "../task/process.h"

#define CAPABILITY_SLOT_GENERATION_MAX UINT32_C(0x00ffffff)
#define CAPABILITY_HANDLE_INDEX_MASK UINT64_C(0xff)

typedef struct {
    uint32_t generation;
    bool occupied;
    bool retired;
    uint32_t object_index;
    uint32_t object_generation;
    uint32_t rights;
} capability_slot;

typedef struct {
    uint32_t generation;
    bool occupied;
    bool retired;
    const orion_process *owner;
    capability_slot slots[ORION_CAPABILITY_SLOT_LIMIT];
} capability_process_table;

typedef struct {
    uint32_t generation;
    bool occupied;
    bool retired;
    bool revoked;
    bool owner_reference;
    orion_object_type type;
    void *object;
    orion_capability_object_destructor destructor;
    uint32_t capability_references;
    uint32_t lease_references;
} capability_object;

typedef struct {
    uint32_t generation;
    bool occupied;
    bool retired;
    const orion_process *owner;
    uint32_t object_index;
    uint32_t object_generation;
} capability_lease_record;

typedef struct {
    orion_capability_object_destructor destructor;
    void *object;
} deferred_destructor;

static capability_process_table process_tables[ORION_CAPABILITY_PROCESS_LIMIT];
static capability_object objects[ORION_CAPABILITY_OBJECT_LIMIT];
static capability_lease_record leases[ORION_CAPABILITY_LEASE_LIMIT];
static bool capability_system_initialized;

extern orion_process *orion_process_kernel(void);

static bool interrupts_are_disabled(void)
{
    return !orion_x86_64_interrupts_enabled();
}

static void clear_bytes(void *memory, size_t size)
{
    volatile unsigned char *bytes = memory;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static bool type_is_valid(orion_object_type type)
{
    return type >= ORION_OBJECT_PROCESS && type <= ORION_OBJECT_STORAGE;
}

static uint32_t rights_for_type(orion_object_type type)
{
    switch (type) {
    case ORION_OBJECT_PROCESS:
        return ORION_RIGHT_QUERY | ORION_RIGHT_ADMIN |
               ORION_RIGHT_DERIVE | ORION_RIGHT_TRANSFER |
               ORION_RIGHT_REVOKE;
    case ORION_OBJECT_THREAD:
        return ORION_RIGHT_QUERY | ORION_RIGHT_USE | ORION_RIGHT_ADMIN |
               ORION_RIGHT_DERIVE | ORION_RIGHT_TRANSFER |
               ORION_RIGHT_REVOKE;
    case ORION_OBJECT_ADDRESS_SPACE:
    case ORION_OBJECT_MEMORY_REGION:
        return ORION_RIGHT_QUERY | ORION_RIGHT_USE | ORION_RIGHT_MODIFY |
               ORION_RIGHT_DERIVE | ORION_RIGHT_TRANSFER |
               ORION_RIGHT_REVOKE;
    case ORION_OBJECT_TIMER:
        return ORION_RIGHT_QUERY | ORION_RIGHT_USE | ORION_RIGHT_MODIFY |
               ORION_RIGHT_DERIVE | ORION_RIGHT_TRANSFER |
               ORION_RIGHT_REVOKE;
    case ORION_OBJECT_DEVICE:
        return ORION_RIGHT_QUERY | ORION_RIGHT_USE | ORION_RIGHT_MODIFY |
               ORION_RIGHT_ADMIN | ORION_RIGHT_DERIVE |
               ORION_RIGHT_TRANSFER | ORION_RIGHT_REVOKE;
    case ORION_OBJECT_CHANNEL:
        return ORION_RIGHT_QUERY | ORION_RIGHT_DERIVE |
               ORION_RIGHT_TRANSFER | ORION_RIGHT_REVOKE |
               ORION_RIGHT_SEND | ORION_RIGHT_RECEIVE | ORION_RIGHT_CLOSE;
    case ORION_OBJECT_STORAGE:
        return ORION_RIGHT_QUERY | ORION_RIGHT_USE | ORION_RIGHT_MODIFY |
               ORION_RIGHT_ADMIN | ORION_RIGHT_DERIVE |
               ORION_RIGHT_TRANSFER | ORION_RIGHT_REVOKE;
    default:
        return 0;
    }
}

bool orion_object_rights_are_valid(orion_object_type type, uint32_t rights)
{
    const uint32_t allowed = rights_for_type(type);
    return allowed != 0 && rights != 0 && (rights & ~allowed) == 0;
}

static capability_process_table *find_process_table(
    const struct orion_process *process)
{
    if (process == NULL) {
        return NULL;
    }
    for (uint32_t index = 0; index < ORION_CAPABILITY_PROCESS_LIMIT;
         ++index) {
        capability_process_table *table = &process_tables[index];
        if (table->occupied && table->owner == process) {
            return table;
        }
    }
    return NULL;
}

static uint64_t make_handle(uint32_t table_generation, uint32_t slot_index,
                            uint32_t slot_generation)
{
    return ((uint64_t)table_generation << 32) |
           ((uint64_t)slot_generation << 8) | (slot_index + 1U);
}

static capability_slot *find_slot(const struct orion_process *owner,
                                  orion_capability_handle handle,
                                  capability_process_table **table_out)
{
    const uint32_t table_generation = (uint32_t)(handle >> 32);
    const uint32_t slot_generation =
        (uint32_t)((handle >> 8) & CAPABILITY_SLOT_GENERATION_MAX);
    const uint32_t encoded_index =
        (uint32_t)(handle & CAPABILITY_HANDLE_INDEX_MASK);
    if (handle == 0 || encoded_index == 0 ||
        encoded_index > ORION_CAPABILITY_SLOT_LIMIT ||
        slot_generation == 0) {
        return NULL;
    }
    capability_process_table *table = find_process_table(owner);
    if (table == NULL || table->generation != table_generation) {
        return NULL;
    }
    capability_slot *slot = &table->slots[encoded_index - 1U];
    if (!slot->occupied || slot->generation != slot_generation ||
        slot->object_index >= ORION_CAPABILITY_OBJECT_LIMIT) {
        return NULL;
    }
    capability_object *object = &objects[slot->object_index];
    if (!object->occupied || object->revoked ||
        object->generation != slot->object_generation) {
        return NULL;
    }
    if (table_out != NULL) {
        *table_out = table;
    }
    return slot;
}

static capability_slot *allocate_slot(capability_process_table *table,
                                      uint32_t *slot_index)
{
    for (uint32_t index = 0; index < ORION_CAPABILITY_SLOT_LIMIT; ++index) {
        capability_slot *slot = &table->slots[index];
        if (!slot->occupied && !slot->retired) {
            slot->occupied = true;
            *slot_index = index;
            return slot;
        }
    }
    return NULL;
}

static void clear_slot(capability_slot *slot)
{
    slot->occupied = false;
    slot->object_index = 0;
    slot->object_generation = 0;
    slot->rights = 0;
    if (slot->generation >= CAPABILITY_SLOT_GENERATION_MAX) {
        slot->retired = true;
    } else {
        ++slot->generation;
    }
}

static capability_object *allocate_object(uint32_t *object_index)
{
    for (uint32_t index = 0; index < ORION_CAPABILITY_OBJECT_LIMIT; ++index) {
        capability_object *object = &objects[index];
        if (!object->occupied && !object->retired) {
            if (object->generation == 0) {
                object->generation = 1;
            }
            object->occupied = true;
            *object_index = index;
            return object;
        }
    }
    return NULL;
}

static capability_lease_record *allocate_lease(uint32_t *lease_index)
{
    for (uint32_t index = 0; index < ORION_CAPABILITY_LEASE_LIMIT; ++index) {
        capability_lease_record *lease = &leases[index];
        if (!lease->occupied && !lease->retired) {
            if (lease->generation == 0) {
                lease->generation = 1;
            }
            lease->occupied = true;
            *lease_index = index;
            return lease;
        }
    }
    return NULL;
}

static void capture_destructor(capability_object *object,
                               deferred_destructor *deferred)
{
    if (!object->occupied || !object->revoked || object->owner_reference ||
        object->capability_references != 0 || object->lease_references != 0) {
        return;
    }
    deferred->destructor = object->destructor;
    deferred->object = object->object;
    object->occupied = false;
    object->revoked = false;
    object->type = 0;
    object->object = NULL;
    object->destructor = NULL;
    if (object->generation == UINT32_MAX) {
        object->retired = true;
    } else {
        ++object->generation;
    }
}

static void run_destructor(deferred_destructor deferred)
{
    if (deferred.destructor != NULL) {
        deferred.destructor(deferred.object);
    }
}

static bool drop_capability_reference(capability_object *object)
{
    if (object->capability_references == 0) {
        return false;
    }
    --object->capability_references;
    return true;
}

static bool close_all_object_slots(uint32_t object_index,
                                   uint32_t object_generation)
{
    for (uint32_t process_index = 0;
         process_index < ORION_CAPABILITY_PROCESS_LIMIT; ++process_index) {
        capability_process_table *table = &process_tables[process_index];
        if (!table->occupied) {
            continue;
        }
        for (uint32_t slot_index = 0;
             slot_index < ORION_CAPABILITY_SLOT_LIMIT; ++slot_index) {
            capability_slot *slot = &table->slots[slot_index];
            if (slot->occupied && slot->object_index == object_index &&
                slot->object_generation == object_generation) {
                if (!drop_capability_reference(&objects[object_index])) {
                    return false;
                }
                clear_slot(slot);
            }
        }
    }
    return true;
}

bool orion_capability_system_init(void)
{
    if (capability_system_initialized || !interrupts_are_disabled()) {
        return false;
    }
    clear_bytes(process_tables, sizeof(process_tables));
    clear_bytes(objects, sizeof(objects));
    clear_bytes(leases, sizeof(leases));
    capability_system_initialized = true;
    return true;
}

bool orion_capability_process_init(struct orion_process *process)
{
    if (!capability_system_initialized || !interrupts_are_disabled() ||
        process == NULL || process->id == 0 ||
        process->state != ORION_PROCESS_ACTIVE ||
        find_process_table(process) != NULL) {
        return false;
    }
    for (uint32_t index = 0; index < ORION_CAPABILITY_PROCESS_LIMIT; ++index) {
        capability_process_table *table = &process_tables[index];
        if (table->occupied || table->retired) {
            continue;
        }
        if (table->generation == UINT32_MAX) {
            table->retired = true;
            continue;
        }
        ++table->generation;
        for (uint32_t slot_index = 0;
             slot_index < ORION_CAPABILITY_SLOT_LIMIT; ++slot_index) {
            table->slots[slot_index] = (capability_slot){
                .generation = 1
            };
        }
        table->owner = process;
        table->occupied = true;
        return true;
    }
    return false;
}

orion_capability_status orion_capability_process_release(
    struct orion_process *process)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    capability_process_table *table = find_process_table(process);
    if (!capability_system_initialized || table == NULL) {
        orion_x86_64_interrupts_restore(was_enabled);
        return ORION_CAPABILITY_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0; index < ORION_CAPABILITY_LEASE_LIMIT; ++index) {
        if (leases[index].occupied && leases[index].owner == process) {
            orion_x86_64_interrupts_restore(was_enabled);
            return ORION_CAPABILITY_OBJECT_BUSY;
        }
    }

    for (uint32_t index = 0; index < ORION_CAPABILITY_SLOT_LIMIT; ++index) {
        capability_slot *slot = &table->slots[index];
        if (!slot->occupied) {
            continue;
        }
        if (slot->object_index >= ORION_CAPABILITY_OBJECT_LIMIT) {
            orion_x86_64_interrupts_restore(was_enabled);
            return ORION_CAPABILITY_CORRUPTION;
        }
        capability_object *object = &objects[slot->object_index];
        if (!object->occupied ||
            object->generation != slot->object_generation ||
            object->capability_references == 0) {
            orion_x86_64_interrupts_restore(was_enabled);
            return ORION_CAPABILITY_CORRUPTION;
        }
        --object->capability_references;
        clear_slot(slot);
    }
    table->occupied = false;
    table->owner = NULL;
    orion_x86_64_interrupts_restore(was_enabled);
    return ORION_CAPABILITY_OK;
}

bool orion_capability_process_check(const struct orion_process *process)
{
    if (!capability_system_initialized) {
        return false;
    }
    const capability_process_table *table = find_process_table(process);
    if (table == NULL || table->generation == 0) {
        return false;
    }
    for (uint32_t index = 0; index < ORION_CAPABILITY_SLOT_LIMIT; ++index) {
        const capability_slot *slot = &table->slots[index];
        if (!slot->occupied) {
            continue;
        }
        if (slot->generation == 0 ||
            slot->generation > CAPABILITY_SLOT_GENERATION_MAX ||
            slot->object_index >= ORION_CAPABILITY_OBJECT_LIMIT ||
            !orion_object_rights_are_valid(
                objects[slot->object_index].type, slot->rights)) {
            return false;
        }
        const capability_object *object = &objects[slot->object_index];
        if (!object->occupied || object->revoked ||
            object->generation != slot->object_generation) {
            return false;
        }
    }
    return true;
}

static bool capability_check_invariants_disabled(void)
{
    if (!capability_system_initialized) {
        return false;
    }
    uint32_t capability_counts[ORION_CAPABILITY_OBJECT_LIMIT] = {0};
    uint32_t lease_counts[ORION_CAPABILITY_OBJECT_LIMIT] = {0};
    for (uint32_t process_index = 0;
         process_index < ORION_CAPABILITY_PROCESS_LIMIT; ++process_index) {
        const capability_process_table *table =
            &process_tables[process_index];
        if (!table->occupied) {
            continue;
        }
        if (table->owner == NULL || table->generation == 0) {
            return false;
        }
        for (uint32_t slot_index = 0;
             slot_index < ORION_CAPABILITY_SLOT_LIMIT; ++slot_index) {
            const capability_slot *slot = &table->slots[slot_index];
            if (!slot->occupied) {
                continue;
            }
            if (slot->object_index >= ORION_CAPABILITY_OBJECT_LIMIT ||
                slot->generation == 0 ||
                slot->generation > CAPABILITY_SLOT_GENERATION_MAX) {
                return false;
            }
            const capability_object *object = &objects[slot->object_index];
            if (!object->occupied || object->revoked ||
                object->generation != slot->object_generation ||
                !orion_object_rights_are_valid(object->type, slot->rights) ||
                capability_counts[slot->object_index] == UINT32_MAX) {
                return false;
            }
            ++capability_counts[slot->object_index];
        }
    }

    for (uint32_t object_index = 0;
         object_index < ORION_CAPABILITY_OBJECT_LIMIT; ++object_index) {
        const capability_object *object = &objects[object_index];
        if (!object->occupied) {
            continue;
        }
        if (!type_is_valid(object->type) || object->object == NULL ||
            object->generation == 0 ||
            object->capability_references !=
                capability_counts[object_index] ||
            (object->revoked &&
             (object->owner_reference || object->capability_references != 0))) {
            return false;
        }
    }
    for (uint32_t lease_index = 0;
         lease_index < ORION_CAPABILITY_LEASE_LIMIT; ++lease_index) {
        const capability_lease_record *lease = &leases[lease_index];
        if (!lease->occupied) {
            continue;
        }
        if (lease->generation == 0 || lease->owner == NULL ||
            lease->object_index >= ORION_CAPABILITY_OBJECT_LIMIT ||
            !objects[lease->object_index].occupied ||
            objects[lease->object_index].generation !=
                lease->object_generation ||
            lease_counts[lease->object_index] == UINT32_MAX) {
            return false;
        }
        ++lease_counts[lease->object_index];
    }
    for (uint32_t object_index = 0;
         object_index < ORION_CAPABILITY_OBJECT_LIMIT; ++object_index) {
        if (objects[object_index].occupied &&
            lease_counts[object_index] !=
                objects[object_index].lease_references) {
            return false;
        }
    }
    return true;
}

bool orion_capability_check_invariants(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const bool result = capability_check_invariants_disabled();
    orion_x86_64_interrupts_restore(was_enabled);
    return result;
}

orion_capability_status orion_capability_create_kernel_object(
    struct orion_process *owner, orion_object_type type, void *object,
    uint32_t rights, orion_capability_object_destructor destructor,
    orion_capability_handle *handle)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    orion_capability_status status = ORION_CAPABILITY_INVALID_ARGUMENT;
    if (!capability_system_initialized || owner == NULL ||
        owner != orion_process_kernel() ||
        find_process_table(owner) == NULL || !type_is_valid(type) ||
        object == NULL || handle == NULL ||
        !orion_object_rights_are_valid(type, rights)) {
        goto done;
    }

    uint32_t object_index;
    capability_object *object_record = allocate_object(&object_index);
    if (object_record == NULL) {
        status = ORION_CAPABILITY_OBJECT_LIMIT_REACHED;
        goto done;
    }
    uint32_t slot_index;
    capability_process_table *table = find_process_table(owner);
    capability_slot *slot = allocate_slot(table, &slot_index);
    if (slot == NULL) {
        object_record->occupied = false;
        object_record->type = 0;
        status = ORION_CAPABILITY_TABLE_FULL;
        goto done;
    }

    *object_record = (capability_object){
        .generation = object_record->generation,
        .occupied = true,
        .owner_reference = true,
        .type = type,
        .object = object,
        .destructor = destructor,
        .capability_references = 1
    };
    slot->object_index = object_index;
    slot->object_generation = object_record->generation;
    slot->rights = rights;
    *handle = make_handle(table->generation, slot_index, slot->generation);
    status = ORION_CAPABILITY_OK;

done:
    orion_x86_64_interrupts_restore(was_enabled);
    return status;
}

static orion_capability_status insert_derived(
    capability_process_table *target, uint32_t object_index,
    uint32_t object_generation, uint32_t rights,
    orion_capability_handle *handle)
{
    capability_object *object = &objects[object_index];
    if (object->capability_references == UINT32_MAX) {
        return ORION_CAPABILITY_TABLE_FULL;
    }
    uint32_t slot_index;
    capability_slot *slot = allocate_slot(target, &slot_index);
    if (slot == NULL) {
        return ORION_CAPABILITY_TABLE_FULL;
    }
    slot->object_index = object_index;
    slot->object_generation = object_generation;
    slot->rights = rights;
    ++object->capability_references;
    *handle = make_handle(target->generation, slot_index, slot->generation);
    return ORION_CAPABILITY_OK;
}

orion_capability_status orion_capability_derive(
    struct orion_process *owner, orion_capability_handle source,
    uint32_t reduced_rights, orion_capability_handle *derived)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    orion_capability_status status;
    capability_slot *slot = find_slot(owner, source, NULL);
    if (slot == NULL) {
        status = ORION_CAPABILITY_INVALID_HANDLE;
    } else if ((slot->rights & ORION_RIGHT_DERIVE) == 0) {
        status = ORION_CAPABILITY_RIGHTS_DENIED;
    } else if (derived == NULL || reduced_rights == 0 ||
               (reduced_rights & ~slot->rights) != 0) {
        status = ORION_CAPABILITY_RIGHTS_DENIED;
    } else {
        status = insert_derived(find_process_table(owner), slot->object_index,
                                slot->object_generation, reduced_rights,
                                derived);
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return status;
}

orion_capability_status orion_capability_transfer(
    struct orion_process *source_owner, orion_capability_handle source,
    struct orion_process *target_owner, uint32_t reduced_rights,
    orion_capability_handle *transferred)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    orion_capability_status status;
    capability_slot *slot = find_slot(source_owner, source, NULL);
    capability_process_table *target = find_process_table(target_owner);
    if (slot == NULL) {
        status = ORION_CAPABILITY_INVALID_HANDLE;
    } else if (target == NULL || target == find_process_table(source_owner) ||
               transferred == NULL || reduced_rights == 0 ||
               (slot->rights & ORION_RIGHT_TRANSFER) == 0 ||
               (reduced_rights & ~slot->rights) != 0) {
        status = ORION_CAPABILITY_RIGHTS_DENIED;
    } else {
        status = insert_derived(target, slot->object_index,
                                slot->object_generation, reduced_rights,
                                transferred);
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return status;
}

orion_capability_status orion_capability_describe(
    struct orion_process *owner, orion_capability_handle handle,
    orion_capability_info *info)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    orion_capability_status status;
    capability_slot *slot = find_slot(owner, handle, NULL);
    if (slot == NULL) {
        status = ORION_CAPABILITY_INVALID_HANDLE;
    } else if (info == NULL) {
        status = ORION_CAPABILITY_INVALID_ARGUMENT;
    } else if ((slot->rights & ORION_RIGHT_QUERY) == 0) {
        status = ORION_CAPABILITY_RIGHTS_DENIED;
    } else {
        *info = (orion_capability_info){
            .type = objects[slot->object_index].type,
            .rights = slot->rights
        };
        status = ORION_CAPABILITY_OK;
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return status;
}

orion_capability_status orion_capability_acquire(
    struct orion_process *owner, orion_capability_handle handle,
    orion_object_type expected_type, uint32_t required_rights,
    orion_capability_lease *lease)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    orion_capability_status status;
    capability_slot *slot = find_slot(owner, handle, NULL);
    if (slot == NULL) {
        status = ORION_CAPABILITY_INVALID_HANDLE;
        goto done;
    }
    if (lease == NULL || !type_is_valid(expected_type) ||
        required_rights == 0 ||
        (required_rights & ~ORION_RIGHT_ALL) != 0) {
        status = ORION_CAPABILITY_INVALID_ARGUMENT;
        goto done;
    }

    capability_object *object = &objects[slot->object_index];
    if (object->type != expected_type) {
        status = ORION_CAPABILITY_WRONG_TYPE;
        goto done;
    }
    if ((slot->rights & required_rights) != required_rights) {
        status = ORION_CAPABILITY_RIGHTS_DENIED;
        goto done;
    }
    if (object->lease_references == UINT32_MAX) {
        status = ORION_CAPABILITY_TABLE_FULL;
        goto done;
    }
    uint32_t lease_index;
    capability_lease_record *record = allocate_lease(&lease_index);
    if (record == NULL) {
        status = ORION_CAPABILITY_TABLE_FULL;
        goto done;
    }
    record->owner = owner;
    record->object_index = slot->object_index;
    record->object_generation = slot->object_generation;
    ++object->lease_references;
    *lease = (orion_capability_lease){
        .token = ((uint64_t)record->generation << 32) | (lease_index + 1U),
        .owner = owner,
        .object = object->object,
        .active = true
    };
    status = ORION_CAPABILITY_OK;

done:
    orion_x86_64_interrupts_restore(was_enabled);
    return status;
}

orion_capability_status orion_capability_lease_release(
    struct orion_process *owner, orion_capability_lease *lease)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    deferred_destructor deferred = {0};
    orion_capability_status status = ORION_CAPABILITY_STALE_LEASE;
    if (lease == NULL || !lease->active || lease->owner != owner ||
        lease->token == 0) {
        goto done;
    }
    const uint32_t encoded_index = (uint32_t)lease->token;
    const uint32_t generation = (uint32_t)(lease->token >> 32);
    if (encoded_index == 0 ||
        encoded_index > ORION_CAPABILITY_LEASE_LIMIT || generation == 0) {
        goto done;
    }
    capability_lease_record *record = &leases[encoded_index - 1U];
    if (!record->occupied || record->generation != generation ||
        record->owner != owner ||
        record->object_index >= ORION_CAPABILITY_OBJECT_LIMIT) {
        goto done;
    }
    capability_object *object = &objects[record->object_index];
    if (!object->occupied ||
        object->generation != record->object_generation ||
        object->lease_references == 0) {
        status = ORION_CAPABILITY_CORRUPTION;
        goto done;
    }
    --object->lease_references;
    record->occupied = false;
    record->owner = NULL;
    if (record->generation == UINT32_MAX) {
        record->retired = true;
    } else {
        ++record->generation;
    }
    *lease = (orion_capability_lease){0};
    capture_destructor(object, &deferred);
    status = ORION_CAPABILITY_OK;

done:
    orion_x86_64_interrupts_restore(was_enabled);
    run_destructor(deferred);
    return status;
}

orion_capability_status orion_capability_close(
    struct orion_process *owner, orion_capability_handle handle)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    orion_capability_status status;
    capability_slot *slot = find_slot(owner, handle, NULL);
    if (slot == NULL) {
        status = ORION_CAPABILITY_INVALID_HANDLE;
    } else {
        capability_object *object = &objects[slot->object_index];
        if (object->capability_references == 0) {
            status = ORION_CAPABILITY_CORRUPTION;
        } else {
            --object->capability_references;
            clear_slot(slot);
            status = ORION_CAPABILITY_OK;
        }
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return status;
}

orion_capability_status orion_capability_revoke(
    struct orion_process *owner, orion_capability_handle handle)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    deferred_destructor deferred = {0};
    orion_capability_status status;
    capability_slot *slot = find_slot(owner, handle, NULL);
    if (slot == NULL) {
        status = ORION_CAPABILITY_INVALID_HANDLE;
        goto done;
    }
    if ((slot->rights & ORION_RIGHT_REVOKE) == 0) {
        status = ORION_CAPABILITY_RIGHTS_DENIED;
        goto done;
    }

    const uint32_t object_index = slot->object_index;
    capability_object *object = &objects[object_index];
    const uint32_t object_generation = object->generation;
    object->revoked = true;
    object->owner_reference = false;
    if (!close_all_object_slots(object_index, object_generation)) {
        status = ORION_CAPABILITY_CORRUPTION;
        goto done;
    }
    capture_destructor(object, &deferred);
    status = ORION_CAPABILITY_OK;

done:
    orion_x86_64_interrupts_restore(was_enabled);
    run_destructor(deferred);
    return status;
}

const char *orion_capability_status_name(orion_capability_status status)
{
    switch (status) {
    case ORION_CAPABILITY_OK: return "ok";
    case ORION_CAPABILITY_INVALID_ARGUMENT: return "invalid argument";
    case ORION_CAPABILITY_INVALID_HANDLE: return "invalid handle";
    case ORION_CAPABILITY_WRONG_TYPE: return "wrong object type";
    case ORION_CAPABILITY_RIGHTS_DENIED: return "rights denied";
    case ORION_CAPABILITY_TABLE_FULL: return "capability table full";
    case ORION_CAPABILITY_PROCESS_LIMIT_REACHED: return "process table limit";
    case ORION_CAPABILITY_OBJECT_LIMIT_REACHED: return "object table limit";
    case ORION_CAPABILITY_REVOKED: return "object revoked";
    case ORION_CAPABILITY_OBJECT_BUSY: return "object has active leases";
    case ORION_CAPABILITY_STALE_LEASE: return "stale lease";
    case ORION_CAPABILITY_CORRUPTION: return "capability corruption";
    default: return "unknown capability error";
    }
}
