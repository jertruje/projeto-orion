#include <orion/boot_info.h>
#include <orion/init_state.h>

static uint64_t aligned_map[10];

static int expect(bool condition, const char *name)
{
    if (!condition) {
        (void)name;
        return 1;
    }
    return 0;
}

int main(void)
{
    orion_boot_info info = {
        ORION_BOOT_INFO_SIGNATURE,
        ORION_BOOT_INFO_VERSION,
        0,
        aligned_map,
        sizeof(aligned_map),
        40
    };
    int failures = 0;
    failures += expect(orion_boot_info_is_valid(&info), "valid");
    failures += expect(!orion_boot_info_is_valid(0), "null info");

    info.signature ^= 1;
    failures += expect(!orion_boot_info_is_valid(&info), "signature");
    info.signature = ORION_BOOT_INFO_SIGNATURE;

    info.version++;
    failures += expect(!orion_boot_info_is_valid(&info), "version");
    info.version = ORION_BOOT_INFO_VERSION;

    info.memory_map = 0;
    failures += expect(!orion_boot_info_is_valid(&info), "null map");
    info.memory_map = aligned_map;
    info.memory_map = (const void *)((uintptr_t)aligned_map + 1);
    failures += expect(!orion_boot_info_is_valid(&info), "unaligned map");
    info.memory_map = aligned_map;

    info.descriptor_size = 0;
    failures += expect(!orion_boot_info_is_valid(&info), "zero descriptor");
    info.descriptor_size = 40;

    info.memory_map_size = 39;
    failures += expect(!orion_boot_info_is_valid(&info), "short map");
    info.memory_map_size = 41;
    failures += expect(!orion_boot_info_is_valid(&info), "partial descriptor");
    info.memory_map_size = sizeof(aligned_map);

    info.descriptor_size = 39;
    failures += expect(!orion_boot_info_is_valid(&info), "short descriptor");

    orion_init_stage stage = ORION_INIT_ENTRY;
    failures += expect(orion_init_advance(&stage, ORION_INIT_ENTRY,
                                          ORION_INIT_DIAGNOSTICS_CONFIGURED),
                       "entry to diagnostics transition");
    failures += expect(stage == ORION_INIT_DIAGNOSTICS_CONFIGURED,
                       "diagnostics transition state");
    failures += expect(orion_init_advance(
                           &stage, ORION_INIT_DIAGNOSTICS_CONFIGURED,
                           ORION_INIT_BOOT_INFO_VALID),
                       "diagnostics to boot-info transition");
    failures += expect(orion_init_advance(
                           &stage, ORION_INIT_BOOT_INFO_VALID,
                           ORION_INIT_CPU_STATE_VALID),
                       "boot-info to CPU-state transition");
    failures += expect(orion_init_advance(
                           &stage, ORION_INIT_CPU_STATE_VALID,
                           ORION_INIT_ARCH_TABLES_READY),
                       "CPU-state to architecture-tables transition");
    failures += expect(orion_init_advance(
                           &stage, ORION_INIT_ARCH_TABLES_READY,
                           ORION_INIT_CORE_READY),
                       "architecture-tables to core-ready transition");
    failures += expect(orion_init_advance(&stage, ORION_INIT_CORE_READY,
                                          ORION_INIT_IDLE),
                       "core-ready to idle transition");
    failures += expect(stage == ORION_INIT_IDLE, "final transition state");
    failures += expect(!orion_init_advance(&stage, ORION_INIT_CORE_READY,
                                           ORION_INIT_IDLE),
                       "duplicate init transition");
    failures += expect(!orion_init_advance(
                           &stage, ORION_INIT_STAGE_COUNT, ORION_INIT_IDLE),
                       "invalid expected stage");
    failures += expect(stage == ORION_INIT_IDLE,
                       "invalid transition preserves state");
    failures += expect(!orion_init_advance(
                           &stage, ORION_INIT_IDLE, ORION_INIT_STAGE_COUNT),
                       "invalid next stage");
    failures += expect(!orion_init_stage_is_valid(
                           (orion_init_stage)-1),
                       "negative stage");
    failures += expect(!orion_init_stage_is_valid(ORION_INIT_STAGE_COUNT),
                       "stage sentinel");
    failures += expect(!orion_init_advance(0, ORION_INIT_ENTRY,
                                           ORION_INIT_DIAGNOSTICS_CONFIGURED),
                       "null init state");
    failures += expect(orion_init_stage_name(ORION_INIT_IDLE) != 0 &&
                           orion_init_stage_name(
                               ORION_INIT_STAGE_COUNT)[0] == 'I',
                       "stage names");

    return failures == 0 ? 0 : 1;
}
