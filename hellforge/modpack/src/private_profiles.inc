/* Transactional native profiles for BR-14 Adjudicator, GL-52 and AR-61 Tenderizer. */
#include "private_profile_source.generated.h"
#include "registry_bridge.inc"

#define HF_PRIVATE_MAX_COMPONENTS 4u
#define HF_PRIVATE_MAX_REGISTRIES 2u
#define HF_PRIVATE_MAX_DATA_PLANS 4u
#define HF_PRIVATE_MAX_ATTEMPTS 8u
#define HF_PRIVATE_MAX_STATUS_SIZE 152u

#if !defined(HF_PORT_GAS_OVERHAUL)
#define HF_PORT_GAS_OVERHAUL 0u
#endif

typedef struct HfPrivatePatch
{
    uint32_t offset;
    uint8_t size;
    uint8_t bytes[HF_PRIVATE_MAX_COMPONENT_PATCH];
} HfPrivatePatch;

typedef struct HfPrivateDataPlan
{
    const char* label;
    const HfPrivateComponentSpec* component;
    uintptr_t row;
    uintptr_t proof_slot;
    uint32_t size;
    uint8_t patch_count;
    uint8_t is_status;
    HfPrivatePatch patches[2];
    uint8_t before[HF_PRIVATE_MAX_COMPONENT_SIZE];
    uint8_t after[HF_PRIVATE_MAX_COMPONENT_SIZE];
} HfPrivateDataPlan;

typedef struct HfPrivateRegistryPlan
{
    const HfPrivateRegistrySpec* spec;
    uintptr_t slot;
    uintptr_t source_row;
    uintptr_t owned_row;
    uint8_t before[HF_PRIVATE_MAX_REGISTRY_SIZE];
    uint8_t after[HF_PRIVATE_MAX_REGISTRY_SIZE];
} HfPrivateRegistryPlan;

typedef struct HfPrivateAttempt
{
    uintptr_t row;
    uintptr_t proof_slot;
    uint32_t size;
    uint32_t offset;
    uint8_t patch_size;
    uint8_t before[HF_PRIVATE_MAX_COMPONENT_PATCH];
    uint8_t after[HF_PRIVATE_MAX_COMPONENT_PATCH];
    uint8_t source[HF_PRIVATE_MAX_COMPONENT_SIZE];
    const HfPrivateComponentSpec* component;
} HfPrivateAttempt;

typedef struct HfPrivateState
{
    const char* option;
    uint32_t applied;
    uint32_t poisoned;
    uint32_t component_count;
    uintptr_t component_rows[HF_PRIVATE_MAX_COMPONENTS];
    uint8_t component_after[HF_PRIVATE_MAX_COMPONENTS][HF_PRIVATE_MAX_COMPONENT_SIZE];
    uint32_t registry_count;
    uintptr_t registry_slots[HF_PRIVATE_MAX_REGISTRIES];
    uintptr_t registry_sources[HF_PRIVATE_MAX_REGISTRIES];
    uintptr_t registry_owned[HF_PRIVATE_MAX_REGISTRIES];
    uint8_t registry_after[HF_PRIVATE_MAX_REGISTRIES][HF_PRIVATE_MAX_REGISTRY_SIZE];
    uintptr_t status_slot;
    uintptr_t status_row;
    uint32_t status_size;
    uint8_t status_after[HF_PRIVATE_MAX_STATUS_SIZE];
} HfPrivateState;

static HfPrivateState HF_PRIVATE_STATES[HF_PRIVATE_PROFILE_COUNT];

#if defined(HF_PORT_TEST)
extern int hf_private_test_hash(const void* bytes, size_t size, char output[65]);
extern int hf_private_test_register_allocation(uintptr_t address, size_t size);
static int hf_private_hash(const void* bytes, size_t size, char output[65])
{
    return hf_private_test_hash(bytes, size, output);
}
#else
static HfSickleCrypto HF_PRIVATE_CRYPTO;
static int hf_private_hash(const void* bytes, size_t size, char output[65])
{
    if (!sickle_crypto_init(&HF_PRIVATE_CRYPTO))
        return 0;
    return sickle_hash_bytes(&HF_PRIVATE_CRYPTO, bytes, size, output);
}
#endif

static uint32_t hf_private_u32(const uint8_t* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int hf_private_hash_matches(const void* bytes, size_t size, const char* expected)
{
    char digest[65];
    return hf_private_hash(bytes, size, digest) && strcmp(digest, expected) == 0;
}

static int hf_private_pointer_supported(uintptr_t pointer)
{
    return pointer >= (uintptr_t)65536u &&
           pointer < (uintptr_t)UINT64_C(0x800000000000) && (pointer & 7u) == 0u;
}

static int hf_private_range(uintptr_t base, size_t image_size, uint32_t rva,
                            size_t size, uintptr_t* address)
{
    if (address == NULL || base < (uintptr_t)65536u || image_size == 0u ||
        (size_t)rva > image_size || size > image_size - (size_t)rva ||
        base > UINTPTR_MAX - (uintptr_t)rva)
        return 0;
    *address = base + (uintptr_t)rva;
    return *address >= (uintptr_t)65536u && *address < (uintptr_t)UINT64_C(0x800000000000);
}

/* The source profiles whitelist readable private component/status rows and writable aligned
 * registry slots. The SDK still performs the actual guarded reads and writes. */
static int hf_private_page_access(uintptr_t address, size_t size, int writable,
                                  int allow_image)
{
#if defined(HF_PORT_TEST)
    (void)address;
    (void)size;
    (void)writable;
    (void)allow_image;
    return 1;
#else
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
            (info.Type != MEM_PRIVATE && !(allow_image && info.Type == MEM_IMAGE)) ||
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
#endif
}

static int hf_private_read(const HfApi* api, uintptr_t address, void* output, size_t size)
{
    return api != NULL && api->read != NULL && address != 0u && output != NULL && size != 0u &&
           api->read(address, output, size) == 0;
}

static int hf_private_write(const HfApi* api, uintptr_t address, const void* input, size_t size)
{
    return api != NULL && api->write != NULL && address != 0u && input != NULL && size != 0u &&
           api->write(address, input, size) == 0;
}

static int hf_private_component_proof(const HfApi* api, const HfPrivateDataPlan* plan)
{
    uintptr_t row;
    uint32_t stride = 0u;
    if (api == NULL || api->component_record == NULL || plan == NULL || plan->component == NULL)
        return 0;
    row = api->component_record(plan->component->component,
                                plan->component->resource, &stride);
    return row == plan->row && stride == plan->component->stride;
}

static int hf_private_status_proof(const HfApi* api, const HfPrivateDataPlan* plan)
{
    uintptr_t pointer = 0u;
    return plan != NULL && plan->is_status != 0u && plan->proof_slot != 0u &&
           hf_private_read(api, plan->proof_slot, &pointer, sizeof(pointer)) && pointer == plan->row;
}

static int hf_private_data_proof(const HfApi* api, const HfPrivateDataPlan* plan)
{
    return plan != NULL && (plan->is_status ? hf_private_status_proof(api, plan)
                                            : hf_private_component_proof(api, plan));
}

static int hf_private_pin_check(const HfApi* api, uintptr_t game, size_t game_size,
                                const HfPrivateProfileSpec* profile,
                                char* reason, size_t capacity)
{
    uint8_t actual[16];
    uint32_t i;
    for (i = 0u; i < profile->pin_count; ++i)
    {
        const HfPrivatePinSpec* pin = &HF_PRIVATE_PINS[profile->pin_start + i];
        uintptr_t address;
        if (pin->size == 0u || pin->size > sizeof(actual) ||
            !hf_private_range(game, game_size, pin->rva, pin->size, &address) ||
            !hf_private_read(api, address, actual, pin->size))
        {
            (void)snprintf(reason, capacity, "%s native data-consumer pin %u at RVA 0x%08X is unreadable or malformed",
                           profile->option, (unsigned)i + 1u, pin->rva);
            return 0;
        }
        if (!hf_registry_pin_matches(api, game, game_size, pin->rva,
                                      pin->bytes, actual, pin->size))
        {
            char expected_hex[17] = {0};
            char actual_hex[17] = {0};
            size_t j, shown = pin->size < 8u ? pin->size : 8u;
            for (j = 0u; j < shown; ++j)
            {
                (void)snprintf(expected_hex + j * 2u, 3u, "%02X", pin->bytes[j]);
                (void)snprintf(actual_hex + j * 2u, 3u, "%02X", actual[j]);
            }
            (void)snprintf(reason, capacity,
                           "%s native data-consumer pin %u at RVA 0x%08X changed (expected %s, observed %s)",
                           profile->option, (unsigned)i + 1u, pin->rva, expected_hex, actual_hex);
            return 0;
        }
    }
    return 1;
}

static int hf_private_prepare_component(const HfApi* api,
                                        const HfPrivateComponentSpec* spec,
                                        HfPrivateDataPlan* plan,
                                        char* reason, size_t capacity)
{
    uint32_t stride = 0u;
    uint32_t i;
    memset(plan, 0, sizeof(*plan));
    plan->label = spec->label;
    plan->component = spec;
    plan->size = spec->stride;
    plan->row = api->component_record(spec->component, spec->resource, &stride);
    if (plan->row == 0u || stride != spec->stride || plan->size > HF_PRIVATE_MAX_COMPONENT_SIZE ||
        !hf_private_page_access(plan->row, plan->size, 0, 0) ||
        !hf_private_read(api, plan->row, plan->before, plan->size) ||
        !hf_private_hash_matches(plan->before, plan->size, spec->sha256))
    {
        (void)snprintf(reason, capacity, "%s %s component row ownership or checksum changed",
                       spec->option, spec->label);
        return 0;
    }
    memcpy(plan->after, plan->before, plan->size);
    for (i = 0u; i < spec->edit_count; ++i)
    {
        const HfPrivateEditSpec* edit = &spec->edits[i];
        if (edit->size == 0u || edit->size > HF_PRIVATE_MAX_COMPONENT_PATCH ||
            edit->offset > plan->size || edit->size > plan->size - edit->offset)
        {
            (void)snprintf(reason, capacity, "%s %s component patch is malformed",
                           spec->option, spec->label);
            return 0;
        }
        plan->patches[plan->patch_count].offset = edit->offset;
        plan->patches[plan->patch_count].size = edit->size;
        memcpy(plan->patches[plan->patch_count].bytes, edit->bytes, edit->size);
        memcpy(plan->after + edit->offset, edit->bytes, edit->size);
        ++plan->patch_count;
    }
    return hf_private_component_proof(api, plan);
}

static int hf_private_status_name(const HfApi* api, uintptr_t row,
                                  const HfPrivateProfileSpec* profile,
                                  uint8_t snapshot[HF_PRIVATE_MAX_STATUS_SIZE],
                                  uint32_t* size, char* reason, size_t capacity)
{
    uintptr_t name = 0u;
    uint8_t expected_duration[4] = {0x00u, 0x00u, 0xA0u, 0x40u};
    uint8_t actual_name[11];
    if (!hf_private_page_access(row, 44u, 0, 0) ||
        !hf_private_read(api, row, snapshot, 44u) ||
        hf_private_u32(snapshot) != profile->status_id)
        goto invalid;
    memcpy(&name, snapshot + 8u, sizeof(name));
    if (!hf_registry_address(name, sizeof(actual_name), 1u) ||
        memcmp(snapshot + 40u, expected_duration, sizeof(expected_duration)) != 0 ||
        !hf_private_read(api, name, actual_name, sizeof(actual_name)) ||
        memcmp(actual_name, "Stun Large\0", sizeof(actual_name)) != 0)
        goto invalid;
    *size = 44u;
    return 1;
invalid:
    (void)snprintf(reason, capacity, "GL-52 Stun Large ID/name/duration definition changed");
    return 0;
}

static int hf_private_status_acid(const HfApi* api, uintptr_t row,
                                 const HfPrivateProfileSpec* profile,
                                 uint8_t snapshot[HF_PRIVATE_MAX_STATUS_SIZE],
                                 uint32_t* size, int gas_required,
                                 char* reason, size_t capacity)
{
    static const uint8_t one_second[4] = {0x00u, 0x00u, 0x80u, 0x3Fu};
    static const uint8_t fifteen_seconds[4] = {0x00u, 0x00u, 0x70u, 0x41u};
    uint32_t i;
    if (!hf_private_page_access(row, HF_PRIVATE_MAX_STATUS_SIZE, 0, 0) ||
        !hf_private_read(api, row, snapshot, HF_PRIVATE_MAX_STATUS_SIZE) ||
        hf_private_u32(snapshot) != profile->status_id ||
        memcmp(snapshot + 16u, profile->status_signature, sizeof(profile->status_signature)) != 0 ||
        hf_private_u32(snapshot + 44u) != 0u || snapshot[48] != 0u)
        goto invalid;
    for (i = 56u; i <= 96u; ++i)
        if (snapshot[i] != 0u)
            goto invalid;
    if (memcmp(snapshot + 40u, one_second, sizeof(one_second)) != 0 &&
        memcmp(snapshot + 40u, fifteen_seconds, sizeof(fifteen_seconds)) != 0)
        goto invalid;
    if (gas_required && memcmp(snapshot + 40u, fifteen_seconds, sizeof(fifteen_seconds)) != 0)
    {
        (void)snprintf(reason, capacity,
                       "Gas Overhaul is enabled but Acid Storm has not reached its 15-second Applied state");
        return 0;
    }
    *size = HF_PRIVATE_MAX_STATUS_SIZE;
    return 1;
invalid:
    (void)snprintf(reason, capacity, "Tenderizer Acid Storm ID/signature/payload/duration changed");
    return 0;
}

static int hf_private_prepare_status(const HfApi* api, uintptr_t game, size_t game_size,
                                     const HfPrivateProfileSpec* profile,
                                     HfPrivateDataPlan* plan, uint8_t* status_after,
                                     uint32_t* status_size,
                                     char* reason, size_t capacity)
{
    uintptr_t slot;
    uintptr_t row = 0u;
    uint8_t status_source[HF_PRIVATE_MAX_STATUS_SIZE];
    uint32_t size = 0u;
    if (profile->status_kind == 0u)
        return 1;
    if (!hf_private_range(game, game_size, profile->status_slot, sizeof(row), &slot) ||
        !hf_private_read(api, slot, &row, sizeof(row)) || !hf_registry_address(row, 44u, 4u))
    {
        (void)snprintf(reason, capacity, "%s status lookup row is unavailable", profile->option);
        return 0;
    }
    if (profile->status_kind == 1u)
    {
        if (!hf_private_status_name(api, row, profile, status_source, &size, reason, capacity))
            return 0;
    }
    else if (!hf_private_status_acid(api, row, profile, status_source, &size,
                                     HF_PORT_GAS_OVERHAUL != 0u, reason, capacity))
        return 0;

    if (plan != NULL)
    {
        memset(plan, 0, sizeof(*plan));
        plan->label = "Acid Storm shared duration";
        plan->row = row;
        plan->proof_slot = slot;
        plan->size = size;
        plan->is_status = 1u;
        memcpy(plan->before, status_source, size);
        memcpy(plan->after, status_source, size);
        if (profile->status_kind == 2u &&
            memcmp(status_source + 40u, "\x00\x00\x80\x3F", 4u) == 0)
        {
            plan->patch_count = 1u;
            plan->patches[0].offset = 40u;
            plan->patches[0].size = 4u;
            plan->patches[0].bytes[0] = 0x00u;
            plan->patches[0].bytes[1] = 0x00u;
            plan->patches[0].bytes[2] = 0x70u;
            plan->patches[0].bytes[3] = 0x41u;
            memcpy(plan->after + 40u, plan->patches[0].bytes, 4u);
        }
    }
    memcpy(status_after, status_source, size);
    if (profile->status_kind == 2u && memcmp(status_source + 40u, "\x00\x00\x80\x3F", 4u) == 0)
        memcpy(status_after + 40u, "\x00\x00\x70\x41", 4u);
    *status_size = size;
    return 1;
}

static int hf_private_prepare_registry(const HfApi* api, uintptr_t game, size_t game_size,
                                       const HfPrivateRegistrySpec* spec,
                                       HfPrivateRegistryPlan* plan,
                                       const uintptr_t reusable_owned,
                                       char* reason, size_t capacity)
{
    uintptr_t source = 0u, original_source = 0u;
    memset(plan, 0, sizeof(*plan));
    plan->spec = spec;
    if (sizeof(uintptr_t) != 8u || spec->size == 0u ||
        spec->size > HF_PRIVATE_MAX_REGISTRY_SIZE || spec->patch_size == 0u ||
        spec->patch_size > HF_PRIVATE_MAX_REGISTRY_PATCH || spec->offset > spec->size ||
        spec->patch_size > spec->size - spec->offset ||
        !hf_registry_slot(api, game, game_size, spec->rva, spec->id, spec->size, &plan->slot) ||
        (plan->slot & 7u) != 0u ||
        !hf_private_page_access(plan->slot, sizeof(source), 1, 1) ||
        !hf_private_read(api, plan->slot, &source, sizeof(source)) ||
        !hf_registry_address(source, spec->size, 4u) ||
        !hf_private_read(api, game + spec->rva, &original_source, sizeof(original_source)) ||
        original_source != source ||
        !hf_private_page_access(source, spec->size, 0, 0) ||
        !hf_private_read(api, source, plan->before, spec->size) ||
        hf_private_u32(plan->before) != spec->id ||
        memcmp(plan->before, spec->original, spec->size) != 0 ||
        !hf_private_hash_matches(plan->before, spec->size, spec->sha256))
    {
        (void)snprintf(reason, capacity, "%s %s active registry pointer or source row changed",
                       spec->option, spec->label);
        return 0;
    }
    plan->source_row = source;
    memcpy(plan->after, plan->before, spec->size);
    memcpy(plan->after + spec->offset, spec->patch, spec->patch_size);
    if (reusable_owned != 0u)
    {
        uint8_t retained[HF_PRIVATE_MAX_REGISTRY_SIZE];
        if (hf_private_pointer_supported(reusable_owned) &&
            hf_private_read(api, reusable_owned, retained, spec->size) &&
            memcmp(retained, plan->after, spec->size) == 0)
            plan->owned_row = reusable_owned;
    }
    if (plan->owned_row == 0u)
    {
#if defined(HF_PORT_TEST)
        plan->owned_row = (uintptr_t)malloc(spec->size);
        if (plan->owned_row != 0u &&
            !hf_private_test_register_allocation(plan->owned_row, spec->size))
            plan->owned_row = 0u;
#else
        plan->owned_row = (uintptr_t)VirtualAlloc(NULL, spec->size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#endif
        if (plan->owned_row == 0u)
        {
            (void)snprintf(reason, capacity, "%s %s retained registry-row allocation failed",
                           spec->option, spec->label);
            return 0;
        }
        memcpy((void*)plan->owned_row, plan->after, spec->size);
    }
    {
        uint8_t retained[HF_PRIVATE_MAX_REGISTRY_SIZE];
        if (!hf_private_read(api, plan->owned_row, retained, spec->size) ||
            memcmp(retained, plan->after, spec->size) != 0)
        {
            (void)snprintf(reason, capacity, "%s %s retained registry row failed verification",
                           spec->option, spec->label);
            return 0;
        }
    }
    return 1;
}

static int hf_private_publish(const HfApi* api, uintptr_t slot,
                              uintptr_t expected, uintptr_t replacement)
{
    if ((slot & 7u) != 0u || !hf_private_page_access(slot, sizeof(uintptr_t), 1, 1))
        return 0;
#if defined(HF_PORT_TEST)
    uintptr_t current = 0u;
    if (!hf_private_read(api, slot, &current, sizeof(current)) || current != expected)
        return 0;
    return hf_private_write(api, slot, &replacement, sizeof(replacement)) &&
           hf_private_read(api, slot, &current, sizeof(current)) && current == replacement;
#else
    (void)api;
    return ((uintptr_t)InterlockedCompareExchangePointer((PVOID volatile*)slot,
            (PVOID)replacement, (PVOID)expected) == expected) &&
           (*(volatile uintptr_t*)slot == replacement);
#endif
}

static int hf_private_sources_intact(const HfApi* api, const HfPrivateProfileSpec* profile,
                                     uintptr_t game, size_t game_size,
                                     HfPrivateDataPlan* data, size_t data_count,
                                     HfPrivateRegistryPlan* registries, size_t registry_count)
{
    uint8_t actual[HF_PRIVATE_MAX_COMPONENT_SIZE];
    uintptr_t current = 0u;
    size_t i;
    if (!hf_private_pin_check(api, game, game_size, profile, (char[128]){0}, 128u))
        return 0;
    for (i = 0u; i < data_count; ++i)
    {
        if (!hf_private_data_proof(api, &data[i]) ||
            !hf_private_read(api, data[i].row, actual, data[i].size) ||
            memcmp(actual, data[i].before, data[i].size) != 0)
            return 0;
    }
    for (i = 0u; i < registry_count; ++i)
    {
        HfPrivateRegistryPlan* p = &registries[i];
        uintptr_t active_slot;
        if (!hf_registry_slot(api, game, game_size, p->spec->rva, p->spec->id, p->spec->size, &active_slot) ||
            active_slot != p->slot ||
            !hf_private_read(api, p->slot, &current, sizeof(current)) || current != p->source_row ||
            !hf_private_read(api, p->source_row, actual, p->spec->size) ||
            memcmp(actual, p->before, p->spec->size) != 0)
            return 0;
    }
    return 1;
}

static int hf_private_attempt_rollback(const HfApi* api, const HfPrivateAttempt* attempt)
{
    uint8_t current[HF_PRIVATE_MAX_COMPONENT_SIZE];
    uint8_t restored[HF_PRIVATE_MAX_COMPONENT_SIZE];
    size_t i;
    if (!hf_private_data_proof(api, &(HfPrivateDataPlan){
            .component = attempt->component, .row = attempt->row,
            .proof_slot = attempt->proof_slot, .is_status = attempt->component == NULL}) ||
        !hf_private_read(api, attempt->row, current, attempt->size))
        return 0;
    memcpy(restored, current, attempt->size);
    for (i = 0u; i < attempt->patch_size; ++i)
    {
        uint8_t byte = current[attempt->offset + i];
        if (byte != attempt->before[i] && byte != attempt->after[i])
            return 0;
        restored[attempt->offset + i] = attempt->before[i];
    }
    if (memcmp(restored, attempt->source, attempt->size) != 0 ||
        !hf_private_page_access(attempt->row, attempt->size, 0, 0) ||
        !hf_private_write(api, attempt->row + attempt->offset,
                          attempt->before, attempt->patch_size) ||
        !hf_private_read(api, attempt->row, current, attempt->size) ||
        memcmp(current, attempt->source, attempt->size) != 0)
        return 0;
    return 1;
}

static int hf_private_rollback(const HfApi* api,
                               uintptr_t game, size_t game_size,
                               HfPrivateRegistryPlan* registries, size_t published_count,
                               HfPrivateAttempt* attempts, size_t attempt_count)
{
    uintptr_t current = 0u;
    size_t i;
    int restored = 1;
    for (i = published_count; i > 0u; --i)
    {
        HfPrivateRegistryPlan* plan = &registries[i - 1u];
        uintptr_t active_slot = 0u;
        if (!hf_registry_slot(api, game, game_size, plan->spec->rva, plan->spec->id,
                               plan->spec->size, &active_slot) || active_slot != plan->slot)
            restored = 0;
        if (!hf_private_read(api, plan->slot, &current, sizeof(current)))
        {
            restored = 0;
            continue;
        }
        if (current == plan->owned_row)
        {
            if (!hf_private_publish(api, plan->slot, plan->owned_row, plan->source_row) ||
                !hf_private_read(api, plan->slot, &current, sizeof(current)) ||
                current != plan->source_row)
                restored = 0;
        }
        else if (current != plan->source_row)
            restored = 0; /* Preserve a foreign pointer that took ownership. */
    }
    for (i = attempt_count; i > 0u; --i)
        if (!hf_private_attempt_rollback(api, &attempts[i - 1u]))
            restored = 0;
    return restored;
}

static int hf_private_state_matches(const HfApi* api, const HfPrivateProfileSpec* profile,
                                    uintptr_t game, size_t game_size,
                                    const HfPrivateState* state)
{
    uintptr_t row = 0u;
    uintptr_t pointer = 0u;
    uint32_t stride = 0u;
    uint8_t current[HF_PRIVATE_MAX_COMPONENT_SIZE];
    size_t i;
    for (i = 0u; i < profile->component_count; ++i)
    {
        const HfPrivateComponentSpec* spec = &HF_PRIVATE_COMPONENTS[profile->component_start + i];
        row = api->component_record(spec->component, spec->resource, &stride);
        if (row == 0u || row != state->component_rows[i] || stride != spec->stride ||
            !hf_private_read(api, row, current, spec->stride) ||
            memcmp(current, state->component_after[i], spec->stride) != 0)
            return 0;
    }
    for (i = 0u; i < profile->registry_count; ++i)
    {
        const HfPrivateRegistrySpec* spec = &HF_PRIVATE_REGISTRIES[profile->registry_start + i];
        uintptr_t slot;
        if (!hf_registry_slot(api, game, game_size, spec->rva, spec->id, spec->size, &slot) ||
            !hf_private_read(api, slot, &pointer, sizeof(pointer)) || pointer != state->registry_owned[i] ||
            !hf_private_read(api, pointer, current, spec->size) ||
            memcmp(current, state->registry_after[i], spec->size) != 0)
            return 0;
    }
    if (profile->status_kind != 0u)
    {
        uint8_t status[HF_PRIVATE_MAX_STATUS_SIZE];
        if (!hf_private_range(game, game_size, profile->status_slot, sizeof(pointer), &pointer) ||
            !hf_private_read(api, pointer, &row, sizeof(row)) || row != state->status_row ||
            !hf_private_read(api, row, status, state->status_size) ||
            memcmp(status, state->status_after, state->status_size) != 0)
            return 0;
        if (profile->status_kind == 1u)
        {
            uint8_t proved[HF_PRIVATE_MAX_STATUS_SIZE];
            uint32_t proved_size = 0u;
            char reason[128];
            if (!hf_private_status_name(api, row, profile, proved, &proved_size,
                                        reason, sizeof(reason)) ||
                proved_size != state->status_size ||
                memcmp(proved, state->status_after, proved_size) != 0)
                return 0;
        }
    }
    return 1;
}

static int hf_private_state_is_base(const HfApi* api, const HfPrivateProfileSpec* profile,
                                    uintptr_t game, size_t game_size,
                                    const HfPrivateState* state)
{
    uintptr_t row = 0u;
    uintptr_t pointer = 0u;
    uintptr_t slot = 0u;
    uint32_t stride = 0u;
    uint8_t current[HF_PRIVATE_MAX_COMPONENT_SIZE];
    size_t i;
    for (i = 0u; i < profile->component_count; ++i)
    {
        const HfPrivateComponentSpec* spec = &HF_PRIVATE_COMPONENTS[profile->component_start + i];
        row = api->component_record(spec->component, spec->resource, &stride);
        if (row == 0u || stride != spec->stride ||
            !hf_private_read(api, row, current, spec->stride) ||
            !hf_private_hash_matches(current, spec->stride, spec->sha256))
            return 0;
    }
    for (i = 0u; i < profile->registry_count; ++i)
    {
        const HfPrivateRegistrySpec* spec = &HF_PRIVATE_REGISTRIES[profile->registry_start + i];
        if (!hf_registry_slot(api, game, game_size, spec->rva, spec->id, spec->size, &slot) ||
            !hf_private_read(api, slot, &pointer, sizeof(pointer)) ||
            pointer != state->registry_sources[i] ||
            !hf_private_read(api, pointer, current, spec->size) ||
            memcmp(current, spec->original, spec->size) != 0)
            return 0;
    }
    return 1;
}

static const HfPrivateProfileSpec* hf_private_find_profile(const char* option, size_t* index)
{
    size_t i;
    if (option == NULL)
        return NULL;
    for (i = 0u; i < HF_PRIVATE_PROFILE_COUNT; ++i)
    {
        if (strcmp(option, HF_PRIVATE_PROFILES[i].option) == 0)
        {
            if (index != NULL)
                *index = i;
            return &HF_PRIVATE_PROFILES[i];
        }
    }
    return NULL;
}

static int hf_private_apply_profile(const HfApi* api, HfMod* mod, const char* option,
                                    char* reason, size_t capacity)
{
    const HfPrivateProfileSpec* profile;
    HfPrivateDataPlan data[HF_PRIVATE_MAX_DATA_PLANS];
    HfPrivateRegistryPlan registries[HF_PRIVATE_MAX_REGISTRIES];
    HfPrivateAttempt attempts[HF_PRIVATE_MAX_ATTEMPTS];
    HfPrivateState* state;
    uintptr_t game = 0u;
    size_t game_size = 0u;
    size_t profile_index = 0u;
    size_t data_count = 0u;
    size_t registry_count = 0u;
    size_t attempt_count = 0u;
    size_t published_count = 0u;
    uint8_t status_after[HF_PRIVATE_MAX_STATUS_SIZE];
    uint32_t status_size = 0u;
    size_t i;
    int failed = 0;

    if (reason != NULL && capacity > 0u)
        reason[0] = '\0';
    profile = hf_private_find_profile(option, &profile_index);
    if (profile == NULL || api == NULL || api->size < (uint32_t)(offsetof(HfApi, component_record) + sizeof(api->component_record)) ||
        api->component_record == NULL || api->module_base == NULL || api->read == NULL || api->write == NULL ||
        sizeof(uintptr_t) != 8u)
    {
        (void)snprintf(reason, capacity, "%s private-profile APIs are unavailable", option != NULL ? option : "Native");
        return 0;
    }
    state = &HF_PRIVATE_STATES[profile_index];
    state->option = profile->option;
    if (state->poisoned != 0u)
    {
        (void)snprintf(reason, capacity, "%s private-profile transaction is poisoned after incomplete rollback", option);
        return -1;
    }
    game = api->module_base("game.dll", &game_size);
    if (game == 0u || game_size == 0u ||
        !hf_private_pin_check(api, game, game_size, profile, reason, capacity))
        return 0;
    if (state->applied != 0u)
    {
        if (hf_private_state_matches(api, profile, game, game_size, state))
        {
            (void)snprintf(reason, capacity, "%s accepted native profile is already applied", option);
            return 2;
        }
        if (!hf_private_state_is_base(api, profile, game, game_size, state))
        {
            (void)snprintf(reason, capacity, "%s changed after application; foreign values were preserved", option);
            return 0;
        }
        state->applied = 0u;
    }

    if (profile->component_count > HF_PRIVATE_MAX_COMPONENTS ||
        profile->registry_count > HF_PRIVATE_MAX_REGISTRIES)
    {
        (void)snprintf(reason, capacity, "%s profile exceeds native transaction bounds", option);
        return 0;
    }
    for (i = 0u; i < profile->component_count; ++i)
    {
        if (data_count >= HF_PRIVATE_MAX_DATA_PLANS ||
            !hf_private_prepare_component(api,
                &HF_PRIVATE_COMPONENTS[profile->component_start + i], &data[data_count],
                reason, capacity))
            return 0;
        ++data_count;
    }
    if (profile->status_kind != 0u)
    {
        HfPrivateDataPlan* status_plan = &data[data_count];
        if (data_count >= HF_PRIVATE_MAX_DATA_PLANS)
        {
            (void)snprintf(reason, capacity, "%s status plan exceeds native transaction bounds", option);
            return 0;
        }
        if (!hf_private_prepare_status(api, game, game_size, profile, status_plan,
                                       status_after, &status_size, reason, capacity))
            return 0;
        ++data_count;
    }
    for (i = 0u; i < profile->registry_count; ++i)
    {
        const HfPrivateRegistrySpec* spec = &HF_PRIVATE_REGISTRIES[profile->registry_start + i];
        uintptr_t reusable = 0u;
        if (state->registry_owned[i] != 0u)
            reusable = state->registry_owned[i];
        if (!hf_private_prepare_registry(api, game, game_size, spec,
                                         &registries[registry_count], reusable,
                                         reason, capacity))
            return 0;
        ++registry_count;
    }
    if (!hf_private_sources_intact(api, profile, game, game_size,
                                  data, data_count, registries, registry_count))
    {
        (void)snprintf(reason, capacity, "%s source rows changed during preflight; no edit applied", option);
        return 0;
    }

    for (i = 0u; i < data_count; ++i)
    {
        HfPrivateDataPlan* plan = &data[i];
        size_t j;
        uint8_t current[HF_PRIVATE_MAX_COMPONENT_SIZE];
        for (j = 0u; j < plan->patch_count; ++j)
        {
            HfPrivatePatch* patch = &plan->patches[j];
            HfPrivateAttempt* attempt;
            if (attempt_count >= HF_PRIVATE_MAX_ATTEMPTS ||
                !hf_private_data_proof(api, plan) ||
                !hf_private_page_access(plan->row, plan->size, 0, 0) ||
                !hf_private_read(api, plan->row, current, plan->size) ||
                memcmp(current, plan->before, plan->size) != 0)
            {
                failed = 1;
                (void)snprintf(reason, capacity, "%s ownership changed before editing %s",
                               option, plan->label);
                break;
            }
            attempt = &attempts[attempt_count++];
            memset(attempt, 0, sizeof(*attempt));
            attempt->row = plan->row;
            attempt->proof_slot = plan->proof_slot;
            attempt->size = plan->size;
            attempt->offset = patch->offset;
            attempt->patch_size = patch->size;
            attempt->component = plan->component;
            memcpy(attempt->source, plan->before, plan->size);
            memcpy(attempt->before, plan->before + patch->offset, patch->size);
            memcpy(attempt->after, patch->bytes, patch->size);
            if (!hf_private_write(api, plan->row + patch->offset, patch->bytes, patch->size))
            {
                failed = 1;
                (void)snprintf(reason, capacity, "%s %s write failed", option, plan->label);
                break;
            }
            memcpy(plan->before + patch->offset, patch->bytes, patch->size);
            if (!hf_private_read(api, plan->row, current, plan->size) ||
                memcmp(current, plan->before, plan->size) != 0)
            {
                failed = 1;
                (void)snprintf(reason, capacity, "%s %s write did not verify", option, plan->label);
                break;
            }
        }
        if (failed)
            break;
        memcpy(plan->after, plan->before, plan->size);
        /* Keep the original snapshot for full transaction proof and rollback attempts. */
        for (j = 0u; j < plan->patch_count; ++j)
        {
            HfPrivatePatch* patch = &plan->patches[j];
            memcpy(plan->before + patch->offset, attempts[attempt_count - plan->patch_count + j].before,
                   patch->size);
        }
    }

    if (!failed)
    {
        for (i = 0u; i < registry_count; ++i)
        {
            HfPrivateRegistryPlan* plan = &registries[i];
            uintptr_t current = 0u;
            uintptr_t active_slot = 0u;
            uint8_t verify[HF_PRIVATE_MAX_REGISTRY_SIZE];
            published_count = i + 1u;
            if (!hf_registry_slot(api, game, game_size, plan->spec->rva, plan->spec->id,
                                   plan->spec->size, &active_slot) || active_slot != plan->slot ||
                !hf_private_read(api, plan->slot, &current, sizeof(current)) || current != plan->source_row ||
                !hf_private_read(api, plan->source_row, verify, plan->spec->size) ||
                memcmp(verify, plan->before, plan->spec->size) != 0 ||
                !hf_private_read(api, plan->owned_row, verify, plan->spec->size) ||
                memcmp(verify, plan->after, plan->spec->size) != 0 ||
                !hf_private_publish(api, plan->slot, plan->source_row, plan->owned_row))
            {
                failed = 1;
                (void)snprintf(reason, capacity, "%s %s registry publication failed", option, plan->spec->label);
                break;
            }
        }
    }

    if (!failed)
    {
        for (i = 0u; i < data_count; ++i)
        {
            uint8_t current[HF_PRIVATE_MAX_COMPONENT_SIZE];
            if (!hf_private_data_proof(api, &data[i]) ||
                !hf_private_read(api, data[i].row, current, data[i].size) ||
                memcmp(current, data[i].after, data[i].size) != 0)
            {
                failed = 1;
                (void)snprintf(reason, capacity, "%s full component/status verification failed at %s",
                               option, data[i].label);
                break;
            }
        }
    }
    if (!failed)
    {
        for (i = 0u; i < registry_count; ++i)
        {
            uintptr_t current = 0u;
            uintptr_t active_slot = 0u;
            uint8_t verify[HF_PRIVATE_MAX_REGISTRY_SIZE];
            if (!hf_registry_slot(api, game, game_size, registries[i].spec->rva,
                                   registries[i].spec->id, registries[i].spec->size, &active_slot) ||
                active_slot != registries[i].slot ||
                !hf_private_read(api, registries[i].slot, &current, sizeof(current)) ||
                current != registries[i].owned_row ||
                !hf_private_read(api, current, verify, registries[i].spec->size) ||
                memcmp(verify, registries[i].after, registries[i].spec->size) != 0 ||
                !hf_private_read(api, registries[i].source_row, verify, registries[i].spec->size) ||
                memcmp(verify, registries[i].before, registries[i].spec->size) != 0)
            {
                failed = 1;
                (void)snprintf(reason, capacity, "%s full active registry verification failed", option);
                break;
            }
        }
    }

    if (failed)
    {
        if (!hf_private_rollback(api, game, game_size, registries, published_count, attempts, attempt_count))
        {
            state->poisoned = 1u;
            (void)snprintf(reason, capacity, "FATAL: %s failed and rollback could not preserve all foreign state", option);
            return -1;
        }
        return 0;
    }

    state->component_count = profile->component_count;
    for (i = 0u; i < profile->component_count; ++i)
    {
        state->component_rows[i] = data[i].row;
        memcpy(state->component_after[i], data[i].after, data[i].size);
    }
    state->registry_count = registry_count;
    for (i = 0u; i < registry_count; ++i)
    {
        state->registry_slots[i] = registries[i].slot;
        state->registry_sources[i] = registries[i].source_row;
        state->registry_owned[i] = registries[i].owned_row;
        memcpy(state->registry_after[i], registries[i].after, registries[i].spec->size);
    }
    if (profile->status_kind != 0u)
    {
        const HfPrivateDataPlan* status_plan = &data[data_count - 1u];
        state->status_row = status_plan->row;
        state->status_slot = status_plan->proof_slot;
        state->status_size = status_size;
        memcpy(state->status_after, status_after, status_size);
    }
    state->applied = 1u;
    (void)snprintf(reason, capacity, "%s source-defined native profile applied with guarded registry clones", option);
    (void)mod;
    return 1;
}
