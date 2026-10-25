/* Transactional Sickle v0.1.2 native profile port. Included by native_pack.c. */
#include "sickle_source_guard.generated.h"

typedef enum HfSickleResult
{
    HF_SICKLE_FATAL = -1,
    HF_SICKLE_REFUSED = 0,
    HF_SICKLE_APPLIED = 1,
    HF_SICKLE_RETRY_NOT_READY = 2,
    HF_SICKLE_RETRY_RACE = 3
} HfSickleResult;

typedef struct HfSickleOps
{
    void* context;
    const HfApi* sdk;
    int (*read)(void* context, uintptr_t address, void* output, size_t size);
    int (*data_access)(void* context, uintptr_t address, size_t size, int writable);
    int (*write_component)(void* context, uintptr_t address, const void* input, size_t size);
    int (*registry_access)(void* context, uintptr_t game, uintptr_t slot);
    uintptr_t (*owned_copy)(void* context, const void* bytes, size_t size);
    int (*publish)(void* context, uintptr_t slot, uintptr_t expected, uintptr_t replacement);
    int (*sha256)(void* context, const void* bytes, size_t size, char output[65]);
    int (*verify_modules)(void* context, uintptr_t executable, uintptr_t game);
} HfSickleOps;

typedef struct HfSickleReadContext
{
    const HfApi* api;
} HfSickleReadContext;

/* Keep the SDK convention: zero means success, all other values mean failure.
 * Native contexts embed this as their first member for the shared callback. */
static int sickle_native_read(void* context, uintptr_t address, void* output, size_t size)
{
    HfSickleReadContext* reader = (HfSickleReadContext*)context;
    if (reader == NULL || reader->api == NULL || reader->api->read == NULL)
        return -1;
    return reader->api->read(address, output, size);
}

typedef struct HfSickleComponentPlan
{
    const HfSickleComponentSpec* spec;
    uintptr_t row;
    uint8_t owners[730u * 16u];
    uint8_t source[1232u];
    uint8_t expected[1232u];
} HfSickleComponentPlan;

typedef struct HfSickleRegistryPlan
{
    const HfSickleRegistrySpec* spec;
    uintptr_t slot;
    uintptr_t row;
    uintptr_t owned;
    uint8_t after[272u];
} HfSickleRegistryPlan;

typedef struct HfSickleComponentAttempt
{
    size_t plan_index;
    uintptr_t row;
    uint32_t offset;
    uint8_t size;
    uint8_t before[8];
    uint8_t after[8];
    uint8_t source[1232];
} HfSickleComponentAttempt;

static int sickle_add_address(uintptr_t base, uint64_t offset, uintptr_t* output)
{
    if (output == NULL || offset > (uint64_t)(UINTPTR_MAX - base))
        return 0;
    *output = base + (uintptr_t)offset;
    return *output >= (uintptr_t)65536u && *output < (uintptr_t)UINT64_C(0x800000000000);
}

static uint32_t sickle_u32(const uint8_t* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uintptr_t sickle_pointer(const uint8_t bytes[8])
{
    uint64_t value = 0u;
    size_t i;
    for (i = 0u; i < 8u; ++i)
        value |= (uint64_t)bytes[i] << (i * 8u);
    if (value < UINT64_C(65536) || value >= UINT64_C(0x800000000000))
        return 0u;
    return (uintptr_t)value;
}

static int sickle_read(const HfSickleOps* ops, uintptr_t address, void* output, size_t size)
{
    return ops != NULL && ops->read != NULL && address != 0u && output != NULL && size != 0u &&
           ops->read(ops->context, address, output, size) == 0;
}

static int sickle_hash_matches(const HfSickleOps* ops, const uint8_t* data, size_t size,
                              const char* expected)
{
    char actual[65];
    if (ops == NULL || ops->sha256 == NULL || expected == NULL ||
        ops->sha256(ops->context, data, size, actual) == 0)
        return 0;
    actual[64] = '\0';
    return strcmp(actual, expected) == 0;
}

static uint64_t sickle_retry_delay_ms(HfSickleResult result, unsigned attempts)
{
    if (attempts >= 8u)
        return 0u;
    if (result == HF_SICKLE_RETRY_RACE)
        return 3000u;
    if (result == HF_SICKLE_RETRY_NOT_READY)
        return 10000u;
    return 0u;
}

static int sickle_component_proof(const HfSickleOps* ops, uintptr_t game,
                                  uintptr_t owner, uintptr_t map, uintptr_t entities,
                                  const HfSickleComponentPlan* plan)
{
    uint8_t pointer_bytes[8];
    uint8_t header[28];
    uint8_t owners[730u * 16u];
    uintptr_t group;
    uintptr_t current_owner;
    uintptr_t current_map;
    uintptr_t current_entities;
    const HfSickleComponentSpec* spec;

    if (plan == NULL || plan->spec == NULL)
        return 0;
    spec = plan->spec;
    if (!sickle_read(ops, game + 0x346BF98u, pointer_bytes, sizeof(pointer_bytes)))
        return 0;
    current_owner = sickle_pointer(pointer_bytes);
    if (current_owner != owner || !sickle_add_address(current_owner, 0xF12BD8u, &group) ||
        !sickle_read(ops, group, pointer_bytes, sizeof(pointer_bytes)))
        return 0;
    current_map = sickle_pointer(pointer_bytes);
    if (current_map != map || current_map < 26151684u)
        return 0;
    current_entities = current_map - 26151684u;
    if (current_entities != entities || !sickle_add_address(entities, spec->group, &group) ||
        !sickle_read(ops, group, header, sizeof(header)) ||
        memcmp(header, spec->header, sizeof(header)) != 0 ||
        !sickle_read(ops, group + 28u, owners, (size_t)spec->count * 16u) ||
        memcmp(owners, plan->owners, (size_t)spec->count * 16u) != 0)
        return 0;
    return 1;
}

static int sickle_resolve_component(const HfSickleOps* ops, uintptr_t game,
                                    uintptr_t owner, uintptr_t map, uintptr_t entities,
                                    const HfSickleComponentSpec* spec,
                                    HfSickleComponentPlan* plan,
                                    HfSickleResult* retry, char* reason, size_t capacity)
{
    uint8_t header[28];
    uintptr_t group;
    uintptr_t row;
    size_t i;
    size_t hits = 0u;
    size_t aliases = 0u;
    size_t owners_size;
    uint64_t row_offset;

    memset(plan, 0, sizeof(*plan));
    plan->spec = spec;
    if (!sickle_add_address(entities, spec->group, &group) ||
        !sickle_read(ops, group, header, sizeof(header)) ||
        memcmp(header, spec->header, sizeof(header)) != 0)
    {
        (void)snprintf(reason, capacity, "Sickle %s group header changed", spec->label);
        return 0;
    }
    owners_size = (size_t)spec->count * 16u;
    if (owners_size > sizeof(plan->owners) ||
        !sickle_read(ops, group + 28u, plan->owners, owners_size))
    {
        *retry = HF_SICKLE_RETRY_NOT_READY;
        (void)snprintf(reason, capacity, "Sickle %s owner map is not ready", spec->label);
        return 0;
    }
    for (i = 0u; i < spec->count; ++i)
    {
        const uint8_t* entry = plan->owners + i * 16u;
        uint32_t index = sickle_u32(entry + 8u);
        uint32_t flags = sickle_u32(entry + 12u);
        if (memcmp(entry, spec->owner, 8u) == 0)
        {
            if (index != spec->index || flags != 0u)
            {
                (void)snprintf(reason, capacity, "Sickle %s owner index changed", spec->label);
                return 0;
            }
            ++hits;
        }
        if (index == spec->index && memcmp(entry, "\0\0\0\0\0\0\0\0", 8u) != 0)
            ++aliases;
    }
    if (hits == 0u)
    {
        *retry = HF_SICKLE_RETRY_NOT_READY;
        (void)snprintf(reason, capacity, "Expected one supported Sickle %s row; found 0", spec->label);
        return 0;
    }
    if (hits != 1u || aliases != 1u)
    {
        (void)snprintf(reason, capacity, "Sickle %s component ownership is not unique", spec->label);
        return 0;
    }
    row_offset = 28u + owners_size + (uint64_t)spec->index * spec->stride;
    if (!sickle_add_address(group, row_offset, &row) || spec->stride > sizeof(plan->source) ||
        ops->data_access == NULL || !ops->data_access(ops->context, row, spec->stride, 0) ||
        !sickle_read(ops, row, plan->source, spec->stride))
    {
        (void)snprintf(reason, capacity, "Sickle %s component row is unreadable or unsupported", spec->label);
        return 0;
    }
    if (!sickle_hash_matches(ops, plan->source, spec->stride, spec->sha256))
    {
        (void)snprintf(reason, capacity, "Sickle %s source row fingerprint changed", spec->label);
        return 0;
    }
    if (spec->edit_size > 0u &&
        (spec->edit_size > sizeof(spec->edit) ||
         spec->edit_offset + spec->edit_size > spec->stride))
    {
        (void)snprintf(reason, capacity, "Sickle %s edit lies outside its row", spec->label);
        return 0;
    }
    plan->row = row;
    memcpy(plan->expected, plan->source, spec->stride);
    if (spec->edit_size > 0u)
        memcpy(plan->expected + spec->edit_offset, spec->edit, spec->edit_size);
    if (!sickle_component_proof(ops, game, owner, map, entities, plan))
    {
        *retry = HF_SICKLE_RETRY_RACE;
        (void)snprintf(reason, capacity, "Sickle %s ownership changed during preflight", spec->label);
        return 0;
    }
    return 1;
}

static int sickle_resolve_registry(const HfSickleOps* ops, uintptr_t game, size_t image_size,
                                   const HfSickleRegistrySpec* spec,
                                   HfSickleRegistryPlan* plan,
                                   HfSickleResult* retry,
                                   char* reason, size_t capacity)
{
    uint8_t pointer_bytes[8];
    uint8_t source_row[272u];
    size_t i;

    memset(plan, 0, sizeof(*plan));
    plan->spec = spec;
    if (spec->size == 0u || spec->size > sizeof(plan->after))
    {
        (void)snprintf(reason, capacity, "Sickle %s registry row size is malformed", spec->label);
        return 0;
    }
    if ((size_t)spec->rva > image_size || 8u > image_size - (size_t)spec->rva ||
        !hf_registry_slot(ops->sdk, game, image_size, spec->rva, spec->id, spec->size, &plan->slot) ||
        ops->registry_access == NULL || !ops->registry_access(ops->context, game, plan->slot) ||
        !sickle_read(ops, plan->slot, pointer_bytes, sizeof(pointer_bytes)))
    {
        (void)snprintf(reason, capacity, "Sickle %s registry slot is not supported writable data", spec->label);
        return 0;
    }
    plan->row = sickle_pointer(pointer_bytes);
    if (plan->row == 0u)
    {
        *retry = HF_SICKLE_RETRY_NOT_READY;
        (void)snprintf(reason, capacity, "Expected one supported Sickle %s row; found 0", spec->label);
        return 0;
    }
    if (!sickle_read(ops, game + spec->rva, pointer_bytes, sizeof(pointer_bytes)) ||
        sickle_pointer(pointer_bytes) != plan->row ||
        !hf_registry_address(plan->row, spec->size, 4u))
    {
        (void)snprintf(reason, capacity, "Sickle %s active registry has a foreign redirect", spec->label);
        return 0;
    }
    if (!sickle_read(ops, plan->row, source_row, spec->size) ||
        sickle_u32(source_row) != spec->id ||
        !sickle_hash_matches(ops, source_row, spec->size, spec->sha256) ||
        memcmp(source_row, spec->original, spec->size) != 0)
    {
        (void)snprintf(reason, capacity, "Sickle %s source registry row changed", spec->label);
        return 0;
    }
    if (spec->patch_size == 0u || spec->offset + spec->patch_size > spec->size ||
        spec->patch_size > sizeof(spec->patch))
    {
        (void)snprintf(reason, capacity, "Sickle %s registry patch is malformed", spec->label);
        return 0;
    }
    memcpy(plan->after, spec->original, spec->size);
    for (i = 0u; i < spec->patch_size; ++i)
        plan->after[spec->offset + i] = spec->patch[i];
    if (ops->owned_copy == NULL ||
        (plan->owned = ops->owned_copy(ops->context, plan->after, spec->size)) == 0u ||
        !sickle_read(ops, plan->owned, source_row, spec->size) ||
        memcmp(source_row, plan->after, spec->size) != 0)
    {
        (void)snprintf(reason, capacity, "Sickle %s retained row allocation failed", spec->label);
        return 0;
    }
    return 1;
}

static int sickle_sources_intact(const HfSickleOps* ops, uintptr_t game,
                                 size_t game_size,
                                 uintptr_t owner, uintptr_t map, uintptr_t entities,
                                 const HfSickleComponentPlan* components, size_t component_count,
                                 const HfSickleRegistryPlan* registries, size_t registry_count)
{
    uint8_t current[1232u];
    uint8_t pointer_bytes[8];
    size_t i;
    for (i = 0u; i < component_count; ++i)
    {
        const HfSickleComponentPlan* p = &components[i];
        if (!sickle_component_proof(ops, game, owner, map, entities, p) ||
            !sickle_read(ops, p->row, current, p->spec->stride) ||
            memcmp(current, p->source, p->spec->stride) != 0)
            return 0;
    }
    for (i = 0u; i < registry_count; ++i)
    {
        const HfSickleRegistryPlan* p = &registries[i];
        uintptr_t active_slot = 0u;
        if (!hf_registry_slot(ops->sdk, game, game_size, p->spec->rva,
                               p->spec->id, p->spec->size, &active_slot) || active_slot != p->slot ||
            !sickle_read(ops, p->slot, pointer_bytes, sizeof(pointer_bytes)) ||
            sickle_pointer(pointer_bytes) != p->row ||
            !sickle_read(ops, p->row, current, p->spec->size) ||
            !sickle_hash_matches(ops, current, p->spec->size, p->spec->sha256) ||
            memcmp(current, p->spec->original, p->spec->size) != 0)
            return 0;
    }
    return 1;
}

static int sickle_rollback(const HfSickleOps* ops, uintptr_t game,
                           size_t game_size,
                           uintptr_t owner, uintptr_t map, uintptr_t entities,
                           HfSickleComponentPlan* components, size_t component_count,
                           HfSickleRegistryPlan* registries, size_t published_count,
                           HfSickleComponentAttempt* attempts, size_t attempt_count)
{
    uint8_t pointer_bytes[8];
    uint8_t current[1232u];
    size_t i;
    int restored = 1;

    for (i = published_count; i > 0u; --i)
    {
        HfSickleRegistryPlan* p = &registries[i - 1u];
        uintptr_t current_pointer;
        uintptr_t active_slot = 0u;
        if (!hf_registry_slot(ops->sdk, game, game_size, p->spec->rva,
                               p->spec->id, p->spec->size, &active_slot) || active_slot != p->slot)
            restored = 0;
        if (!sickle_read(ops, p->slot, pointer_bytes, sizeof(pointer_bytes)))
        {
            restored = 0;
            continue;
        }
        current_pointer = sickle_pointer(pointer_bytes);
        if (current_pointer == p->owned)
        {
            if (ops->publish == NULL || !ops->publish(ops->context, p->slot, p->owned, p->row) ||
                !sickle_read(ops, p->slot, pointer_bytes, sizeof(pointer_bytes)) ||
                sickle_pointer(pointer_bytes) != p->row)
                restored = 0;
        }
        else if (current_pointer != p->row)
            restored = 0; /* A foreign pointer owns the slot now; preserve it. */
    }

    for (i = attempt_count; i > 0u; --i)
    {
        HfSickleComponentAttempt* attempt = &attempts[i - 1u];
        HfSickleComponentPlan* p = &components[attempt->plan_index];
        size_t j;
        int owned = sickle_component_proof(ops, game, owner, map, entities, p) &&
                    sickle_read(ops, attempt->row, current, p->spec->stride);
        if (owned)
        {
            for (j = 0u; j < p->spec->stride; ++j)
            {
                if (j >= attempt->offset && j < attempt->offset + attempt->size)
                {
                    size_t k = j - attempt->offset;
                    if (current[j] != attempt->before[k] && current[j] != attempt->after[k])
                    {
                        owned = 0;
                        break;
                    }
                }
                else if (current[j] != attempt->source[j])
                {
                    owned = 0;
                    break;
                }
            }
        }
        if (!owned)
        {
            restored = 0;
            continue;
        }
        if (ops->write_component == NULL ||
            !ops->write_component(ops->context, attempt->row + attempt->offset,
                                  attempt->before, attempt->size) ||
            !sickle_read(ops, attempt->row, current, p->spec->stride) ||
            memcmp(current, attempt->source, p->spec->stride) != 0)
            restored = 0;
    }
    (void)component_count;
    return restored;
}

static HfSickleResult sickle_apply_profile(const HfSickleOps* ops, uintptr_t executable,
                                           uintptr_t game, size_t game_size,
                                           const HfSickleComponentSpec* component_specs,
                                           size_t component_count,
                                           const HfSickleRegistrySpec* registry_specs,
                                           size_t registry_count,
                                           const HfSickleCodePin* pins, size_t pin_count,
                                           char* reason, size_t capacity)
{
    HfSickleComponentPlan components[6];
    HfSickleRegistryPlan registries[11];
    HfSickleComponentAttempt attempts[6];
    uint8_t pointer_bytes[8];
    uint8_t pin_bytes[16];
    uintptr_t owner;
    uintptr_t map;
    uintptr_t entities;
    uintptr_t address;
    uint8_t component_bytes[1232u];
    uint8_t registry_bytes[272u];
    size_t i;
    size_t attempt_count = 0u;
    size_t published_count = 0u;
    HfSickleResult retry = HF_SICKLE_REFUSED;
    int failed = 0;
    int retry_after_rollback = 0;

    if (reason != NULL && capacity > 0u)
        reason[0] = '\0';
    if (ops == NULL || ops->sdk == NULL || ops->read == NULL || ops->data_access == NULL ||
        ops->write_component == NULL || ops->registry_access == NULL ||
        ops->publish == NULL || ops->sha256 == NULL || ops->verify_modules == NULL ||
        component_specs == NULL || registry_specs == NULL || pins == NULL ||
        component_count != HF_SICKLE_COMPONENT_COUNT ||
        registry_count != HF_SICKLE_REGISTRY_COUNT || pin_count != HF_SICKLE_PIN_COUNT ||
        game == 0u || executable == 0u || game_size == 0u)
    {
        (void)snprintf(reason, capacity, "Sickle native transaction API or profile is incomplete");
        return HF_SICKLE_REFUSED;
    }
    if (!ops->verify_modules(ops->context, executable, game))
    {
        (void)snprintf(reason, capacity, "Sickle executable or game.dll fingerprint is unsupported");
        return HF_SICKLE_REFUSED;
    }
    for (i = 0u; i < pin_count; ++i)
    {
        char expected_hex[17] = {0};
        char actual_hex[17] = {0};
        size_t j, shown;
        if (pins[i].size == 0u || pins[i].size > sizeof(pin_bytes) ||
            (size_t)pins[i].rva > game_size || pins[i].size > game_size - (size_t)pins[i].rva ||
            !sickle_add_address(game, pins[i].rva, &address) ||
            !sickle_read(ops, address, pin_bytes, pins[i].size))
        {
            (void)snprintf(reason, capacity,
                           "Sickle native instruction pin %u at RVA 0x%08X is unreadable or malformed",
                           (unsigned)i + 1u, pins[i].rva);
            return HF_SICKLE_REFUSED;
        }
        if (!hf_registry_pin_matches(ops->sdk, game, game_size, pins[i].rva,
                                      pins[i].bytes, pin_bytes, pins[i].size))
        {
            shown = pins[i].size < 8u ? pins[i].size : 8u;
            for (j = 0u; j < shown; ++j)
            {
                (void)snprintf(expected_hex + j * 2u, 3u, "%02X", pins[i].bytes[j]);
                (void)snprintf(actual_hex + j * 2u, 3u, "%02X", pin_bytes[j]);
            }
            (void)snprintf(reason, capacity,
                           "Sickle instruction pin %u at RVA 0x%08X changed (expected %s, observed %s)",
                           (unsigned)i + 1u, pins[i].rva, expected_hex, actual_hex);
            return HF_SICKLE_REFUSED;
        }
    }
    if (!sickle_read(ops, game + 0x346BF98u, pointer_bytes, sizeof(pointer_bytes)) ||
        (owner = sickle_pointer(pointer_bytes)) == 0u ||
        !sickle_add_address(owner, 0xF12BD8u, &address) ||
        !sickle_read(ops, address, pointer_bytes, sizeof(pointer_bytes)) ||
        (map = sickle_pointer(pointer_bytes)) == 0u || map < 26151684u)
    {
        (void)snprintf(reason, capacity, "Expected one supported Sickle entity table; found 0");
        return HF_SICKLE_RETRY_NOT_READY;
    }
    entities = map - 26151684u;
    for (i = 0u; i < component_count; ++i)
    {
        if (!sickle_resolve_component(ops, game, owner, map, entities,
                                      &component_specs[i], &components[i],
                                      &retry, reason, capacity))
            return retry;
    }
    for (i = 0u; i < registry_count; ++i)
    {
        if (!sickle_resolve_registry(ops, game, game_size, &registry_specs[i],
                                     &registries[i], &retry, reason, capacity))
            return retry;
    }
    if (!sickle_sources_intact(ops, game, game_size, owner, map, entities,
                               components, component_count, registries, registry_count))
    {
        (void)snprintf(reason, capacity, "Data changed before edit. No edit applied.");
        return HF_SICKLE_RETRY_RACE;
    }

    for (i = 0u; i < component_count; ++i)
    {
        HfSickleComponentPlan* p = &components[i];
        const HfSickleComponentSpec* spec = p->spec;
        HfSickleComponentAttempt* attempt;
        if (spec->edit_size == 0u ||
            memcmp(p->source + spec->edit_offset, spec->edit, spec->edit_size) == 0)
            continue;
        if (!sickle_component_proof(ops, game, owner, map, entities, p) ||
            !sickle_read(ops, p->row, component_bytes, spec->stride) ||
            memcmp(component_bytes, p->source, spec->stride) != 0)
        {
            (void)snprintf(reason, capacity, "Data changed before edit. No edit applied.");
            return HF_SICKLE_RETRY_RACE;
        }
        attempt = &attempts[attempt_count++];
        memset(attempt, 0, sizeof(*attempt));
        attempt->plan_index = i;
        attempt->row = p->row;
        attempt->offset = spec->edit_offset;
        attempt->size = spec->edit_size;
        memcpy(attempt->source, p->source, spec->stride);
        memcpy(attempt->before, p->source + spec->edit_offset, spec->edit_size);
        memcpy(attempt->after, spec->edit, spec->edit_size);
        if (!ops->write_component(ops->context, p->row + spec->edit_offset,
                                  spec->edit, spec->edit_size) ||
            !sickle_read(ops, p->row, component_bytes, spec->stride) ||
            memcmp(component_bytes, p->expected, spec->stride) != 0)
        {
            failed = 1;
            (void)snprintf(reason, capacity, "Sickle %s component write did not verify", spec->label);
            break;
        }
    }
    if (!failed)
    {
        for (i = 0u; i < registry_count; ++i)
        {
            HfSickleRegistryPlan* p = &registries[i];
            uintptr_t active_slot = 0u;
            published_count = i + 1u;
            if (!hf_registry_slot(ops->sdk, game, game_size, p->spec->rva,
                                   p->spec->id, p->spec->size, &active_slot) || active_slot != p->slot ||
                !sickle_read(ops, p->row, registry_bytes, p->spec->size) ||
                memcmp(registry_bytes, p->spec->original, p->spec->size) != 0 ||
                !sickle_read(ops, p->owned, registry_bytes, p->spec->size) ||
                memcmp(registry_bytes, p->after, p->spec->size) != 0 ||
                !ops->publish(ops->context, p->slot, p->row, p->owned))
            {
                failed = 1;
                (void)snprintf(reason, capacity, "Sickle %s registry publication failed", p->spec->label);
                break;
            }
        }
    }
    if (!failed)
    {
        for (i = 0u; i < component_count; ++i)
        {
            if (!sickle_component_proof(ops, game, owner, map, entities, &components[i]) ||
                !sickle_read(ops, components[i].row, component_bytes, components[i].spec->stride) ||
                memcmp(component_bytes, components[i].expected, components[i].spec->stride) != 0)
            {
                failed = 1;
                retry_after_rollback = 1;
                (void)snprintf(reason, capacity, "Full table verification failed for %s", components[i].spec->label);
                break;
            }
        }
    }
    if (!failed)
    {
        for (i = 0u; i < registry_count; ++i)
        {
            HfSickleRegistryPlan* p = &registries[i];
            uintptr_t active_slot = 0u;
            if (!hf_registry_slot(ops->sdk, game, game_size, p->spec->rva,
                                   p->spec->id, p->spec->size, &active_slot) || active_slot != p->slot ||
                !sickle_read(ops, p->slot, pointer_bytes, sizeof(pointer_bytes)) ||
                sickle_pointer(pointer_bytes) != p->owned ||
                !sickle_read(ops, p->owned, registry_bytes, p->spec->size) ||
                memcmp(registry_bytes, p->after, p->spec->size) != 0 ||
                !sickle_read(ops, p->row, registry_bytes, p->spec->size) ||
                memcmp(registry_bytes, p->spec->original, p->spec->size) != 0)
            {
                failed = 1;
                (void)snprintf(reason, capacity, "Full registry verification failed for %s", p->spec->label);
                break;
            }
        }
    }
    if (!failed)
    {
        (void)snprintf(reason, capacity,
                       "LAS-16 70/10, half heat, 700 RPM; Double-Edge 65/10 cold-warm, 83/14 hot-maximum, -25%% direct heat self-damage; both 2000 m/s, zero spread, no fall-off; AP3 minimum and AP4 maximum retained");
        return HF_SICKLE_APPLIED;
    }

    if (!sickle_rollback(ops, game, game_size, owner, map, entities,
                         components, component_count, registries, published_count,
                         attempts, attempt_count))
    {
        (void)snprintf(reason, capacity, "FATAL: Sickle transaction failed and one or more owned changes could not be restored");
        return HF_SICKLE_FATAL;
    }
    return retry_after_rollback ? HF_SICKLE_RETRY_RACE : HF_SICKLE_REFUSED;
}

#if HF_PORT_ALLOW_NATIVE_RUNTIME
#include <windows.h>

typedef LONG (WINAPI *HfSickleBcryptOpen)(void**, LPCWSTR, LPCWSTR, ULONG);
typedef LONG (WINAPI *HfSickleBcryptClose)(void*, ULONG);
typedef LONG (WINAPI *HfSickleBcryptCreate)(void*, void**, void*, ULONG, PUCHAR, ULONG, ULONG);
typedef LONG (WINAPI *HfSickleBcryptData)(void*, PUCHAR, ULONG, ULONG);
typedef LONG (WINAPI *HfSickleBcryptFinish)(void*, PUCHAR, ULONG, ULONG);
typedef LONG (WINAPI *HfSickleBcryptDestroy)(void*);

typedef struct HfSickleCrypto
{
    HMODULE library;
    HfSickleBcryptOpen open;
    HfSickleBcryptClose close;
    HfSickleBcryptCreate create;
    HfSickleBcryptData data;
    HfSickleBcryptFinish finish;
    HfSickleBcryptDestroy destroy;
} HfSickleCrypto;

static int sickle_crypto_init(HfSickleCrypto* crypto)
{
    if (crypto->library != NULL)
        return 1;
    crypto->library = LoadLibraryW(L"bcrypt.dll");
    if (crypto->library == NULL)
        return 0;
    crypto->open = (HfSickleBcryptOpen)GetProcAddress(crypto->library, "BCryptOpenAlgorithmProvider");
    crypto->close = (HfSickleBcryptClose)GetProcAddress(crypto->library, "BCryptCloseAlgorithmProvider");
    crypto->create = (HfSickleBcryptCreate)GetProcAddress(crypto->library, "BCryptCreateHash");
    crypto->data = (HfSickleBcryptData)GetProcAddress(crypto->library, "BCryptHashData");
    crypto->finish = (HfSickleBcryptFinish)GetProcAddress(crypto->library, "BCryptFinishHash");
    crypto->destroy = (HfSickleBcryptDestroy)GetProcAddress(crypto->library, "BCryptDestroyHash");
    return crypto->open != NULL && crypto->close != NULL && crypto->create != NULL &&
           crypto->data != NULL && crypto->finish != NULL && crypto->destroy != NULL;
}

static int sickle_hash_bytes(HfSickleCrypto* crypto, const void* bytes, size_t size, char output[65])
{
    static const wchar_t algorithm_name[] = L"SHA256";
    static const char hex[] = "0123456789ABCDEF";
    void* algorithm = NULL;
    void* hash = NULL;
    uint8_t digest[32];
    size_t offset = 0u;
    size_t i;
    int ok = 0;
    if (!sickle_crypto_init(crypto) || bytes == NULL || output == NULL || size > UINT32_MAX)
        return 0;
    if (crypto->open(&algorithm, algorithm_name, NULL, 0u) != 0 ||
        crypto->create(algorithm, &hash, NULL, 0u, NULL, 0u, 0u) != 0)
        goto done;
    while (offset < size)
    {
        ULONG take = (ULONG)((size - offset) > 65536u ? 65536u : (size - offset));
        if (crypto->data(hash, (PUCHAR)((const uint8_t*)bytes + offset), take, 0u) != 0)
            goto done;
        offset += take;
    }
    if (crypto->finish(hash, digest, sizeof(digest), 0u) != 0)
        goto done;
    for (i = 0u; i < sizeof(digest); ++i)
    {
        output[i * 2u] = hex[digest[i] >> 4];
        output[i * 2u + 1u] = hex[digest[i] & 0x0Fu];
    }
    output[64] = '\0';
    ok = 1;
done:
    if (hash != NULL)
        (void)crypto->destroy(hash);
    if (algorithm != NULL)
        (void)crypto->close(algorithm, 0u);
    return ok;
}

static int sickle_hash_module_file(HfSickleCrypto* crypto, uintptr_t module, char output[65])
{
    WCHAR path[32768];
    DWORD path_size;
    HANDLE file;
    void* algorithm = NULL;
    void* hash = NULL;
    uint8_t buffer[65536];
    uint8_t digest[32];
    static const wchar_t algorithm_name[] = L"SHA256";
    static const char hex[] = "0123456789ABCDEF";
    size_t i;
    int ok = 0;
    if (!sickle_crypto_init(crypto) || module == 0u)
        return 0;
    path_size = GetModuleFileNameW((HMODULE)module, path, (DWORD)(sizeof(path) / sizeof(path[0])));
    if (path_size == 0u || path_size >= sizeof(path) / sizeof(path[0]))
        return 0;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    if (crypto->open(&algorithm, algorithm_name, NULL, 0u) != 0 ||
        crypto->create(algorithm, &hash, NULL, 0u, NULL, 0u, 0u) != 0)
        goto done;
    for (;;)
    {
        DWORD read_size = 0u;
        if (!ReadFile(file, buffer, (DWORD)sizeof(buffer), &read_size, NULL))
            goto done;
        if (read_size == 0u)
            break;
        if (crypto->data(hash, buffer, read_size, 0u) != 0)
            goto done;
    }
    if (crypto->finish(hash, digest, sizeof(digest), 0u) != 0)
        goto done;
    for (i = 0u; i < sizeof(digest); ++i)
    {
        output[i * 2u] = hex[digest[i] >> 4];
        output[i * 2u + 1u] = hex[digest[i] & 0x0Fu];
    }
    output[64] = '\0';
    ok = 1;
done:
    if (hash != NULL)
        (void)crypto->destroy(hash);
    if (algorithm != NULL)
        (void)crypto->close(algorithm, 0u);
    CloseHandle(file);
    return ok;
}

typedef struct HfSickleNativeContext
{
    HfSickleReadContext reader;
    HfMod* mod;
    uintptr_t game;
    size_t game_size;
    HfSickleCrypto crypto;
    uintptr_t retained[HF_SICKLE_REGISTRY_COUNT];
    size_t retained_count;
} HfSickleNativeContext;

static int sickle_native_page_access(uintptr_t address, size_t size, int writable,
                                     DWORD allowed_type_a, DWORD allowed_type_b)
{
    uintptr_t cursor = address;
    size_t remaining = size;
    if (size == 0u || address < (uintptr_t)65536u || address > UINTPTR_MAX - size)
        return 0;
    while (remaining > 0u)
    {
        MEMORY_BASIC_INFORMATION info;
        SIZE_T queried = VirtualQuery((LPCVOID)cursor, &info, sizeof(info));
        DWORD protection;
        size_t offset;
        size_t take;
        if (queried != sizeof(info) || info.State != MEM_COMMIT ||
            (info.Type != allowed_type_a && info.Type != allowed_type_b) ||
            (info.Protect & (PAGE_GUARD | PAGE_NOCACHE)) != 0u)
            return 0;
        protection = info.Protect;
        if ((protection != PAGE_READONLY && protection != PAGE_READWRITE) ||
            (writable && protection != PAGE_READWRITE))
            return 0;
        offset = (size_t)(cursor - (uintptr_t)info.BaseAddress);
        if (offset >= info.RegionSize)
            return 0;
        take = info.RegionSize - offset;
        if (take > remaining)
            take = remaining;
        cursor += take;
        remaining -= take;
    }
    return 1;
}

static int sickle_native_data_access(void* context, uintptr_t address, size_t size, int writable)
{
    (void)context;
    return sickle_native_page_access(address, size, writable, MEM_PRIVATE, MEM_PRIVATE);
}

static int sickle_native_write_component(void* context, uintptr_t address,
                                        const void* input, size_t size)
{
    HfSickleNativeContext* native = (HfSickleNativeContext*)context;
    uintptr_t cursor = address;
    size_t remaining = size;
    const uint8_t* bytes = (const uint8_t*)input;
    if (input == NULL || !sickle_native_data_access(context, address, size, 0))
        return 0;
    while (remaining > 0u)
    {
        MEMORY_BASIC_INFORMATION info;
        SIZE_T queried = VirtualQuery((LPCVOID)cursor, &info, sizeof(info));
        DWORD protection;
        DWORD previous = 0u;
        uintptr_t region_end;
        size_t offset;
        size_t take;
        int write_ok;
        if (queried != sizeof(info) || info.State != MEM_COMMIT || info.Type != MEM_PRIVATE)
            return 0;
        protection = info.Protect;
        offset = (size_t)(cursor - (uintptr_t)info.BaseAddress);
        if (offset >= info.RegionSize)
            return 0;
        take = info.RegionSize - offset;
        if (take > remaining)
            take = remaining;
        region_end = cursor + take;
        if (protection == PAGE_READONLY)
        {
            if (!VirtualProtect((LPVOID)cursor, take, PAGE_READWRITE, &previous))
                return 0;
        }
        else if (protection != PAGE_READWRITE)
            return 0;
        write_ok = native->reader.api->write(cursor, bytes, take) == 0;
        if (protection == PAGE_READONLY)
        {
            DWORD ignored = 0u;
            if (!VirtualProtect((LPVOID)cursor, take, previous, &ignored))
                return 0;
        }
        if (!write_ok)
            return 0;
        bytes += take;
        remaining -= take;
        cursor = region_end;
    }
    return 1;
}

static int sickle_native_registry_access(void* context, uintptr_t game, uintptr_t slot)
{
    HfSickleNativeContext* native = (HfSickleNativeContext*)context;
    size_t i;
    for (i = 0u; i < HF_SICKLE_REGISTRY_COUNT; ++i)
    {
        const HfSickleRegistrySpec* spec = &HF_SICKLE_REGISTRIES[i];
        uintptr_t active_slot;
        if (game == native->game && hf_registry_slot(native->reader.api, game, native->game_size,
              spec->rva, spec->id, spec->size, &active_slot) && slot == active_slot)
            return (slot & 7u) == 0u &&
                   sickle_native_page_access(slot, 8u, 1, MEM_PRIVATE, MEM_IMAGE);
    }
    return 0;
}

static uintptr_t sickle_native_owned_copy(void* context, const void* bytes, size_t size)
{
    HfSickleNativeContext* native = (HfSickleNativeContext*)context;
    void* memory;
    if (bytes == NULL || native->retained_count >= HF_SICKLE_REGISTRY_COUNT ||
        (size != 76u && size != 272u))
        return 0u;
    memory = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (memory == NULL)
        return 0u;
    memcpy(memory, bytes, size);
    native->retained[native->retained_count++] = (uintptr_t)memory;
    return (uintptr_t)memory;
}

static int sickle_native_publish(void* context, uintptr_t slot,
                                 uintptr_t expected, uintptr_t replacement)
{
    HfSickleNativeContext* native = (HfSickleNativeContext*)context;
    uintptr_t previous;
    if ((slot & 7u) != 0u ||
        !sickle_native_registry_access(context, native->game, slot))
        return 0;
    previous = (uintptr_t)InterlockedCompareExchangePointer(
        (PVOID volatile*)slot, (PVOID)replacement, (PVOID)expected);
    return previous == expected && *(volatile uintptr_t*)slot == replacement;
}

static int sickle_native_sha256(void* context, const void* bytes, size_t size, char output[65])
{
    HfSickleNativeContext* native = (HfSickleNativeContext*)context;
    return sickle_hash_bytes(&native->crypto, bytes, size, output);
}

static int sickle_native_verify_modules(void* context, uintptr_t executable, uintptr_t game)
{
    HfSickleNativeContext* native = (HfSickleNativeContext*)context;
    char exe_hash[65];
    char game_hash[65];
    return sickle_hash_module_file(&native->crypto, executable, exe_hash) &&
           sickle_hash_module_file(&native->crypto, game, game_hash) &&
           strcmp(exe_hash, HF_SICKLE_EXE_SHA256) == 0 &&
           strcmp(game_hash, HF_SICKLE_GAME_SHA256) == 0;
}

static int sickle_apply_native(const HfApi* api, HfMod* mod, char* reason, size_t capacity,
                               HfSickleResult* outcome)
{
    HfSickleNativeContext* native = NULL;
    uintptr_t executable;
    uintptr_t game;
    size_t executable_size = 0u;
    size_t game_size = 0u;
    HfSickleOps ops;
    if (api == NULL || api->module_base == NULL || api->read == NULL || api->write == NULL)
    {
        (void)snprintf(reason, capacity, "HellForge module-memory APIs are unavailable");
        *outcome = HF_SICKLE_REFUSED;
        return 0;
    }
    executable = api->module_base("helldivers2.exe", &executable_size);
    game = api->module_base("game.dll", &game_size);
    if (executable == 0u || executable_size == 0u || game == 0u || game_size == 0u)
    {
        (void)snprintf(reason, capacity, "Helldivers 2 executable or game.dll is unavailable");
        *outcome = HF_SICKLE_REFUSED;
        return 0;
    }
    native = (HfSickleNativeContext*)calloc(1u, sizeof(*native));
    if (native == NULL)
    {
        (void)snprintf(reason, capacity, "Sickle native transaction state allocation failed");
        *outcome = HF_SICKLE_REFUSED;
        return 0;
    }
    native->reader.api = api;
    native->mod = mod;
    native->game = game;
    native->game_size = game_size;
    memset(&ops, 0, sizeof(ops));
    ops.context = native;
    ops.sdk = api;
    ops.read = sickle_native_read;
    ops.data_access = sickle_native_data_access;
    ops.write_component = sickle_native_write_component;
    ops.registry_access = sickle_native_registry_access;
    ops.owned_copy = sickle_native_owned_copy;
    ops.publish = sickle_native_publish;
    ops.sha256 = sickle_native_sha256;
    ops.verify_modules = sickle_native_verify_modules;
    *outcome = sickle_apply_profile(&ops, executable, game, game_size,
                                    HF_SICKLE_COMPONENTS, HF_SICKLE_COMPONENT_COUNT,
                                    HF_SICKLE_REGISTRIES, HF_SICKLE_REGISTRY_COUNT,
                                    HF_SICKLE_PINS, HF_SICKLE_PIN_COUNT,
                                    reason, capacity);
    /* Every owned row is deliberately leaked into the process lifetime, including clean refusals. */
    if (native->crypto.library != NULL)
        (void)FreeLibrary(native->crypto.library);
    free(native);
    return 1;
}

typedef struct HfSickleRunner
{
    const HfApi* api;
    HfMod* mod;
    uint64_t next_attempt_ms;
    unsigned attempts;
    int registered;
    int done;
} HfSickleRunner;

static HfSickleRunner HF_SICKLE_RUNNER;

static void sickle_native_tick(HfMod* mod, float dt, void* user)
{
    HfSickleRunner* runner = (HfSickleRunner*)user;
    uint64_t now;
    uint64_t delay;
    HfSickleResult outcome = HF_SICKLE_REFUSED;
    char reason[384];
    int invoked;
    (void)dt;
    if (runner == NULL || runner->done)
        return;
    now = GetTickCount64();
    if (now < runner->next_attempt_ms)
        return;
    ++runner->attempts;
    invoked = sickle_apply_native(runner->api, mod, reason, sizeof(reason), &outcome);
    if (!invoked)
        outcome = HF_SICKLE_REFUSED;
    if (outcome == HF_SICKLE_APPLIED)
    {
        runner->done = 1;
        runner->api->log(mod, "Applied sickle-laser-ar v0.1.2: %s", reason);
        return;
    }
    delay = sickle_retry_delay_ms(outcome, runner->attempts);
    if (delay != 0u)
    {
        runner->next_attempt_ms = now + delay;
        runner->api->log(mod, "Waiting for Sickle native data (%u/8): %s",
                         runner->attempts, reason);
        return;
    }
    runner->done = 1;
    if (outcome == HF_SICKLE_FATAL)
        runner->api->log(mod, "FATAL: Sickle native transaction stopped: %s", reason);
    else
        runner->api->log(mod, "Refused sickle-laser-ar v0.1.2: %s", reason);
}

#if defined(__clang__) || defined(__GNUC__)
#define HF_SICKLE_UNUSED __attribute__((unused))
#else
#define HF_SICKLE_UNUSED
#endif
static void HF_SICKLE_UNUSED sickle_begin_native(const HfApi* api, HfMod* mod)
{
    HfSickleRunner* runner = &HF_SICKLE_RUNNER;
    if (runner->registered || runner->done)
        return;
    runner->api = api;
    runner->mod = mod;
    runner->attempts = 0u;
    runner->next_attempt_ms = GetTickCount64() + 15000u;
    if (api->size < (uint32_t)(offsetof(HfApi, on_game_thread) + sizeof(api->on_game_thread)) ||
        api->on_game_thread == NULL || api->on_game_thread(mod, sickle_native_tick, runner) == 0)
    {
        runner->done = 1;
        api->log(mod, "Refused sickle-laser-ar v0.1.2: HellForge game-thread scheduling is unavailable");
        return;
    }
    runner->registered = 1;
    api->log(mod, "Sickle native setup queued; first guarded attempt is in 15 seconds");
}
#endif
