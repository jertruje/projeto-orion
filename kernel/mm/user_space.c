#include "user_space.h"

#include "../arch/x86_64/cpu_state.h"
#include "../arch/x86_64/interrupt_state.h"
#include "../log/log.h"
#include "physical.h"

#define CR3_ROOT_MASK UINT64_C(0x000ffffffffff000)
#define USER_PML4_INDEX 2U
#define PAGE_MASK UINT64_C(0xfff)

static bool allocate_table(void *context, orion_physical_address *physical,
                           uint64_t **virtual_table)
{
    (void)context;
    if (physical == NULL || virtual_table == NULL ||
        !orion_physical_allocate(1, &physical->value)) {
        return false;
    }
    if (physical->value >= ORION_PHYSICAL_MAX_ADDRESS) {
        (void)orion_physical_release(physical->value);
        return false;
    }
    *virtual_table = (uint64_t *)(uintptr_t)physical->value;
    for (uint32_t index = 0; index < ORION_X86_64_PAGE_TABLE_ENTRIES;
         ++index) {
        (*virtual_table)[index] = 0;
    }
    return true;
}

static bool resolve_table(void *context, orion_physical_address physical,
                          uint64_t **virtual_table)
{
    (void)context;
    if (virtual_table == NULL || physical.value >= ORION_PHYSICAL_MAX_ADDRESS ||
        (physical.value & PAGE_MASK) != 0 ||
        !orion_physical_is_allocated(physical.value)) {
        return false;
    }
    *virtual_table = (uint64_t *)(uintptr_t)physical.value;
    return true;
}

static bool owns_table(void *context, orion_physical_address physical)
{
    (void)context;
    return physical.value < ORION_PHYSICAL_MAX_ADDRESS &&
           (physical.value & PAGE_MASK) == 0 &&
           orion_physical_is_allocated(physical.value);
}

static void release_table(void *context, orion_physical_address physical,
                          uint64_t *virtual_table)
{
    (void)context;
    (void)virtual_table;
    (void)orion_physical_release(physical.value);
}

static void invalidate_page(void *context, orion_virtual_address address)
{
    (void)context;
    __asm__ volatile("invlpg (%0)" : : "r"(address.value) : "memory");
}

bool orion_user_space_create(orion_user_address_space *space)
{
    if (space == NULL || space->initialized ||
        orion_x86_64_interrupts_enabled() ||
        !orion_x86_64_uses_four_level_paging() ||
        !orion_x86_64_nx_enabled()) {
        (void)orion_log(ORION_LOG_ERROR, "USER",
                        "User address-space prerequisites were not met");
        return false;
    }
    uint64_t root_physical = 0;
    if (!orion_physical_allocate(1, &root_physical) ||
        root_physical >= ORION_PHYSICAL_MAX_ADDRESS) {
        if (root_physical != 0) {
            (void)orion_physical_release(root_physical);
        }
        (void)orion_log(ORION_LOG_ERROR, "USER",
                        "Could not allocate the user page-table root");
        return false;
    }
    uint64_t *root = (uint64_t *)(uintptr_t)root_physical;
    const uint64_t inherited_cr3 = orion_x86_64_read_cr3();
    const uint64_t inherited_root_physical =
        inherited_cr3 & CR3_ROOT_MASK;
    if (inherited_root_physical == 0 ||
        inherited_root_physical >= ORION_PHYSICAL_MAX_ADDRESS ||
        (inherited_root_physical & PAGE_MASK) != 0) {
        (void)orion_physical_release(root_physical);
        (void)orion_log(ORION_LOG_ERROR, "USER",
                        "Inherited kernel page-table root is invalid");
        return false;
    }
    const uint64_t *inherited_root =
        (const uint64_t *)(uintptr_t)inherited_root_physical;
    for (uint32_t index = 0; index < ORION_X86_64_PAGE_TABLE_ENTRIES;
         ++index) {
        root[index] = inherited_root[index];
    }
    if (root[USER_PML4_INDEX] != 0) {
        (void)orion_log_u64(ORION_LOG_ERROR, "USER",
                            "User PML4 slot is occupied:",
                            root[USER_PML4_INDEX]);
        (void)orion_physical_release(root_physical);
        return false;
    }

    const orion_page_table_ops operations = {
        .context = NULL,
        .nx_enabled = true,
        .allocate_table = allocate_table,
        .resolve_table = resolve_table,
        .owns_table = owns_table,
        .release_table = release_table,
        .invalidate_page = invalidate_page
    };
    if (!orion_x86_64_page_tables_init(
            &space->tables, root,
            (orion_physical_address){root_physical}, &operations)) {
        (void)orion_physical_release(root_physical);
    (void)orion_log(ORION_LOG_ERROR, "USER",
                    "Could not initialize user page-table metadata");
    return false;
    }
    space->root_physical = (orion_physical_address){root_physical};
    space->initialized = true;
    return true;
}

bool orion_user_space_map(orion_user_address_space *space,
                          orion_virtual_address address,
                          orion_physical_address frame, uint32_t flags)
{
    if (space == NULL || !space->initialized ||
        address.value < ORION_USER_BASE ||
        address.value >= ORION_USER_STACK_TOP ||
        (flags & ORION_VM_USER) == 0 ||
        !orion_physical_is_allocated(frame.value)) {
        return false;
    }
    return orion_x86_64_page_map(&space->tables, address, frame, flags);
}

bool orion_user_space_unmap(orion_user_address_space *space,
                            orion_virtual_address address,
                            orion_physical_address *frame)
{
    return space != NULL && space->initialized && frame != NULL &&
           address.value >= ORION_USER_BASE &&
           address.value < ORION_USER_STACK_TOP &&
           orion_x86_64_page_unmap(&space->tables, address, frame);
}

bool orion_user_space_check(const orion_user_address_space *space,
                            uint64_t address, uint64_t length,
                            uint32_t required_flags)
{
    if (space == NULL || !space->initialized ||
        (required_flags & (ORION_VM_READ | ORION_VM_WRITE)) == 0) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (address < ORION_USER_BASE || address >= ORION_USER_STACK_TOP ||
        length > ORION_USER_STACK_TOP - address) {
        return false;
    }
    const uint64_t end = address + length;
    uint64_t page_address = address & ~PAGE_MASK;
    while (page_address < end) {
        orion_physical_address frame = {0};
        uint32_t flags = 0;
        if (!orion_x86_64_page_lookup(
                (orion_page_table_space *)&space->tables,
                (orion_virtual_address){page_address}, &frame, &flags) ||
            (flags & ORION_VM_USER) == 0 ||
            (flags & required_flags) != required_flags) {
            return false;
        }
        page_address += ORION_MEMORY_PAGE_SIZE;
    }
    return true;
}

bool orion_user_space_destroy(orion_user_address_space *space)
{
    if (space == NULL || !space->initialized ||
        (orion_x86_64_read_cr3() & CR3_ROOT_MASK) ==
            space->root_physical.value) {
        return false;
    }
    uint64_t *root = space->tables.root_table;
    if (root == NULL || root[USER_PML4_INDEX] != 0 ||
        !orion_physical_release(space->root_physical.value)) {
        return false;
    }
    *space = (orion_user_address_space){0};
    return true;
}
