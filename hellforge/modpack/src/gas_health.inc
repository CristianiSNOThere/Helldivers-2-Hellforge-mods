/* Guarded native implementation of Gas Overhaul's player-health multiplier edit.
 * The accepted Lua source addresses one read-only health export through the game build's
 * entity owner map, validates the complete record, then writes exactly one protected scalar. */
#include "gas_health_source_guard.generated.h"

#if defined(HF_PORT_TEST_GAS)
extern int hf_gas_test_health_readonly(uintptr_t address, size_t size);
#endif

typedef struct HfGasPlayerHealthPlan
{
    uintptr_t address;
    uint32_t multiplier_offset;
    int ready;
    int already_applied;
    int applied;
    uint8_t original[HF_GAS_HEALTH_RECORD_SIZE];
} HfGasPlayerHealthPlan;

static HfGasPlayerHealthPlan HF_GAS_PLAYER_HEALTH_PLAN;

static int hf_gas_health_known_baseline(const uint8_t* actual, uint8_t* baseline)
{
    const HfGasHealthProfileField* profiles[] = {HF_GAS_HEALTH_CASUAL, HF_GAS_HEALTH_NORMAL};
    const size_t counts[] = {HF_GAS_HEALTH_CASUAL_COUNT, HF_GAS_HEALTH_NORMAL_COUNT};
    char digest[65];
    size_t profile_index;

    if (hf_gas_sha256(actual, HF_GAS_HEALTH_RECORD_SIZE, digest) &&
        strcmp(digest, HF_GAS_HEALTH_BASELINE_SHA256) == 0)
    {
        memcpy(baseline, actual, HF_GAS_HEALTH_RECORD_SIZE);
        return 1;
    }

    for (profile_index = 0u; profile_index < sizeof(profiles) / sizeof(profiles[0]); ++profile_index)
    {
        size_t i;
        int matched = 1;
        memcpy(baseline, actual, HF_GAS_HEALTH_RECORD_SIZE);
        for (i = 0u; i < counts[profile_index]; ++i)
        {
            const HfGasHealthProfileField* field = &profiles[profile_index][i];
            if (field->offset > HF_GAS_HEALTH_RECORD_SIZE - sizeof(field->observed) ||
                memcmp(actual + field->offset, field->observed, sizeof(field->observed)) != 0)
            {
                matched = 0;
                break;
            }
            memcpy(baseline + field->offset, field->baseline, sizeof(field->baseline));
        }
        if (matched && hf_gas_sha256(baseline, HF_GAS_HEALTH_RECORD_SIZE, digest) &&
            strcmp(digest, HF_GAS_HEALTH_BASELINE_SHA256) == 0)
            return 1;
    }
    return 0;
}

static int hf_gas_health_data_readonly(uintptr_t address, size_t size)
{
#if defined(HF_PORT_TEST_GAS)
    return hf_gas_test_health_readonly(address, size);
#else
    uintptr_t cursor = address;
    size_t remaining = size;
    if (address < (uintptr_t)0x10000u || size == 0u || address > UINTPTR_MAX - size)
        return 0;
    while (remaining != 0u)
    {
        MEMORY_BASIC_INFORMATION info;
        SIZE_T queried = VirtualQuery((LPCVOID)cursor, &info, sizeof(info));
        uintptr_t region_base;
        uintptr_t region_size;
        uintptr_t region_end;
        size_t take;
        if (queried != sizeof(info))
            return 0;
        region_base = (uintptr_t)info.BaseAddress;
        region_size = (uintptr_t)info.RegionSize;
        if (info.State != MEM_COMMIT || info.Type != MEM_PRIVATE || info.Protect != PAGE_READONLY ||
            region_size == 0u || region_base > UINTPTR_MAX - region_size)
            return 0;
        region_end = region_base + region_size;
        if (cursor < region_base || cursor >= region_end)
            return 0;
        take = remaining < (size_t)(region_end - cursor) ? remaining : (size_t)(region_end - cursor);
        cursor += take;
        remaining -= take;
    }
    return 1;
#endif
}

static int hf_gas_health_resolve(const HfApi* api, uintptr_t* record_address,
                                 char* reason, size_t capacity)
{
    HfGasRunner reader;
    uintptr_t game_base, owner_slot, owner, map_slot, entities, address;
    uintptr_t map_pointer;
    uint64_t map_value;
    size_t image_size = 0u;

    if (api == NULL || api->module_base == NULL || api->read == NULL || record_address == NULL)
    {
        (void)snprintf(reason, capacity, "HellForge raw-memory or module APIs are unavailable");
        return 0;
    }
    memset(&reader, 0, sizeof(reader));
    reader.api = api;
    game_base = api->module_base("game.dll", &image_size);
    if (game_base == 0u || image_size == 0u ||
        (size_t)HF_GAS_HEALTH_OWNER_RVA > image_size ||
        sizeof(uint64_t) > image_size - (size_t)HF_GAS_HEALTH_OWNER_RVA ||
        !hf_gas_add_address(game_base, (uintptr_t)HF_GAS_HEALTH_OWNER_RVA, &owner_slot) ||
        !hf_gas_read_pointer(&reader, owner_slot, &owner) ||
        !hf_gas_add_address(owner, (uintptr_t)HF_GAS_HEALTH_ENTITY_MAP_OFFSET, &map_slot) ||
        !hf_gas_read_pointer(&reader, map_slot, &map_pointer) ||
        (uint64_t)map_pointer < HF_GAS_HEALTH_ENTITY_MAP_BIAS)
    {
        (void)snprintf(reason, capacity, "the accepted Gas player-health owner-map pointer is unavailable or invalid");
        return 0;
    }
    map_value = (uint64_t)map_pointer - HF_GAS_HEALTH_ENTITY_MAP_BIAS;
    if (map_value < UINT64_C(0x10000) || map_value >= UINT64_C(0x800000000000) ||
        map_value > UINTPTR_MAX)
    {
        (void)snprintf(reason, capacity, "the accepted Gas player-health entity base is invalid");
        return 0;
    }
    entities = (uintptr_t)map_value;
    if (!hf_gas_add_address(entities, (uintptr_t)HF_GAS_HEALTH_RECORD_OFFSET, &address) ||
        address >= (uintptr_t)UINT64_C(0x800000000000) ||
        address > UINTPTR_MAX - HF_GAS_HEALTH_RECORD_SIZE)
    {
        (void)snprintf(reason, capacity, "the accepted Helldiver health record address is invalid");
        return 0;
    }
    *record_address = address;
    return 1;
}

static int hf_gas_health_find_multiplier(const uint8_t* bytes, uint32_t* offset,
                                         char* reason, size_t capacity)
{
    uint32_t found = UINT32_MAX;
    uint32_t i;
    for (i = 0u; i < 4u; ++i)
    {
        uint32_t element = HF_GAS_HEALTH_MULTIPLIER_OFFSET + i * 8u;
        uint32_t id = (uint32_t)bytes[element] | ((uint32_t)bytes[element + 1u] << 8) |
                      ((uint32_t)bytes[element + 2u] << 16) | ((uint32_t)bytes[element + 3u] << 24);
        if (id == HF_GAS_HEALTH_ELEMENT_ID)
        {
            if (found != UINT32_MAX)
            {
                (void)snprintf(reason, capacity, "duplicate Gas element in Helldiver health record");
                return 0;
            }
            found = element + 4u;
        }
    }
    if (found == UINT32_MAX)
    {
        (void)snprintf(reason, capacity, "Helldiver Gas damage multiplier mismatch");
        return 0;
    }
    *offset = found;
    return 1;
}

static int hf_gas_player_health_preflight(const HfApi* api, const HfPortEdit* edit,
                                          char* reason, size_t capacity)
{
    uint8_t baseline[HF_GAS_HEALTH_RECORD_SIZE];
    uint8_t applied_candidate[HF_GAS_HEALTH_RECORD_SIZE];
    uint8_t bytes[HF_GAS_HEALTH_RECORD_SIZE];
    uintptr_t address;
    uint32_t multiplier_offset;
    int baseline_matches;

    memset(&HF_GAS_PLAYER_HEALTH_PLAN, 0, sizeof(HF_GAS_PLAYER_HEALTH_PLAN));
    if (reason != NULL && capacity != 0u)
        reason[0] = '\0';
    if (edit == NULL || edit->option == NULL || strcmp(edit->option, "gas-overhaul") != 0 ||
        edit->value.type != HF_TYPE_F32 || (float)edit->value.number != 0.55f ||
        edit->expected_base.type != HF_TYPE_F32 || (float)edit->expected_base.number != 1.3f ||
        edit->has_expected_base == 0u)
    {
        (void)snprintf(reason, capacity, "the accepted Gas player-health edit intent changed");
        return 0;
    }
    if (!hf_gas_health_resolve(api, &address, reason, capacity))
        return 0;
    if (!hf_gas_health_data_readonly(address, HF_GAS_HEALTH_RECORD_SIZE))
    {
        (void)snprintf(reason, capacity, "Helldiver health record permissions are unsupported or writable");
        return 0;
    }
    if (api->read(address, bytes, sizeof(bytes)) != 0)
    {
        (void)snprintf(reason, capacity, "Helldiver health record size or read access is invalid");
        return 0;
    }
    if (!hf_gas_health_find_multiplier(bytes, &multiplier_offset, reason, capacity))
        return 0;

    baseline_matches = hf_gas_health_known_baseline(bytes, baseline);
    if (!baseline_matches &&
        memcmp(bytes + multiplier_offset, HF_GAS_HEALTH_PLAYER_MULTIPLIER,
               sizeof(HF_GAS_HEALTH_PLAYER_MULTIPLIER)) == 0)
    {
        memcpy(applied_candidate, bytes, sizeof(applied_candidate));
        memcpy(applied_candidate + multiplier_offset, HF_GAS_HEALTH_BASE_MULTIPLIER,
               sizeof(HF_GAS_HEALTH_BASE_MULTIPLIER));
        baseline_matches = hf_gas_health_known_baseline(applied_candidate, baseline);
        if (baseline_matches)
            HF_GAS_PLAYER_HEALTH_PLAN.already_applied = 1;
    }
    if (!baseline_matches)
    {
        (void)snprintf(reason, capacity, "unsupported Helldiver health profile or checksum mismatch");
        return 0;
    }
    if (memcmp(baseline, HF_GAS_HEALTH_SIGNATURE, sizeof(HF_GAS_HEALTH_SIGNATURE)) != 0)
    {
        (void)snprintf(reason, capacity, "Helldiver health baseline signature mismatch");
        return 0;
    }
    if (!HF_GAS_PLAYER_HEALTH_PLAN.already_applied &&
        memcmp(bytes + multiplier_offset, HF_GAS_HEALTH_BASE_MULTIPLIER,
               sizeof(HF_GAS_HEALTH_BASE_MULTIPLIER)) != 0)
    {
        (void)snprintf(reason, capacity, "Helldiver Gas damage multiplier mismatch");
        return 0;
    }

    HF_GAS_PLAYER_HEALTH_PLAN.address = address;
    HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset = multiplier_offset;
    HF_GAS_PLAYER_HEALTH_PLAN.ready = 1;
    memcpy(HF_GAS_PLAYER_HEALTH_PLAN.original, bytes, sizeof(bytes));
    return 1;
}

static int hf_gas_player_health_apply(const HfApi* api, char* reason, size_t capacity)
{
    uint8_t actual[HF_GAS_HEALTH_RECORD_SIZE];
    uint8_t expected[HF_GAS_HEALTH_RECORD_SIZE];
    uint8_t restored[HF_GAS_HEALTH_RECORD_SIZE];
    uintptr_t current_address;
    int write_result;

    if (!HF_GAS_PLAYER_HEALTH_PLAN.ready || api == NULL || api->read == NULL || api->write == NULL ||
        !hf_gas_health_resolve(api, &current_address, reason, capacity) ||
        current_address != HF_GAS_PLAYER_HEALTH_PLAN.address ||
        !hf_gas_health_data_readonly(current_address, HF_GAS_HEALTH_RECORD_SIZE) ||
        api->read(current_address, actual, sizeof(actual)) != 0 ||
        memcmp(actual, HF_GAS_PLAYER_HEALTH_PLAN.original, sizeof(actual)) != 0)
    {
        if (reason[0] == '\0')
            (void)snprintf(reason, capacity, "Helldiver health record changed after preflight");
        return 0;
    }
    if (HF_GAS_PLAYER_HEALTH_PLAN.already_applied)
    {
        HF_GAS_PLAYER_HEALTH_PLAN.applied = 1;
        return 1;
    }

    memcpy(expected, HF_GAS_PLAYER_HEALTH_PLAN.original, sizeof(expected));
    memcpy(expected + HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset, HF_GAS_HEALTH_PLAYER_MULTIPLIER,
           sizeof(HF_GAS_HEALTH_PLAYER_MULTIPLIER));
    write_result = api->write(current_address + HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset,
                              HF_GAS_HEALTH_PLAYER_MULTIPLIER, sizeof(HF_GAS_HEALTH_PLAYER_MULTIPLIER));
    if (api->read(current_address, actual, sizeof(actual)) == 0 && memcmp(actual, expected, sizeof(actual)) == 0)
    {
        HF_GAS_PLAYER_HEALTH_PLAN.applied = 1;
        return 1;
    }

    (void)write_result;
    if (api->read(current_address, actual, sizeof(actual)) == 0 &&
        memcmp(actual, HF_GAS_PLAYER_HEALTH_PLAN.original, sizeof(actual)) == 0)
    {
        (void)snprintf(reason, capacity, "HellForge refused the guarded Helldiver Gas multiplier write");
        return 0;
    }

    if (api->read(current_address, actual, sizeof(actual)) == 0 &&
        memcmp(actual, HF_GAS_PLAYER_HEALTH_PLAN.original, HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset) == 0 &&
        memcmp(actual + HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset + sizeof(HF_GAS_HEALTH_PLAYER_MULTIPLIER),
               HF_GAS_PLAYER_HEALTH_PLAN.original + HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset +
                   sizeof(HF_GAS_HEALTH_PLAYER_MULTIPLIER),
               sizeof(actual) - HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset -
                   sizeof(HF_GAS_HEALTH_PLAYER_MULTIPLIER)) == 0)
    {
        if (api->write(current_address + HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset,
                       HF_GAS_PLAYER_HEALTH_PLAN.original + HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset,
                       sizeof(HF_GAS_HEALTH_PLAYER_MULTIPLIER)) == 0 &&
            api->read(current_address, restored, sizeof(restored)) == 0 &&
            memcmp(restored, HF_GAS_PLAYER_HEALTH_PLAN.original, sizeof(restored)) == 0)
        {
            (void)snprintf(reason, capacity, "Helldiver Gas multiplier write failed and was rolled back");
            return 0;
        }
    }
    (void)snprintf(reason, capacity, "FATAL: Helldiver Gas multiplier write could not be verified or safely restored");
    return -1;
}

static int hf_gas_player_health_reset(const HfApi* api, char* reason, size_t capacity)
{
    uint8_t actual[HF_GAS_HEALTH_RECORD_SIZE];
    uint8_t expected[HF_GAS_HEALTH_RECORD_SIZE];
    uint8_t restored[HF_GAS_HEALTH_RECORD_SIZE];
    uintptr_t current_address;
    int write_result;

    if (!HF_GAS_PLAYER_HEALTH_PLAN.applied || HF_GAS_PLAYER_HEALTH_PLAN.already_applied)
        return 1;
    if (api == NULL || api->read == NULL || api->write == NULL ||
        !hf_gas_health_resolve(api, &current_address, reason, capacity) ||
        current_address != HF_GAS_PLAYER_HEALTH_PLAN.address ||
        !hf_gas_health_data_readonly(current_address, HF_GAS_HEALTH_RECORD_SIZE) ||
        api->read(current_address, actual, sizeof(actual)) != 0)
    {
        if (reason[0] == '\0')
            (void)snprintf(reason, capacity, "Helldiver health record is unavailable during rollback");
        return 0;
    }
    memcpy(expected, HF_GAS_PLAYER_HEALTH_PLAN.original, sizeof(expected));
    memcpy(expected + HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset, HF_GAS_HEALTH_PLAYER_MULTIPLIER,
           sizeof(HF_GAS_HEALTH_PLAYER_MULTIPLIER));
    if (memcmp(actual, expected, sizeof(actual)) != 0)
    {
        (void)snprintf(reason, capacity, "Helldiver health record changed before rollback");
        return 0;
    }
    write_result = api->write(current_address + HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset,
                              HF_GAS_PLAYER_HEALTH_PLAN.original + HF_GAS_PLAYER_HEALTH_PLAN.multiplier_offset,
                              sizeof(HF_GAS_HEALTH_PLAYER_MULTIPLIER));
    if (api->read(current_address, restored, sizeof(restored)) != 0 ||
        memcmp(restored, HF_GAS_PLAYER_HEALTH_PLAN.original, sizeof(restored)) != 0)
    {
        (void)write_result;
        (void)snprintf(reason, capacity, "FATAL: Helldiver health record could not be restored");
        return 0;
    }
    HF_GAS_PLAYER_HEALTH_PLAN.applied = 0;
    return 1;
}
