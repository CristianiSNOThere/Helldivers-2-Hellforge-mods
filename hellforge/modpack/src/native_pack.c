/* Native C runtime for the selectable Super Earth Arsenal pack.
 * Catalogue-resolved scalar changes live in port-edits.generated.h. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hellforge_mod.h"
#include "component_lookup.h"
static int hf_component_api_read(void* context,uintptr_t at,void* bytes,size_t size) {
    const HfApi* api=context;return api->read(at,bytes,size);
}
#include "ac8_source_guard.generated.h"
#include "epoch_source_guard.generated.h"
#include "vg70_profile.h"
#include "vg70_source_guard.generated.h"
#include "vg70_profile.inc"
#ifndef HF_PORT_TEST
#include "pack_config.h"
#else
#define HF_PORT_FLAG_NORMAL_DAMAGE 300u
#define HF_PORT_FLAG_DURABLE_DAMAGE 150u
#ifdef HF_PORT_TEST_FULL
#define HF_PORT_PLAS_45_EPOCH 1u
#define HF_PORT_SICKLE_LASER_AR 1u
#else
#define HF_PORT_PLAS_45_EPOCH 0u
#define HF_PORT_SICKLE_LASER_AR 0u
#endif
#if defined(HF_PORT_TEST_PLAS39) || defined(HF_PORT_TEST_NATIVE_MODULES)
#define HF_PORT_PLAS_39_ACCELERATOR_RIFLE 1u
#else
#define HF_PORT_PLAS_39_ACCELERATOR_RIFLE 0u
#endif
#endif

#ifndef HF_PORT_VG_70_VARIABLE
#define HF_PORT_VG_70_VARIABLE 0u
#endif
#ifndef HF_PORT_ARC_3_RAPID_SUPERCHARGE
#define HF_PORT_ARC_3_RAPID_SUPERCHARGE 0u
#endif
#ifndef HF_PORT_PLAS_39_ACCELERATOR_RIFLE
#define HF_PORT_PLAS_39_ACCELERATOR_RIFLE 0u
#endif
#ifndef HF_PORT_GAS_OVERHAUL
#define HF_PORT_GAS_OVERHAUL 0u
#endif
#ifndef HF_PORT_PUNISHER_PLASMA
#define HF_PORT_PUNISHER_PLASMA 0u
#endif
#ifndef HF_PORT_ADJUDICATOR_HEAVY
#define HF_PORT_ADJUDICATOR_HEAVY 0u
#endif
#ifndef HF_PORT_GL52_DE_ESCALATOR
#define HF_PORT_GL52_DE_ESCALATOR 0u
#endif
#ifndef HF_PORT_TENDERIZER_ARMOR_SOFTENER
#define HF_PORT_TENDERIZER_ARMOR_SOFTENER 0u
#endif

#if defined(HF_PORT_TEST) && !defined(HF_PORT_TEST_NATIVE_MODULES) && \
    !defined(HF_PORT_TEST_PUNISHER_LOADER)
#define HF_PORT_ALLOW_NATIVE_RUNTIME 0u
#else
#define HF_PORT_ALLOW_NATIVE_RUNTIME 1u
#endif

#include "registry_bridge.inc"
#include "sickle_port.inc"
#if HF_PORT_PLAS_15_LOYALIST
#include "loyalist_magazine_guard.generated.h"
#include "loyalist_sway_guard.generated.h"
#endif
#if HF_PORT_SICKLE_LASER_AR && HF_PORT_ALLOW_NATIVE_RUNTIME
#include "heatsink_runtime.inc"
#endif
#if (!defined(HF_PORT_TEST) && \
     (HF_PORT_ADJUDICATOR_HEAVY || HF_PORT_GL52_DE_ESCALATOR || HF_PORT_TENDERIZER_ARMOR_SOFTENER)) || \
    defined(HF_PORT_TEST_PRIVATE_PROFILES)
#include "private_profiles.inc"
#endif
#if HF_PORT_PUNISHER_PLASMA && HF_PORT_ALLOW_NATIVE_RUNTIME
#include "punisher_port.inc"
#endif
#if HF_PORT_PLAS_1_SCORCHER_AR && HF_PORT_ALLOW_NATIVE_RUNTIME
#include "scorcher_private.inc"
#endif
#if HF_PORT_RS_422_RAILGUN && HF_PORT_ALLOW_NATIVE_RUNTIME
#include "railgun_private.inc"
#include "railgun_curve.inc"
#endif
#if HF_PORT_GAS_OVERHAUL || defined(HF_PORT_TEST_GAS)
#include "gas_port.inc"
#endif
#if HF_PORT_ARC_3_RAPID_SUPERCHARGE && HF_PORT_ALLOW_NATIVE_RUNTIME
#include "arc3_source_guard.generated.h"
#include "arc3_port.inc"
#include "arc3_supercharge.inc"
#include "arc3_runtime.inc"
#endif
#if HF_PORT_PLAS_39_ACCELERATOR_RIFLE && HF_PORT_ALLOW_NATIVE_RUNTIME
#include "plas39_source_guard.generated.h"
#include "plas39_native.inc"
#endif

typedef enum HfPortEditKind
{
    PORT_EDIT_LIVE = 1,
    PORT_EDIT_STAT = 2,
    PORT_EDIT_GAS_PLAYER_HEALTH = 3
} HfPortEditKind;

typedef struct HfPortEdit
{
    HfPortEditKind kind;
    const char* option;
    HfRef target;
    HfField field;
    HfStat stat;
    HfValue value;
    HfValue expected_base;
    uint32_t has_expected_base;
    uint32_t flags;
    const char* label;
} HfPortEdit;

typedef struct HfSaiCodePin
{
    uint32_t rva;
    uint8_t size;
    uint8_t bytes[9];
} HfSaiCodePin;

/* Exact build-25480438 projectile-physics fingerprints from source/sai-focus-precision/entry.lua. */
static const HfSaiCodePin HF_SAI_BALLISTIC_PINS[] = {
    {0x013AA095u, 5u, {0xF3, 0x0F, 0x10, 0x40, 0x18}},
    {0x013AA0B6u, 5u, {0xF3, 0x0F, 0x59, 0x40, 0x28}},
    {0x013AAF54u, 9u, {0xF3, 0x0F, 0x5E, 0xB4, 0x24, 0x00, 0x01, 0x00, 0x00}},
    {0x013AD060u, 5u, {0xF3, 0x0F, 0x5E, 0x4F, 0x2C}},
    {0x013AD06Eu, 4u, {0xF3, 0x0F, 0x59, 0xC9}},
    {0x013AD07Bu, 9u, {0xF3, 0x44, 0x0F, 0x59, 0x15, 0x3C, 0x98, 0x01, 0x01}},
    {0x013AD084u, 5u, {0xF3, 0x44, 0x0F, 0x58, 0xD1}},
};
#define HF_SAI_BALLISTIC_PIN_COUNT (sizeof(HF_SAI_BALLISTIC_PINS) / sizeof(HF_SAI_BALLISTIC_PINS[0]))
static uint32_t HF_EPOCH_BURNING_DAMAGE_ID;
static uint32_t HF_NATIVE_READY_EVENTS;
static int HF_NATIVE_READY_CALLBACK_APPLIED;

#define HF_STATS_RETRY_MAX_MS UINT64_C(30000)

#if defined(HF_PORT_TEST)
static uint64_t HF_PORT_TEST_CLOCK_MS;
static uint64_t hf_port_monotonic_ms(void) { return HF_PORT_TEST_CLOCK_MS; }
#else
static uint64_t hf_port_monotonic_ms(void) { return (uint64_t)GetTickCount64(); }
#endif

#define HF_NATIVE_READY_NETWORK 0x01u
#define HF_NATIVE_READY_ENTITIES 0x02u
#define HF_NATIVE_READY_PROJECTILES 0x04u
#define HF_NATIVE_READY_ALL (HF_NATIVE_READY_NETWORK | HF_NATIVE_READY_ENTITIES | HF_NATIVE_READY_PROJECTILES)
#define HF_AC8_RESOURCE UINT64_C(0xA8CFFB316F0B5C5F)

typedef struct HfAc8RawEdit
{
    uintptr_t address;
    uint8_t size;
    uint8_t before[4];
    uint8_t after[4];
    const char* label;
} HfAc8RawEdit;

#if defined(HF_PORT_TEST_EMPTY_EDITS)
static const HfPortEdit HF_PORT_EDITS[1] = {{0}};
#define HF_PORT_EDIT_COUNT 0u
#elif defined(HF_PORT_TEST_FULL)
#include "../tests/port_edits_full.generated.h"
#elif defined(HF_PORT_TEST)
#include "../tests/port_edits_test.generated.h"
#else
#include "port_edits.generated.h"
#endif

#if HF_PORT_GAS_OVERHAUL || defined(HF_PORT_TEST_GAS)
#include "gas_health.inc"
#include "gas_perk.inc"
#endif

#include "sai_attachment_native.inc"

typedef struct HfStatsRetryState
{
    const HfApi* api;
    HfMod* mod;
    const char* options[HF_PORT_EDIT_COUNT > 0 ? HF_PORT_EDIT_COUNT : 1];
    size_t count;
    uint64_t deadline_ms;
    int registered;
} HfStatsRetryState;
static HfStatsRetryState HF_STATS_RETRY;

static int same_value(HfValue left, HfValue right)
{
    if (left.type != right.type)
        return 0;
    if (left.type == HF_TYPE_U64)
        return left.u64 == right.u64;
    if (left.type == HF_TYPE_F32)
        return (float)left.number == (float)right.number;
    return left.number == right.number;
}

static int preflight_one(const HfApi* api, HfMod* mod, const HfPortEdit* edit,
                         int* stats_not_ready, char* reason, size_t capacity)
{
    HfLiveValue current;
    int ready;

    if (stats_not_ready != NULL)
        *stats_not_ready = 0;
    if (hf_sai_attachment_field(edit) >= 0)
        return hf_sai_attachment_preflight(api,mod,stats_not_ready,reason,capacity);
#if HF_PORT_PLAS_15_LOYALIST
    if (edit->kind==PORT_EDIT_LIVE && !strcmp(edit->option,"plas-15-loyalist") && !(edit->field.offset==104u ? hf_loyalist_sway_source(api,0) : hf_loyalist_magazine_source(api,0))) {
        (void)snprintf(reason,capacity,"Loyalist magazine source/ownership differs");return 0;
    }
#endif
    memset(&current, 0, sizeof(current));
#if HF_PORT_GAS_OVERHAUL || defined(HF_PORT_TEST_GAS)
    if (edit->kind == PORT_EDIT_GAS_PLAYER_HEALTH)
        return hf_gas_player_health_preflight(api, edit, reason, capacity);
#endif
    if (edit->kind == PORT_EDIT_LIVE)
        ready = api->get_live != NULL && api->get_live(mod, edit->target, &edit->field, &current) != 0;
    else if (edit->kind == PORT_EDIT_STAT)
    {
        ready = api->get_stat != NULL && api->get_stat(mod, &edit->stat, &current) != 0;
        if (!ready && api->get_stat != NULL && stats_not_ready != NULL)
            *stats_not_ready = 1;
    }
    else
        ready = 0;

#if HF_PORT_GAS_OVERHAUL || defined(HF_PORT_TEST_GAS)
    if (!ready && hf_gas_perk_field(edit)) return hf_gas_perk_preflight(api,reason,capacity);
#endif
    if (!ready)
    {
        (void)snprintf(reason, capacity, "the current field or table is unavailable");
        return 0;
    }
    if (current.mod != NULL && current.mod[0] != '\0')
    {
        (void)snprintf(reason, capacity, "the base value is already owned by mod %s", current.mod);
        return 0;
    }
    if (!same_value(current.now, current.base))
    {
        (void)snprintf(reason, capacity, "the field already differs from its unmodified base value");
        return 0;
    }
    if (edit->has_expected_base != 0u && !same_value(current.base, edit->expected_base))
    {
        (void)snprintf(reason, capacity, "the unmodified base value differs from the reviewed source value");
        return 0;
    }
    return 1;
}

static void ApplyReadyFiltered(HfMod* mod, HfEvent event, void* user, const char* only_option);
static void hf_stats_retry_tick(HfMod* mod, float dt, void* user);

static size_t hf_stats_retry_find(const char* option)
{
    size_t i;
    if (option == NULL)
        return HF_STATS_RETRY.count;
    for (i = 0u; i < HF_STATS_RETRY.count; ++i)
        if (strcmp(HF_STATS_RETRY.options[i], option) == 0)
            return i;
    return HF_STATS_RETRY.count;
}

static void hf_stats_retry_remove(const char* option)
{
    size_t index = hf_stats_retry_find(option);
    if (index >= HF_STATS_RETRY.count)
        return;
    while (index + 1u < HF_STATS_RETRY.count)
    {
        HF_STATS_RETRY.options[index] = HF_STATS_RETRY.options[index + 1u];
        ++index;
    }
    --HF_STATS_RETRY.count;
    if (HF_STATS_RETRY.count == 0u)
    {
        HF_STATS_RETRY.deadline_ms = 0u;
    }
}

static void hf_stats_retry_add(const char* option)
{
    if (option == NULL || hf_stats_retry_find(option) < HF_STATS_RETRY.count ||
        HF_STATS_RETRY.count >= HF_PORT_EDIT_COUNT)
        return;
    if (HF_STATS_RETRY.count == 0u)
    {
        HF_STATS_RETRY.deadline_ms = hf_port_monotonic_ms() + HF_STATS_RETRY_MAX_MS;
    }
    HF_STATS_RETRY.options[HF_STATS_RETRY.count++] = option;
}

static void hf_stats_retry_start(HfMod* mod, const HfApi* api)
{
    size_t i;
    if (HF_STATS_RETRY.count == 0u || HF_STATS_RETRY.registered)
        return;
    if (api == NULL)
    {
        HF_STATS_RETRY.count = 0u;
        return;
    }
    if (api->size < (uint32_t)(offsetof(HfApi, on_game_thread) + sizeof(api->on_game_thread)) ||
        api->on_game_thread == NULL)
    {
        for (i = 0u; i < HF_STATS_RETRY.count; ++i)
            api->log(mod, "Refused %s: stats snapshot field is unreadable and HellForge game-thread retry is unavailable",
                     HF_STATS_RETRY.options[i]);
        HF_STATS_RETRY.count = 0u;
        return;
    }
    HF_STATS_RETRY.api = api;
    HF_STATS_RETRY.mod = mod;
    if (api->on_game_thread(mod, hf_stats_retry_tick, &HF_STATS_RETRY) == 0)
    {
        for (i = 0u; i < HF_STATS_RETRY.count; ++i)
            api->log(mod, "Refused %s: stats snapshot field is unreadable and HellForge game-thread retry could not be registered",
                     HF_STATS_RETRY.options[i]);
        HF_STATS_RETRY.count = 0u;
        return;
    }
    HF_STATS_RETRY.registered = 1;
    api->log(mod, "Queued %u option groups until their stats snapshot fields are readable (bounded wait: 30 seconds)",
             (unsigned)HF_STATS_RETRY.count);
}

static void hf_stats_retry_tick(HfMod* mod, float dt, void* user)
{
    HfStatsRetryState* retry = (HfStatsRetryState*)user;
    size_t index = 0u;
    (void)dt;
    if (retry == NULL || retry != &HF_STATS_RETRY || retry->api == NULL || retry->count == 0u)
        return;
    if (hf_port_monotonic_ms() >= retry->deadline_ms)
    {
        for (index = 0u; index < retry->count; ++index)
            retry->api->log(mod, "Refused %s: stats snapshot fields remained unavailable after a bounded 30-second wait",
                            retry->options[index]);
        retry->count = 0u;
        retry->deadline_ms = 0u;
        return;
    }
    while (index < retry->count)
    {
        const char* option = retry->options[index];
        ApplyReadyFiltered(mod, HF_EVENT_ENTITIES_READY, (void*)retry->api, option);
        if (index < retry->count && strcmp(retry->options[index], option) == 0)
            ++index;
    }
}


#define HF_AC8_ENTITY_MANAGER_RVA UINT64_C(0x346BF98)
#define HF_AC8_ENTITY_MAP_RVA UINT64_C(0xF12BD8)
#define HF_AC8_ENTITY_MAP_BIAS UINT64_C(26151684)

static int ac8_safe_address(uintptr_t base, uint64_t offset, size_t size, uintptr_t* output)
{
    uint64_t address;
    const uint64_t limit = UINT64_C(0x800000000000);
    if (output == NULL || base < (uintptr_t)0x10000u ||
        (uint64_t)base > UINT64_MAX - offset)
        return 0;
    address = (uint64_t)base + offset;
    if (address < UINT64_C(0x10000) || address >= limit ||
        (uint64_t)size > limit - address || address > (uint64_t)UINTPTR_MAX)
        return 0;
    *output = (uintptr_t)address;
    return 1;
}

static int ac8_store_scalar(uint8_t* output, const HfField* field, HfValue value)
{
    if (field->type != value.type) return 0;
    if (field->type == HF_TYPE_U32 && field->size == 4u)
    {
        uint32_t number = (uint32_t)value.number;
        memcpy(output, &number, sizeof(number));
        return 1;
    }
    if (field->type == HF_TYPE_F32 && field->size == 4u)
    {
        float number = (float)value.number;
        memcpy(output, &number, sizeof(number));
        return 1;
    }
    return 0;
}

/* Called only after this transaction's scalar writes succeeded. HellForge reports our
 * live setting as the new owned base; require that exact owner and value on read-back. */
static int ac8_component_matches(const HfApi* api, HfMod* edited_mod,
                                  const HfAc8ComponentFingerprint* source, const uint8_t* actual)
{
    uint8_t expected[1232];
    char component_name[96];
    size_t i;
    uint64_t resource;
    if (source->size > sizeof(expected)) return 0;
    memcpy(expected, source->before, source->size);
    memcpy(&resource, source->owner, sizeof(resource));
    (void)snprintf(component_name, sizeof(component_name), "%sData", source->name);
    if (edited_mod != NULL)
    {
        for (i = 0u; i < HF_PORT_EDIT_COUNT; ++i)
        {
            const HfPortEdit* edit = &HF_PORT_EDITS[i];
            HfLiveValue current;
            if (edit->kind != PORT_EDIT_LIVE || strcmp(edit->option, "ac-8-autocannon") != 0 ||
                edit->target != resource || edit->field.component == NULL ||
                strcmp(edit->field.component, component_name) != 0) continue;
            memset(&current, 0, sizeof(current));
            if (edit->field.offset > source->size || edit->field.size > source->size-edit->field.offset ||
                api->get_live == NULL || api->get_live(edited_mod, edit->target, &edit->field, &current) == 0 ||
                current.mod == NULL || strcmp(current.mod, "superearth.arsenal-native-port") != 0 ||
                !same_value(current.now, edit->value) || !edit->has_expected_base ||
                !same_value(current.base, edit->value) ||
                !ac8_store_scalar(expected + edit->field.offset, &edit->field, edit->value))
            {
                api->log(edited_mod, "AC-8 scalar read-back proof failed for %s +%u: owner=%s, now=%g (type %u), base=%g (type %u), expected=%g/%g",
                         component_name, (unsigned)edit->field.offset, current.mod ? current.mod : "stock",
                         current.now.number, current.now.type, current.base.number, current.base.type,
                         edit->value.number, edit->expected_base.number);
                return 0;
            }
        }
    }
    if (memcmp(actual, expected, source->size) == 0) return 1;
    if (edited_mod != NULL)
        for (i = 0u; i < source->size; ++i)
            if (actual[i] != expected[i])
            {
                api->log(edited_mod, "AC-8 component read-back differs for %s +%u: found %02X, expected %02X",
                         source->name, (unsigned)i, actual[i], expected[i]);
                break;
            }
    return 0;
}

static int ac8_resolve_component_records(const HfApi* api, HfMod* edited_mod, uintptr_t* game_base_out,
                                         size_t* image_size_out, uintptr_t* entities_out,
                                         uintptr_t records[], char* reason, size_t capacity)
{
    uint64_t owner_value = 0u;
    uint64_t map_value = 0u;
    uintptr_t game_base;
    uintptr_t owner_field;
    uintptr_t owner;
    uintptr_t map_field;
    uintptr_t entities;
    size_t image_size = 0u;
    size_t i;
    uint8_t component_bytes[1232];
    const size_t component_count = sizeof(HF_AC8_COMPONENTS) / sizeof(HF_AC8_COMPONENTS[0]);

    if (api == NULL || api->module_base == NULL || api->read == NULL || records == NULL)
    {
        (void)snprintf(reason, capacity, "the game-module or raw-memory API is unavailable for AC-8 source validation");
        return 0;
    }
    game_base = api->module_base("game.dll", &image_size);
    if (game_base == 0u || image_size == 0u ||
        HF_AC8_ENTITY_MANAGER_RVA > (uint64_t)image_size ||
        sizeof(owner_value) > image_size - (size_t)HF_AC8_ENTITY_MANAGER_RVA ||
        !ac8_safe_address(game_base, HF_AC8_ENTITY_MANAGER_RVA, sizeof(owner_value), &owner_field))
    {
        (void)snprintf(reason, capacity, "game.dll cannot safely resolve the AC-8 entity-manager pointer");
        return 0;
    }
    if (api->read(owner_field, &owner_value, sizeof(owner_value)) != 0 ||
        owner_value < UINT64_C(0x10000) || owner_value >= UINT64_C(0x800000000000) ||
        owner_value > (uint64_t)UINTPTR_MAX)
    {
        (void)snprintf(reason, capacity, "the AC-8 entity-manager pointer is unavailable or invalid");
        return 0;
    }
    owner = (uintptr_t)owner_value;
    if (!ac8_safe_address(owner, HF_AC8_ENTITY_MAP_RVA, sizeof(map_value), &map_field) ||
        api->read(map_field, &map_value, sizeof(map_value)) != 0 ||
        map_value < HF_AC8_ENTITY_MAP_BIAS || map_value >= UINT64_C(0x800000000000) ||
        map_value > (uint64_t)UINTPTR_MAX)
    {
        (void)snprintf(reason, capacity, "the AC-8 entity owner-map pointer is unavailable or invalid");
        return 0;
    }
    map_value -= HF_AC8_ENTITY_MAP_BIAS;
    if (map_value < UINT64_C(0x10000) || map_value >= UINT64_C(0x800000000000) ||
        map_value > (uint64_t)UINTPTR_MAX)
    {
        (void)snprintf(reason, capacity, "the AC-8 entity base derived from the owner map is invalid");
        return 0;
    }
    entities = (uintptr_t)map_value;

    for (i = 0u; i < component_count; ++i)
    {
        const HfAc8ComponentFingerprint* expected = &HF_AC8_COMPONENTS[i];
        uintptr_t row_address;
        if (!hf_component_record(hf_component_api_read,(void*)api,owner,expected->group_offset,
                expected->header,expected->map_offset,expected->owner,expected->size,&row_address)) {
            (void)snprintf(reason,capacity,"the AC-8 %s active table or exact owner mapping changed",expected->name);
            return 0;
        }
        if (expected->size > sizeof(component_bytes) ||
            api->read(row_address, component_bytes, expected->size) != 0 ||
            !ac8_component_matches(api, edited_mod, expected, component_bytes))
        {
            (void)snprintf(reason, capacity, "the AC-8 %s exact owner row differs from its accepted source",
                           expected->name);
            return 0;
        }
        records[i] = row_address;
    }
    if (game_base_out != NULL)
        *game_base_out = game_base;
    if (image_size_out != NULL)
        *image_size_out = image_size;
    if (entities_out != NULL)
        *entities_out = entities;
    return 1;
}

static uintptr_t ac8_component_by_name(const uintptr_t records[], const char* name)
{
    size_t i;
    if (records == NULL || name == NULL)
        return 0u;
    for (i = 0u; i < sizeof(HF_AC8_COMPONENTS) / sizeof(HF_AC8_COMPONENTS[0]); ++i)
        if (strcmp(HF_AC8_COMPONENTS[i].name, name) == 0)
            return records[i];
    return 0u;
}

static int preflight_ac8_source_state(const HfApi* api, char* reason, size_t capacity);

static int preflight_ac8_extension(const HfApi* api, char* reason, size_t capacity)
{
    uintptr_t reload;
    uintptr_t assisted;
    uint8_t allow_move = 0xFFu;
    uint32_t ability_primary = UINT32_MAX;
    uint32_t ability_secondary = UINT32_MAX;
    uintptr_t records[sizeof(HF_AC8_COMPONENTS) / sizeof(HF_AC8_COMPONENTS[0])];

    if (api->read == NULL || api->write == NULL || api->module_base == NULL)
    {
        (void)snprintf(reason, capacity, "the HellForge raw-memory API is unavailable");
        return 0;
    }
    if (!preflight_ac8_source_state(api, reason, capacity))
        return 0;
    if (!ac8_resolve_component_records(api, NULL, NULL, NULL, NULL, records, reason, capacity))
        return 0;
    reload = ac8_component_by_name(records, "WeaponReloadComponent");
    if (reload == 0u || api->read(reload + 1u, &allow_move, sizeof(allow_move)) != 0 || allow_move != 0u)
    {
        (void)snprintf(reason, capacity, "the AC-8 reload record or its reviewed mobile-reload base is unavailable");
        return 0;
    }
    assisted = ac8_component_by_name(records, "WeaponAssistedReloadComponent");
    if (assisted == 0u)
    {
        (void)snprintf(reason, capacity, "the AC-8 assisted-reload component is missing from its accepted owner map");
        return 0;
    }
    if (api->read(assisted, &ability_primary, sizeof(ability_primary)) != 0 ||
        api->read(assisted + 4u, &ability_secondary, sizeof(ability_secondary)) != 0)
    {
        (void)snprintf(reason, capacity, "the AC-8 assisted-reload row could not be read");
        return 0;
    }
    if (ability_primary != 2622u || ability_secondary != 2621u)
    {
        (void)snprintf(reason, capacity,
                       "AC-8 assisted-reload ability IDs changed (expected 2622/2621, found %u/%u)",
                       ability_primary, ability_secondary);
        return 0;
    }
    return 1;
}

static int rollback_ac8_raw(const HfApi* api, const HfAc8RawEdit* edits, size_t count)
{
    uint8_t actual[4];
    size_t i;
    int failed = 0;
    for (i = count; i > 0u; --i)
    {
        const HfAc8RawEdit* edit = &edits[i - 1u];
        if (api->read(edit->address, actual, edit->size) != 0)
        {
            failed = 1;
            continue;
        }
        if (memcmp(actual, edit->before, edit->size) == 0)
            continue;
        if (memcmp(actual, edit->after, edit->size) != 0 ||
            api->write(edit->address, edit->before, edit->size) != 0 ||
            api->read(edit->address, actual, edit->size) != 0 ||
            memcmp(actual, edit->before, edit->size) != 0)
            failed = 1;
    }
    return failed ? 0 : 1;
}

static int apply_ac8_extension(const HfApi* api, HfMod* mod, char* reason, size_t capacity)
{
    uintptr_t reload;
    uintptr_t assisted;
    uintptr_t records[sizeof(HF_AC8_COMPONENTS) / sizeof(HF_AC8_COMPONENTS[0])];
    uint8_t actual[4];
    size_t i;
    static const uint8_t zero8[4] = {0u, 0u, 0u, 0u};
    static const uint8_t move1[4] = {1u, 0u, 0u, 0u};
    static const uint8_t assist_primary[4] = {0x3Eu, 0x0Au, 0u, 0u};
    static const uint8_t assist_secondary[4] = {0x3Du, 0x0Au, 0u, 0u};
    HfAc8RawEdit edits[3];
    size_t attempted = 0u;

    if (!ac8_resolve_component_records(api, mod, NULL, NULL, NULL, records, reason, capacity))
        return 0;
    reload = ac8_component_by_name(records, "WeaponReloadComponent");
    assisted = ac8_component_by_name(records, "WeaponAssistedReloadComponent");
    if (reload == 0u || assisted == 0u)
    {
        (void)snprintf(reason, capacity, "the AC-8 reload or assisted-reload component record changed after preflight");
        return 0;
    }

    memset(edits, 0, sizeof(edits));
    edits[0].address = reload + 1u;
    edits[0].size = 1u;
    memcpy(edits[0].before, zero8, 1u);
    memcpy(edits[0].after, move1, 1u);
    edits[0].label = "allow movement during reload";
    edits[1].address = assisted;
    edits[1].size = 4u;
    memcpy(edits[1].before, assist_primary, 4u);
    memcpy(edits[1].after, zero8, 4u);
    edits[1].label = "primary team-reload ability";
    edits[2].address = assisted + 4u;
    edits[2].size = 4u;
    memcpy(edits[2].before, assist_secondary, 4u);
    memcpy(edits[2].after, zero8, 4u);
    edits[2].label = "secondary team-reload ability";

    for (i = 0u; i < 3u; ++i)
    {
        if (api->read(edits[i].address, actual, edits[i].size) != 0 ||
            memcmp(actual, edits[i].before, edits[i].size) != 0)
        {
            (void)snprintf(reason, capacity, "the AC-8 %s changed after preflight", edits[i].label);
            return 0;
        }
    }
    for (i = 0u; i < 3u; ++i)
    {
        attempted = i + 1u;
        if (api->write(edits[i].address, edits[i].after, edits[i].size) != 0 ||
            api->read(edits[i].address, actual, edits[i].size) != 0 ||
            memcmp(actual, edits[i].after, edits[i].size) != 0)
        {
            if (!rollback_ac8_raw(api, edits, attempted))
            {
                (void)snprintf(reason, capacity,
                               "FATAL: AC-8 %s failed and one or more raw fields could not be restored",
                               edits[i].label);
                return -1;
            }
            (void)snprintf(reason, capacity, "HellForge could not apply the AC-8 %s", edits[i].label);
            return 0;
        }
    }
    return 1;
}

static int preflight_sai_ballistics(const HfApi* api, char* reason, size_t capacity)
{
    uintptr_t base;
    size_t image_size = 0;
    size_t i;

    if (api->module_base == NULL || api->read == NULL)
    {
        (void)snprintf(reason, capacity, "the HellForge code-read API is unavailable");
        return 0;
    }
    base = api->module_base("game.dll", &image_size);
    if (base == 0u)
    {
        (void)snprintf(reason, capacity, "game.dll is not available for the SAI ballistic guard");
        return 0;
    }
    for (i = 0; i < HF_SAI_BALLISTIC_PIN_COUNT; ++i)
    {
        const HfSaiCodePin* pin = &HF_SAI_BALLISTIC_PINS[i];
        uint8_t current[sizeof(pin->bytes)];
        if ((size_t)pin->rva > image_size || (size_t)pin->size > image_size - (size_t)pin->rva ||
            (uintptr_t)pin->rva > UINTPTR_MAX - base ||
            api->read(base + (uintptr_t)pin->rva, current, pin->size) != 0 ||
            memcmp(current, pin->bytes, pin->size) != 0)
        {
            (void)snprintf(reason, capacity,
                           "the SAI projectile-physics code pin at RVA 0x%08X is unreadable or changed",
                           (unsigned)pin->rva);
            return 0;
        }
    }
    return 1;
}

static uint32_t read_u32_le(const uint8_t* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint64_t read_u64_le(const uint8_t* bytes)
{
    return (uint64_t)read_u32_le(bytes) | ((uint64_t)read_u32_le(bytes + 4u) << 32);
}

static int ac8_record_matches(const HfAc8SettingsTable* table,
                              const HfAc8SettingsFingerprint* expected,
                              const uint8_t* actual)
{
    uint32_t i;
    int exact = 1;
    for (i = 0u; i < expected->size; ++i)
    {
        if (expected->mask_pointer != 0u && i >= 40u && i < 48u)
            continue;
        if (actual[i] != expected->before[i])
        {
            exact = 0;
            break;
        }
    }
    if (exact)
        return 1;
    if (table->allow_foliage == 0u || expected->size < 248u ||
        (expected->id != 115u && expected->id != 206u && expected->id != 284u) ||
        read_u32_le(expected->before + 244u) != 0u || read_u32_le(actual + 244u) != 2u)
        return 0;
    for (i = 0u; i < expected->size; ++i)
    {
        if ((expected->mask_pointer != 0u && i >= 40u && i < 48u) ||
            (i >= 244u && i < 248u))
            continue;
        if (actual[i] != expected->before[i])
            return 0;
    }
    return 1;
}

static int read_ac8_settings_table(const HfApi* api, uintptr_t game_base, size_t image_size,
                                   const HfAc8SettingsTable* table, uint8_t** output,
                                   char* reason, size_t capacity)
{
    uint64_t root_value = 0u;
    uint64_t rows_value = 0u;
    uintptr_t root;
    uintptr_t group;
    uintptr_t payload;
    uintptr_t rows_address;
    uintptr_t slot_address;
    uint32_t groups = 0u;
    uint32_t row_count = 0u;
    uint8_t header[24];
    uint8_t payload_bytes[16];
    const uint8_t signature[4] = {'L', 'D', 'L', 'D'};
    size_t bytes;
    uint8_t* records;
    uint32_t i;

    *output = NULL;
    if ((size_t)table->slot > image_size || sizeof(root_value) > image_size - (size_t)table->slot ||
        (uintptr_t)table->slot > UINTPTR_MAX - game_base)
    {
        (void)snprintf(reason, capacity, "the %s table slot is outside the supported game image", table->name);
        return 0;
    }
    slot_address = game_base + (uintptr_t)table->slot;
    if (api->read(slot_address, &root_value, sizeof(root_value)) != 0 ||
        root_value < UINT64_C(0x10000) || root_value >= UINT64_C(0x800000000000) ||
        root_value > UINTPTR_MAX)
    {
        (void)snprintf(reason, capacity, "the %s table pointer is unreadable or invalid", table->name);
        return 0;
    }
    root = (uintptr_t)root_value;
    if ((uintptr_t)table->group > UINTPTR_MAX - root)
    {
        (void)snprintf(reason, capacity, "the %s table layout overflows the address space", table->name);
        return 0;
    }
    if (api->read(root, &groups, sizeof(groups)) != 0 || groups != table->groups)
    {
        (void)snprintf(reason, capacity, "the %s table group count changed", table->name);
        return 0;
    }
    group = root + (uintptr_t)table->group;
    if (group > UINTPTR_MAX - 40u)
    {
        (void)snprintf(reason, capacity, "the %s table group address overflows", table->name);
        return 0;
    }
    if (api->read(group, header, sizeof(header)) != 0 || memcmp(header, signature, sizeof(signature)) != 0 ||
        read_u32_le(header + 4u) != 1u || read_u32_le(header + 8u) != table->kind ||
        read_u32_le(header + 12u) != table->size)
    {
        (void)snprintf(reason, capacity, "the %s table header changed", table->name);
        return 0;
    }
    payload = group + 24u;
    rows_address = group + 40u;
    if (api->read(payload, payload_bytes, sizeof(payload_bytes)) != 0)
    {
        (void)snprintf(reason, capacity, "the %s table index is unreadable", table->name);
        return 0;
    }
    rows_value = read_u64_le(payload_bytes);
    row_count = read_u32_le(payload_bytes + 8u);
    if (rows_value != (uint64_t)rows_address || row_count != table->count ||
        table->count == 0u || table->count > 4096u || table->stride == 0u ||
        (size_t)table->count > SIZE_MAX / (size_t)table->stride)
    {
        (void)snprintf(reason, capacity, "the %s table row pointer, count, or stride changed", table->name);
        return 0;
    }
    bytes = (size_t)table->count * (size_t)table->stride;
    if (bytes > 2u * 1024u * 1024u)
    {
        (void)snprintf(reason, capacity, "the %s table exceeds the native preflight read bound", table->name);
        return 0;
    }
    records = (uint8_t*)malloc(bytes);
    if (records == NULL || api->read(rows_address, records, bytes) != 0)
    {
        free(records);
        (void)snprintf(reason, capacity, "the %s table rows are unreadable", table->name);
        return 0;
    }

    for (i = 0u; i < table->count; ++i)
    {
        uint32_t j;
        uint32_t id = read_u32_le(records + (size_t)i * table->stride);
        for (j = 0u; j < i; ++j)
        {
            if (read_u32_le(records + (size_t)j * table->stride) == id)
            {
                free(records);
                (void)snprintf(reason, capacity, "the %s table has duplicate row id %u", table->name, id);
                return 0;
            }
        }
    }
    for (i = 0u; i < table->row_count; ++i)
    {
        const HfAc8SettingsFingerprint* expected = &table->rows[i];
        const uint8_t* actual;
        if (expected->index >= table->count || expected->size != table->stride)
        {
            free(records);
            (void)snprintf(reason, capacity, "the native %s fingerprint is malformed", table->name);
            return 0;
        }
        actual = records + (size_t)expected->index * table->stride;
        if (read_u32_le(actual) != expected->id || !ac8_record_matches(table, expected, actual))
        {
            free(records);
            (void)snprintf(reason, capacity, "the %s row %u differs from its reviewed base", table->name,
                           expected->id);
            return 0;
        }
    }
    *output = records;
    return 1;
}

static int ac8_projectile_consumers_match(const HfAc8SettingsTable* table,
                                          const uint8_t* rows, uint32_t explosion,
                                          const uint32_t* expected, size_t expected_count)
{
    uint32_t found[3];
    size_t count = 0u;
    uint32_t i;
    for (i = 0u; i < table->count; ++i)
    {
        const uint8_t* row = rows + (size_t)i * table->stride;
        uint32_t id = read_u32_le(row);
        if (read_u32_le(row + 144u) == explosion || read_u32_le(row + 156u) == explosion)
        {
            if (count >= sizeof(found) / sizeof(found[0]))
                return 0;
            found[count++] = id;
        }
    }
    if (count != expected_count)
        return 0;
    for (i = 0u; i < (uint32_t)count; ++i)
    {
        size_t j;
        for (j = i + 1u; j < count; ++j)
        {
            if (found[j] < found[i])
            {
                uint32_t swap = found[i];
                found[i] = found[j];
                found[j] = swap;
            }
        }
        if (found[i] != expected[i])
            return 0;
    }
    return 1;
}

static int ac8_explosion_consumers_match(const HfAc8SettingsTable* table,
                                         const uint8_t* rows, uint32_t damage,
                                         uint32_t expected)
{
    uint32_t found = 0u;
    uint32_t count = 0u;
    uint32_t i;
    for (i = 0u; i < table->count; ++i)
    {
        const uint8_t* row = rows + (size_t)i * table->stride;
        if (read_u32_le(row + 4u) == damage)
        {
            found = read_u32_le(row);
            ++count;
        }
    }
    return count == 1u && found == expected;
}

static int preflight_ac8_source_state(const HfApi* api, char* reason, size_t capacity)
{
    uintptr_t game_base;
    size_t image_size = 0u;
    uintptr_t component_records[sizeof(HF_AC8_COMPONENTS) / sizeof(HF_AC8_COMPONENTS[0])];
    uint8_t* projectile_rows = NULL;
    uint8_t* explosion_rows = NULL;
    uint8_t* damage_rows = NULL;
    int result = 0;
    static const uint32_t flak_projectiles[] = {206u, 284u};
    static const uint32_t aphet_projectiles[] = {115u};

    if (!ac8_resolve_component_records(api, NULL, &game_base, &image_size, NULL,
                                       component_records, reason, capacity))
        return 0;
    if (!read_ac8_settings_table(api, game_base, image_size, &HF_AC8_SETTINGS_TABLES[0],
                                 &projectile_rows, reason, capacity) ||
        !read_ac8_settings_table(api, game_base, image_size, &HF_AC8_SETTINGS_TABLES[1],
                                 &explosion_rows, reason, capacity) ||
        !read_ac8_settings_table(api, game_base, image_size, &HF_AC8_SETTINGS_TABLES[2],
                                 &damage_rows, reason, capacity))
        goto done;
    if (!ac8_projectile_consumers_match(&HF_AC8_SETTINGS_TABLES[0], projectile_rows, 380u,
                                        flak_projectiles, sizeof(flak_projectiles) / sizeof(flak_projectiles[0])) ||
        !ac8_projectile_consumers_match(&HF_AC8_SETTINGS_TABLES[0], projectile_rows, 404u,
                                        aphet_projectiles, sizeof(aphet_projectiles) / sizeof(aphet_projectiles[0])) ||
        !ac8_explosion_consumers_match(&HF_AC8_SETTINGS_TABLES[1], explosion_rows, 346u, 380u) ||
        !ac8_explosion_consumers_match(&HF_AC8_SETTINGS_TABLES[1], explosion_rows, 370u, 404u))
    {
        (void)snprintf(reason, capacity, "the AC-8 APHET/FLAK shared consumer graph changed");
        goto done;
    }
    result = 1;

done:
    free(projectile_rows);
    free(explosion_rows);
    free(damage_rows);
    return result;
}

static uint32_t epoch_damage_row_mismatch(const uint8_t* row, uint32_t id)
{
    static const uint8_t offsets[] = {0u, 4u, 8u, 12u, 16u, 20u, 24u, 28u, 32u, 36u, 44u, 48u};
    const uint32_t expected[] = {id, 1040u, 1040u, 5u, 5u, 5u, 0u, 30u, 35u, 30u, 0u, 0u};
    size_t i;
    for (i = 0u; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
        if (read_u32_le(row + offsets[i]) != expected[i])
            return offsets[i];
    return UINT32_MAX;
}

static int epoch_damage_row_supported(const uint8_t* row, uint32_t id)
{
    return epoch_damage_row_mismatch(row, id) == UINT32_MAX;
}

static int epoch_reload_record(const HfApi* api, uint8_t flag, uintptr_t* address,
                               char* reason, size_t capacity)
{
    const HfAc8ComponentFingerprint* source = &HF_EPOCH_RELOAD_SOURCE;
    uintptr_t game, owner_field, map_field, entities, row;
    uint64_t owner, map;
    size_t image_size = 0u;
    uint8_t bytes[HF_EPOCH_RELOAD_SIZE], expected[HF_EPOCH_RELOAD_SIZE];
    if (api == NULL || api->module_base == NULL || api->read == NULL || flag > 1u ||
        (game = api->module_base("game.dll", &image_size)) == 0u ||
        HF_AC8_ENTITY_MANAGER_RVA > image_size || sizeof(owner) > image_size-HF_AC8_ENTITY_MANAGER_RVA ||
        !ac8_safe_address(game, HF_AC8_ENTITY_MANAGER_RVA, sizeof(owner), &owner_field) ||
        api->read(owner_field, &owner, sizeof(owner)) != 0 ||
        !ac8_safe_address((uintptr_t)owner, HF_AC8_ENTITY_MAP_RVA, sizeof(map), &map_field) ||
        api->read(map_field, &map, sizeof(map)) != 0 || map < HF_AC8_ENTITY_MAP_BIAS ||
        !ac8_safe_address((uintptr_t)(map-HF_AC8_ENTITY_MAP_BIAS), 0u, 1u, &entities))
    {
        (void)snprintf(reason, capacity, "Epoch reload entity pointer topology is unavailable");
        return 0;
    }
    if (!hf_component_record(hf_component_api_read,(void*)api,(uintptr_t)owner,source->group_offset,
            source->header,source->map_offset,source->owner,source->size,&row)) {
        (void)snprintf(reason,capacity,"Epoch reload active table or exact owner mapping changed");
        return 0;
    }
    memcpy(expected, source->before, sizeof(expected));
    expected[1] = flag;
    if (api->read(row, bytes, sizeof(bytes)) != 0 || memcmp(bytes, expected, sizeof(bytes)) != 0)
    {
        (void)snprintf(reason, capacity, "Epoch exact reload owner row changed (expected movement flag %u)", flag);
        return 0;
    }
    *address = row;
    return 1;
}

static int epoch_restore_reload(const HfApi* api, uintptr_t address)
{
    char reason[128];
    uintptr_t current;
    const uint8_t disabled = 0u;
    if (epoch_reload_record(api, 0u, &current, reason, sizeof(reason))) return current == address;
    if (!epoch_reload_record(api, 1u, &current, reason, sizeof(reason)) || current != address ||
        api->write(address+1u, &disabled, sizeof(disabled)) != 0 ||
        !epoch_reload_record(api, 0u, &current, reason, sizeof(reason)) || current != address) return 0;
    return 1;
}

static int preflight_epoch_extension(const HfApi* api, HfMod* mod, char* reason, size_t capacity)
{
    HfPortEdit damage_link;
    HfField damage_field = HF_FIELD(ExplosionSettings, damage);
    uint8_t row[sizeof(HfDamageInfo)];
    uintptr_t address;

    if (HF_EPOCH_BURNING_DAMAGE_ID < 650u || api->damage_row == NULL ||
        api->read == NULL || api->write == NULL || api->set_live == NULL || api->get_live == NULL)
    {
        (void)snprintf(reason, capacity, "the Epoch burning damage type is unavailable");
        return 0;
    }
    address = api->damage_row(HF_EPOCH_BURNING_DAMAGE_ID);
    if (address == 0u)
    {
        (void)snprintf(reason, capacity, "the registered Epoch burning damage row is unavailable");
        return 0;
    }
    if (api->read(address, row, sizeof(row)) != 0)
    {
        (void)snprintf(reason, capacity, "the registered Epoch burning damage row could not be read");
        return 0;
    }
    {
        uint32_t mismatch = epoch_damage_row_mismatch(row, HF_EPOCH_BURNING_DAMAGE_ID);
        if (mismatch != UINT32_MAX)
        {
            (void)snprintf(reason, capacity,
                           "Epoch damage row differs at +0x%02X (found %u, expected %u)",
                           mismatch, read_u32_le(row + mismatch),
                           mismatch == 0u ? HF_EPOCH_BURNING_DAMAGE_ID :
                           (mismatch == 4u || mismatch == 8u) ? 1040u :
                           mismatch >= 12u && mismatch <= 20u ? 5u :
                           (mismatch == 28u || mismatch == 36u) ? 30u : mismatch == 32u ? 35u : 0u);
        return 0;
        }
    }

    memset(&damage_link, 0, sizeof(damage_link));
    damage_link.kind = PORT_EDIT_LIVE;
    damage_link.option = "plas-45-epoch";
    damage_link.target = UINT64_C(407);
    damage_link.field = damage_field;
    damage_link.value = (HfValue)HF_U32(HF_EPOCH_BURNING_DAMAGE_ID);
    damage_link.expected_base = (HfValue)HF_U32(61u);
    damage_link.has_expected_base = 1u;
    damage_link.flags = HF_LIVE_SHARED;
    damage_link.label = "PLAS-45 Epoch / full-charge explosion damage link";
    if (!preflight_one(api, mod, &damage_link, NULL, reason, capacity))
        return 0;
    return epoch_reload_record(api, 0u, &address, reason, capacity);
}

static int preflight_option(const HfApi* api, HfMod* mod, const char* option,
                            char* reason, size_t capacity)
{
#if HF_PORT_PLAS_1_SCORCHER_AR && HF_PORT_ALLOW_NATIVE_RUNTIME
    if (strcmp(option, "plas-1-scorcher-ar") == 0)
        return hf_sc_preflight(api, mod, &HF_SCORCHER, reason, capacity) > 0;
#endif
    if (strcmp(option, "ac-8-autocannon") == 0)
        return preflight_ac8_extension(api, reason, capacity);
    if (strcmp(option, "sai-focus-precision") == 0)
        return preflight_sai_ballistics(api, reason, capacity);
    if (strcmp(option, "plas-45-epoch") == 0)
        return preflight_epoch_extension(api, mod, reason, capacity);
    return 1;
}

static void capture_reason(const HfApi* api, HfMod* mod, char* output, size_t capacity);

static int restore_epoch_owned_row(const HfApi* api, uintptr_t address,
                                   const HfDamageInfo* before)
{
    uint8_t actual[28];
    const uint8_t* original = (const uint8_t*)before + 24u;
    if (api->write(address + 24u, original, sizeof(actual)) != 0 ||
        api->read(address + 24u, actual, sizeof(actual)) != 0 ||
        memcmp(actual, original, sizeof(actual)) != 0)
        return 0;
    return 1;
}

static int rollback_epoch_extension(const HfApi* api, HfMod* mod, uintptr_t address,
                                   const HfDamageInfo* before, int row_may_be_changed,
                                   int link_attempted, int reload_attempted, uintptr_t reload_address)
{
    HfField damage_field = HF_FIELD(ExplosionSettings, damage);
    int failed = 0;
    if (reload_attempted && !epoch_restore_reload(api, reload_address))
        failed = 1;
    if (link_attempted && api->set_live(mod, UINT64_C(407), &damage_field, NULL, HF_LIVE_SHARED) == 0)
        failed = 1;
    if (row_may_be_changed && !restore_epoch_owned_row(api, address, before))
        failed = 1;
    return failed ? 0 : 1;
}

/* Mutate only Epoch's own freshly registered DamageInfo row. The selected explosion link is a
 * shared stock record, so HellForge's live-edit API owns and can undo that part of the change. */
static int apply_epoch_extension(const HfApi* api, HfMod* mod, char* reason, size_t capacity)
{
    HfDamageInfo before;
    HfDamageInfo desired;
    HfDamageInfo actual;
    HfLiveValue linked;
    HfField damage_field = HF_FIELD(ExplosionSettings, damage);
    HfValue damage_value = HF_U32(HF_EPOCH_BURNING_DAMAGE_ID);
    uintptr_t address;
    uintptr_t reload_address, current_reload;
    const uint8_t enabled = 1u;
    int row_may_be_changed = 0;
    int link_attempted = 0;
    int reload_attempted = 0;

    if (!epoch_reload_record(api, 0u, &reload_address, reason, capacity)) return 0;

    address = api->damage_row(HF_EPOCH_BURNING_DAMAGE_ID);
    if (address == 0u || address > UINTPTR_MAX - 28u ||
        api->read(address, &before, sizeof(before)) != 0 ||
        !epoch_damage_row_supported((const uint8_t*)&before, HF_EPOCH_BURNING_DAMAGE_ID))
    {
        (void)snprintf(reason, capacity, "the Epoch burning row changed after preflight");
        return 0;
    }

    desired = before;
    desired.armor_penetration[3] = 0u;
    desired.demolition = 30u;
    desired.stagger = 35u;
    desired.push_force = 30u;
    desired.status_1_type = HF_STATUS_FIRE;
    desired.status_1_strength = 100.0f;

    row_may_be_changed = 1;
    if (api->write(address + 24u, (const uint8_t*)&desired + 24u, 28u) != 0 ||
        api->read(address, &actual, sizeof(actual)) != 0 ||
        memcmp((const uint8_t*)&actual + 24u, (const uint8_t*)&desired + 24u, 28u) != 0)
    {
        if (!rollback_epoch_extension(api, mod, address, &before, row_may_be_changed,
                                      link_attempted, reload_attempted, reload_address))
        {
            (void)snprintf(reason, capacity, "FATAL: the Epoch-owned damage row could not be restored");
            return -1;
        }
        (void)snprintf(reason, capacity, "HellForge refused or changed the Epoch-owned damage row");
        return 0;
    }

    link_attempted = 1;
    if (api->set_live(mod, UINT64_C(407), &damage_field, &damage_value, HF_LIVE_SHARED) == 0)
    {
        capture_reason(api, mod, reason, capacity);
        if (!rollback_epoch_extension(api, mod, address, &before, row_may_be_changed,
                                      link_attempted, reload_attempted, reload_address))
        {
            (void)snprintf(reason, capacity, "FATAL: the Epoch damage link or its owned row could not be restored");
            return -1;
        }
        return 0;
    }

    memset(&linked, 0, sizeof(linked));
    if (api->get_live(mod, UINT64_C(407), &damage_field, &linked) == 0 ||
        !same_value(linked.now, damage_value) || !same_value(linked.base, damage_value) ||
        linked.mod == NULL || strcmp(linked.mod, "superearth.arsenal-native-port") != 0)
    {
        (void)snprintf(reason, capacity, "the Epoch damage link did not read back as the registered type");
        if (!rollback_epoch_extension(api, mod, address, &before, row_may_be_changed,
                                      link_attempted, reload_attempted, reload_address))
        {
            (void)snprintf(reason, capacity, "FATAL: the unverified Epoch damage link could not be restored");
            return -1;
        }
        return 0;
    }

    reload_attempted = 1;
    if (!epoch_reload_record(api, 0u, &current_reload, reason, capacity) || current_reload != reload_address ||
        api->write(reload_address+1u, &enabled, sizeof(enabled)) != 0)
    {
        (void)snprintf(reason, capacity, "the Epoch guarded movement reload write failed");
        if (!rollback_epoch_extension(api, mod, address, &before, row_may_be_changed,
                                      link_attempted, reload_attempted, reload_address))
        {
            (void)snprintf(reason, capacity, "FATAL: the Epoch reload flag, damage link, or owned row could not be restored");
            return -1;
        }
        return 0;
    }

    if (!epoch_reload_record(api, 1u, &current_reload, reason, capacity) || current_reload != reload_address)
    {
        (void)snprintf(reason, capacity, "the Epoch movement reload flag did not read back as enabled");
        if (!rollback_epoch_extension(api, mod, address, &before, row_may_be_changed,
                                      link_attempted, reload_attempted, reload_address))
        {
            (void)snprintf(reason, capacity, "FATAL: the Epoch reload flag, damage link, or owned row could not be restored");
            return -1;
        }
        return 0;
    }
    return 1;
}

static int apply_one(const HfApi* api, HfMod* mod, const HfPortEdit* edit,
                     char* reason, size_t capacity)
{
    if (reason != NULL && capacity != 0u)
        reason[0] = '\0';
#if HF_PORT_PLAS_15_LOYALIST
    if (edit->kind==PORT_EDIT_LIVE && !strcmp(edit->option,"plas-15-loyalist"))return edit->field.offset==104u ? hf_loyalist_sway_apply(api,mod,reason,capacity) : hf_loyalist_magazine_apply(api,mod,reason,capacity);
#endif
    if (hf_sai_attachment_field(edit) >= 0)
        return hf_sai_attachment_change(api,mod,(unsigned)hf_sai_attachment_field(edit),0,reason,capacity);
#if HF_PORT_GAS_OVERHAUL || defined(HF_PORT_TEST_GAS)
    if (hf_gas_perk_field(edit) && HF_GAS_PERK.ready) return hf_gas_perk_change(api,0,reason,capacity);
    if (edit->kind == PORT_EDIT_GAS_PLAYER_HEALTH)
        return hf_gas_player_health_apply(api, reason, capacity);
#endif
    if (edit->kind == PORT_EDIT_LIVE)
        return api->set_live(mod, edit->target, &edit->field, &edit->value, edit->flags);
    if (edit->kind == PORT_EDIT_STAT)
        return api->set_stat(mod, &edit->stat, &edit->value, edit->flags);
    return 0;
}

static int reset_one(const HfApi* api, HfMod* mod, const HfPortEdit* edit,
                     char* reason, size_t capacity)
{
    if (reason != NULL && capacity != 0u)
        reason[0] = '\0';
    if (hf_sai_attachment_field(edit) >= 0)
        return hf_sai_attachment_change(api,mod,(unsigned)hf_sai_attachment_field(edit),1,reason,capacity);
#if HF_PORT_GAS_OVERHAUL || defined(HF_PORT_TEST_GAS)
    if (hf_gas_perk_field(edit) && HF_GAS_PERK.ready) return hf_gas_perk_change(api,1,reason,capacity);
    if (edit->kind == PORT_EDIT_GAS_PLAYER_HEALTH)
        return hf_gas_player_health_reset(api, reason, capacity);
#endif
    if (edit->kind == PORT_EDIT_LIVE)
        return api->set_live(mod, edit->target, &edit->field, NULL, edit->flags);
    if (edit->kind == PORT_EDIT_STAT)
        return api->set_stat(mod, &edit->stat, NULL, edit->flags);
    return 0;
}

static void capture_reason(const HfApi* api, HfMod* mod, char* output, size_t capacity)
{
    const char* reason = api->last_log ? api->last_log(mod) : NULL;
    if (reason == NULL || reason[0] == '\0')
        reason = "HellForge refused the edit without a detail message";
    (void)snprintf(output, capacity, "%s", reason);
}

#if HF_PORT_VG_70_VARIABLE && HF_PORT_ALLOW_NATIVE_RUNTIME
typedef struct HfVg70Runner
{
    const HfApi* api;
    HfMod* mod;
    HfVg70MemoryOps memory;
    HfVg70NativePlan plan;
    HfVg70Instance* instances;
    HfVg70ModeMemory memories[HF_VG70_MAX_INSTANCES];
    uintptr_t game;
    uint64_t next_attempt_ms;
    uint64_t settle_until_ms;
    size_t previous_instances;
    int registered;
    int modules_checked;
    int initialized;
    int boot_sweep_done;
    int done;
    unsigned module_attempts;
    char last_message[512];
} HfVg70Runner;

static HfVg70Runner HF_VG70_RUNNER;

static int vg70_api_read(void* context, uintptr_t address, void* output, size_t size)
{
    HfVg70Runner* runner = (HfVg70Runner*)context;
    return runner->api->read == NULL ? 1 : runner->api->read(address, output, size);
}

static int vg70_api_write(void* context, uintptr_t address, const void* input, size_t size)
{
    HfVg70Runner* runner = (HfVg70Runner*)context;
    return runner->api->write == NULL ? 1 : runner->api->write(address, input, size);
}

static int vg70_api_data_access(void* context, uintptr_t address, size_t size)
{
    (void)context;
    return sickle_native_data_access(NULL, address, size, 0);
}

static void vg70_log_once(HfVg70Runner* runner, const char* message)
{
    if (runner == NULL || message == NULL || strcmp(runner->last_message, message) == 0)
        return;
    (void)snprintf(runner->last_message, sizeof(runner->last_message), "%s", message);
    runner->api->log(runner->mod, "%s", message);
}

static int vg70_verify_modules(HfVg70Runner* runner, char* reason, size_t capacity)
{
    static const char* const accepted_executables[] = {
        "F5FEE03DCFDB2E553A4752C83E290950AC13316B376D8196AA556FF0400D5F06",
        "F5FEE03DCFDB2E553A4752C283590950AC13316B376D8196AA556FF0400D5F06"
    };
    static const char accepted_game[] =
        "2E2C3B7C2500646DADD5F2B4C6E0504DBB7E7896139F64CDDC0D1813C718F51E";
    HfSickleCrypto crypto;
    uintptr_t executable, game;
    size_t executable_size = 0u, game_size = 0u;
    char executable_hash[65], game_hash[65];
    size_t i;
    int executable_matches = 0;
    int result = 0;

    if (runner == NULL || runner->api->module_base == NULL || runner->api->read == NULL ||
        runner->api->write == NULL)
    {
        (void)snprintf(reason, capacity, "HellForge module-memory APIs are unavailable");
        return -1;
    }
    executable = runner->api->module_base("helldivers2.exe", &executable_size);
    game = runner->api->module_base("game.dll", &game_size);
    if (executable == 0u || executable_size == 0u || game == 0u || game_size == 0u)
    {
        (void)snprintf(reason, capacity, "Helldivers 2 executable or game.dll is unavailable");
        return 0;
    }
    memset(&crypto, 0, sizeof(crypto));
    if (!sickle_hash_module_file(&crypto, executable, executable_hash) ||
        !sickle_hash_module_file(&crypto, game, game_hash))
    {
        if (crypto.library != NULL)
            (void)FreeLibrary(crypto.library);
        (void)snprintf(reason, capacity, "Could not verify the VG-70 executable and game.dll hashes");
        return 0;
    }
    if (crypto.library != NULL)
        (void)FreeLibrary(crypto.library);
    for (i = 0u; i < sizeof(accepted_executables) / sizeof(accepted_executables[0]); ++i)
        if (strcmp(executable_hash, accepted_executables[i]) == 0)
            executable_matches = 1;
    if (!executable_matches || strcmp(game_hash, accepted_game) != 0)
    {
        (void)snprintf(reason, capacity, "Unsupported executable or game.dll hash; no VG-70 edits were applied");
        return -1;
    }
    runner->game = game;
    result = 1;
    return result;
}

static void vg70_native_tick(HfMod* mod, float dt, void* user)
{
    HfVg70Runner* runner = (HfVg70Runner*)user;
    uint64_t now = GetTickCount64();
    char reason[512];
    int module_result;
    (void)mod;
    (void)dt;
    if (runner == NULL || runner->done || now < runner->next_attempt_ms)
        return;
    reason[0] = '\0';

    if (!runner->modules_checked)
    {
        module_result = vg70_verify_modules(runner, reason, sizeof(reason));
        if (module_result < 0)
        {
            runner->done = 1;
            vg70_log_once(runner, reason);
            return;
        }
        if (module_result == 0)
        {
            ++runner->module_attempts;
            if (runner->module_attempts >= 8u)
            {
                char detail[sizeof(reason)];
                runner->done = 1;
                (void)snprintf(detail, sizeof(detail), "%s", reason);
                (void)snprintf(reason, sizeof(reason),
                               "Refused VG-70: module/hash readiness exceeded its bounded retry window: %s", detail);
                vg70_log_once(runner, reason);
            }
            else
            {
                char detail[sizeof(reason)];
                runner->next_attempt_ms = now + 1000u;
                (void)snprintf(detail, sizeof(detail), "%s", reason);
                (void)snprintf(reason, sizeof(reason), "Waiting for VG-70 module readiness (%u/8): %s",
                               runner->module_attempts, detail);
                vg70_log_once(runner, reason);
            }
            return;
        }
        runner->modules_checked = 1;
        runner->settle_until_ms = now + 30000u;
        vg70_log_once(runner, "VG-70 module hashes match; waiting 30 seconds for game and companion data to settle");
        return;
    }
    if (!runner->initialized)
    {
        uint32_t changed = 0u;
        HfVg70ApplyResult applied;
        if (now < runner->settle_until_ms)
            return;
        applied = hf_vg70_apply_all_profiles(&runner->memory, runner->game, &runner->plan,
                                             &changed, reason, sizeof(reason));
        if (applied != HF_VG70_APPLIED)
        {
            char detail[sizeof(reason)];
            runner->done = 1;
            (void)snprintf(detail, sizeof(detail), "%s", reason);
            if (applied == HF_VG70_FATAL)
                (void)snprintf(reason, sizeof(reason), "FATAL: VG-70 global profile transaction: %s", detail);
            else
                (void)snprintf(reason, sizeof(reason), "Refused VG-70 global profiles: %s", detail);
            vg70_log_once(runner, reason);
            return;
        }
        runner->instances = (HfVg70Instance*)calloc(HF_VG70_MAX_INSTANCES, sizeof(*runner->instances));
        if (runner->instances == NULL)
        {
            runner->done = 1;
            vg70_log_once(runner, "Refused VG-70: bounded instance snapshot allocation failed");
            return;
        }
        runner->initialized = 1;
        (void)snprintf(reason, sizeof(reason),
                       "VG-70 shared settings applied (%u fields); configuring player-owned instances",
                       changed);
        vg70_log_once(runner, reason);
    }

    {
        size_t count = 0u;
        uint32_t new_rows = 0u, changed = 0u;
        HfVg70ApplyResult result;
        if (!hf_vg70_native_discover(&runner->memory, &runner->plan, runner->instances,
                                     HF_VG70_MAX_INSTANCES, &count, reason, sizeof(reason)))
        {
            char detail[sizeof(reason)];
            (void)snprintf(detail, sizeof(detail), "%s", reason);
            (void)snprintf(reason, sizeof(reason), "VG-70 instance discovery refused: %s", detail);
            vg70_log_once(runner, reason);
            return;
        }
        result = hf_vg70_native_isolate(&runner->memory, &runner->plan, runner->instances,
                                        count, &new_rows, reason, sizeof(reason));
        if (result != HF_VG70_APPLIED)
        {
            char detail[sizeof(reason)];
            if (result == HF_VG70_FATAL)
                runner->done = 1;
            (void)snprintf(detail, sizeof(detail), "%s", reason);
            (void)snprintf(reason, sizeof(reason), "%sVG-70 cache isolation: %s",
                           result == HF_VG70_FATAL ? "FATAL: " : "Refused ", detail);
            vg70_log_once(runner, reason);
            return;
        }
        result = hf_vg70_native_configure(&runner->memory, &runner->plan, runner->instances,
                                          count, runner->memories, HF_VG70_MAX_INSTANCES,
                                          runner->boot_sweep_done, &changed, reason, sizeof(reason));
        if (result != HF_VG70_APPLIED)
        {
            char detail[sizeof(reason)];
            if (result == HF_VG70_FATAL)
                runner->done = 1;
            (void)snprintf(detail, sizeof(detail), "%s", reason);
            (void)snprintf(reason, sizeof(reason), "%sVG-70 instance update: %s",
                           result == HF_VG70_FATAL ? "FATAL: " : "Refused ", detail);
            vg70_log_once(runner, reason);
            return;
        }
        runner->boot_sweep_done = 1;
        if (count != runner->previous_instances || new_rows != 0u || changed != 0u)
        {
            (void)snprintf(reason, sizeof(reason),
                           "VG-70 profiles active for %u player-owned weapon instances (%u cache rows, %u fields updated)",
                           (unsigned)count, new_rows, changed);
            vg70_log_once(runner, reason);
        }
        runner->previous_instances = count;
        runner->last_message[0] = '\0';
    }
}

static void vg70_begin_native(const HfApi* api, HfMod* mod)
{
    HfVg70Runner* runner = &HF_VG70_RUNNER;
    if (runner->registered || runner->done)
        return;
    runner->api = api;
    runner->mod = mod;
    runner->memory.context = runner;
    runner->memory.read = vg70_api_read;
    runner->memory.write = vg70_api_write;
    runner->memory.data_access = vg70_api_data_access;
    if (api->size < (uint32_t)(offsetof(HfApi, on_game_thread) + sizeof(api->on_game_thread)) ||
        api->on_game_thread == NULL ||
        api->on_game_thread(mod, vg70_native_tick, runner) == 0)
    {
        runner->done = 1;
        api->log(mod, "Refused VG-70: HellForge game-thread scheduling is unavailable");
        return;
    }
    runner->registered = 1;
    api->log(mod, "VG-70 native setup queued; it will verify module hashes, wait for data readiness and configure instances on the game thread");
}
#endif

static void ApplyReadyFiltered(HfMod* mod, HfEvent event, void* user, const char* only_option)
{
    const HfApi* api = (const HfApi*)user;
    size_t first = 0;
    (void)event;

    while (first < HF_PORT_EDIT_COUNT)
    {
        const char* option = HF_PORT_EDITS[first].option;
        size_t end = first + 1;
        size_t applied = 0;
        size_t i;
        char reason[256];

        while (end < HF_PORT_EDIT_COUNT && strcmp(HF_PORT_EDITS[end].option, option) == 0)
            ++end;

        if (only_option != NULL && strcmp(option, only_option) != 0)
        {
            first = end;
            continue;
        }

        if (!preflight_option(api, mod, option, reason, sizeof(reason)))
        {
            hf_stats_retry_remove(option);
            api->log(mod, "Refused %s before mutation: %s", option, reason);
            first = end;
            continue;
        }

        {
            int stats_not_ready = 0;
        for (i = first; i < end; ++i)
        {
            if (!preflight_one(api, mod, &HF_PORT_EDITS[i], &stats_not_ready, reason, sizeof(reason)))
            {
                if (stats_not_ready)
                    hf_stats_retry_add(option);
                else
                {
                    hf_stats_retry_remove(option);
                    api->log(mod, "Refused %s before mutation at %s: %s", option,
                             HF_PORT_EDITS[i].label, reason);
                }
                break;
            }
        }
        }
        if (i < end)
        {
            first = end;
            continue;
        }

        for (i = first; i < end; ++i)
        {
            int result = apply_one(api, mod, &HF_PORT_EDITS[i], reason, sizeof(reason));
            if (result <= 0)
            {
                size_t rollback;
                int rollback_failed = result < 0;
                const char* failed_field = HF_PORT_EDITS[i].label;
                if (reason[0] == '\0')
                    capture_reason(api, mod, reason, sizeof(reason));

                for (rollback = applied; rollback > 0; --rollback)
                {
                    const HfPortEdit* undo = &HF_PORT_EDITS[first + rollback - 1];
                    char reset_reason[256] = {0};
                    if (reset_one(api, mod, undo, reset_reason, sizeof(reset_reason)) == 0)
                    {
                        if (reset_reason[0] == '\0')
                            capture_reason(api, mod, reset_reason, sizeof(reset_reason));
                        api->log(mod, "FATAL: %s could not roll back %s: %s", option, undo->label, reset_reason);
                        rollback_failed = 1;
                    }
                }

                if (rollback_failed)
                {
                    hf_stats_retry_remove(option);
                    api->log(mod, "FATAL: stopping native pack edits after an incomplete rollback in %s: %s",
                             option, reason);
                    return;
                }
                hf_stats_retry_remove(option);
                api->log(mod, "Refused %s at %s; its earlier edits were rolled back: %s", option, failed_field, reason);
                break;
            }
            ++applied;
        }

        if (i == end && strcmp(option, "sai-focus-precision") == 0)
        {
            HfSaiAttachmentOps attachment_ops=hf_sai_attachment_ops(api,mod);
            if (HF_SAI_ATTACHMENT_STATE.attempted != 7u ||
                !hf_sai_attachment_proof(&attachment_ops,&HF_SAI_ATTACHMENT_STATE))
            {
                size_t rollback;
                int rollback_failed=0;
                for(rollback=applied;rollback>0u;--rollback)
                {
                    const HfPortEdit* undo=&HF_PORT_EDITS[first+rollback-1u];
                    char reset_reason[256]={0};
                    if(reset_one(api,mod,undo,reset_reason,sizeof(reset_reason))==0)
                    {
                        api->log(mod,"FATAL: %s could not roll back %s: %s",option,undo->label,reset_reason);
                        rollback_failed=1;
                    }
                }
                hf_stats_retry_remove(option);
                if(rollback_failed)
                {
                    api->log(mod,"FATAL: stopping native pack edits after incomplete SAI attachment rollback");
                    return;
                }
                api->log(mod,"Refused %s after final attachment-table verification; its edits were rolled back",option);
                first=end;
                continue;
            }
        }

#if HF_PORT_PLAS_1_SCORCHER_AR && HF_PORT_ALLOW_NATIVE_RUNTIME
        if (i == end && strcmp(option, "plas-1-scorcher-ar") == 0)
        {
            int extension_result = hf_sc_apply(api, mod, &HF_SCORCHER, reason, sizeof(reason));
            if (extension_result < 1)
            {
                size_t rollback;
                int rollback_failed = extension_result < 0;
                for (rollback = applied; rollback > 0; --rollback)
                {
                    const HfPortEdit* undo = &HF_PORT_EDITS[first + rollback - 1];
                    char reset_reason[256] = {0};
                    if (reset_one(api, mod, undo, reset_reason, sizeof(reset_reason)) == 0)
                    {
                        if (reset_reason[0] == '\0')
                            capture_reason(api, mod, reset_reason, sizeof(reset_reason));
                        api->log(mod, "FATAL: %s could not roll back %s: %s", option, undo->label, reset_reason);
                        rollback_failed = 1;
                    }
                }
                if (rollback_failed)
                {
                    hf_stats_retry_remove(option);
                    api->log(mod, "FATAL: stopping native pack edits after an incomplete rollback in %s: %s",
                             option, reason);
                    return;
                }
                hf_stats_retry_remove(option);
                api->log(mod, "Refused %s after its scalar edits; those edits were rolled back: %s", option, reason);
                first = end;
                continue;
            }
        }

#endif

        if (i == end && strcmp(option, "plas-45-epoch") == 0)
        {
            int extension_result = apply_epoch_extension(api, mod, reason, sizeof(reason));
            if (extension_result != 1)
            {
                size_t rollback;
                int rollback_failed = extension_result < 0;
                for (rollback = applied; rollback > 0; --rollback)
                {
                    const HfPortEdit* undo = &HF_PORT_EDITS[first + rollback - 1];
                    char reset_reason[256] = {0};
                    if (reset_one(api, mod, undo, reset_reason, sizeof(reset_reason)) == 0)
                    {
                        if (reset_reason[0] == '\0')
                            capture_reason(api, mod, reset_reason, sizeof(reset_reason));
                        api->log(mod, "FATAL: %s could not roll back %s: %s", option, undo->label, reset_reason);
                        rollback_failed = 1;
                    }
                }
                if (rollback_failed)
                {
                    hf_stats_retry_remove(option);
                    api->log(mod, "FATAL: stopping native pack edits after an incomplete rollback in %s: %s",
                             option, reason);
                    return;
                }
                hf_stats_retry_remove(option);
                api->log(mod, "Refused %s after its scalar edits; those edits were rolled back: %s", option, reason);
                first = end;
                continue;
            }
        }

        if (i == end && strcmp(option, "ac-8-autocannon") == 0)
        {
            int extension_result = apply_ac8_extension(api, mod, reason, sizeof(reason));
            if (extension_result != 1)
            {
                size_t rollback;
                int rollback_failed = extension_result < 0;
                for (rollback = applied; rollback > 0; --rollback)
                {
                    const HfPortEdit* undo = &HF_PORT_EDITS[first + rollback - 1];
                    char reset_reason[256] = {0};
                    if (reset_one(api, mod, undo, reset_reason, sizeof(reset_reason)) == 0)
                    {
                        if (reset_reason[0] == '\0')
                            capture_reason(api, mod, reset_reason, sizeof(reset_reason));
                        api->log(mod, "FATAL: %s could not roll back %s: %s", option, undo->label, reset_reason);
                        rollback_failed = 1;
                    }
                }
                if (rollback_failed)
                {
                    hf_stats_retry_remove(option);
                    api->log(mod, "FATAL: stopping native pack edits after an incomplete rollback in %s: %s",
                             option, reason);
                    return;
                }
                hf_stats_retry_remove(option);
                api->log(mod, "Refused %s after its scalar edits; those edits were rolled back: %s", option, reason);
                first = end;
                continue;
            }
        }

        if (i == end)
        {
            hf_stats_retry_remove(option);
            if (strcmp(option, "plas-45-epoch") == 0)
                api->log(mod, "Applied %s (%u native field edits plus its owned burning damage type)",
                         option, (unsigned)(end - first));
#if HF_PORT_PLAS_1_SCORCHER_AR && HF_PORT_ALLOW_NATIVE_RUNTIME
            else if (strcmp(option, "plas-1-scorcher-ar") == 0)
                api->log(mod, "Applied Scorcher AR: private impact damage %u at 75/38; Loyalist shared damage56 preserved at100/50", HF_SCORCHER.damage);
#endif
            else if (strcmp(option, "ac-8-autocannon") == 0)
                api->log(mod, "Applied %s (%u native field edits plus mobile reload and team-reload changes)",
                         option, (unsigned)(end - first));
            else
                api->log(mod, "Applied %s (%u native field edits)", option, (unsigned)(end - first));
        }
#if HF_PORT_GAS_OVERHAUL
        if (i == end && strcmp(option, "gas-overhaul") == 0)
            hf_gas_begin_native(api, mod);
#endif
        first = end;
    }

    if (only_option != NULL)
        return;

#if HF_PORT_ADJUDICATOR_HEAVY
    {
        char reason[256];
        int result = hf_private_apply_profile(api, mod, "adjudicator-heavy", reason, sizeof(reason));
        if (result == 1 || result == 2)
            api->log(mod, "%s", reason);
        else
            api->log(mod, "Refused adjudicator-heavy: %s", reason);
        if (result < 0)
            return;
    }
#endif
#if HF_PORT_GL52_DE_ESCALATOR
    {
        char reason[256];
        int result = hf_private_apply_profile(api, mod, "gl52-de-escalator", reason, sizeof(reason));
        if (result == 1 || result == 2)
            api->log(mod, "%s", reason);
        else
            api->log(mod, "Refused gl52-de-escalator: %s", reason);
        if (result < 0)
            return;
    }
#endif
#if HF_PORT_TENDERIZER_ARMOR_SOFTENER
    {
        char reason[256];
        int result = hf_private_apply_profile(api, mod, "tenderizer-armor-softener", reason, sizeof(reason));
        if (result == 1 || result == 2)
            api->log(mod, "%s", reason);
        else
            api->log(mod, "Refused tenderizer-armor-softener: %s", reason);
        if (result < 0)
            return;
    }
#endif

#if HF_PORT_RS_422_RAILGUN && HF_PORT_ALLOW_NATIVE_RUNTIME
    hf_rg_curve_request(api,mod);
#endif
#if HF_PORT_SICKLE_LASER_AR && HF_PORT_ALLOW_NATIVE_RUNTIME
    sickle_begin_native(api, mod);
#endif
#if HF_PORT_VG_70_VARIABLE && HF_PORT_ALLOW_NATIVE_RUNTIME
    vg70_begin_native(api, mod);
#endif
#if HF_PORT_ARC_3_RAPID_SUPERCHARGE && HF_PORT_ALLOW_NATIVE_RUNTIME
    hf_arc3_begin_native(api, mod);
#endif
#if HF_PORT_PLAS_39_ACCELERATOR_RIFLE && HF_PORT_ALLOW_NATIVE_RUNTIME
    hf_plas39_begin_native(api, mod);
#endif
#if HF_PORT_PUNISHER_PLASMA && HF_PORT_ALLOW_NATIVE_RUNTIME
    hf_punisher_apply_ready(mod, HF_EVENT_PROJECTILES_READY, (void*)api);
#endif
    hf_stats_retry_start(mod, api);
}

static void ApplyReady(HfMod* mod, HfEvent event, void* user)
{
    ApplyReadyFiltered(mod, event, user, NULL);
}

static void ApplyWhenReady(HfMod* mod, HfEvent event, void* user)
{
    switch (event)
    {
        case HF_EVENT_NETWORK_READY:
            HF_NATIVE_READY_EVENTS |= HF_NATIVE_READY_NETWORK;
            break;
        case HF_EVENT_ENTITIES_READY:
            HF_NATIVE_READY_EVENTS |= HF_NATIVE_READY_ENTITIES;
            break;
        case HF_EVENT_PROJECTILES_READY:
            HF_NATIVE_READY_EVENTS |= HF_NATIVE_READY_PROJECTILES;
            break;
        default:
            return;
    }

    if (HF_NATIVE_READY_EVENTS != HF_NATIVE_READY_ALL)
        return;

    /* Entity data can reload after startup. Reapply on that event once every table is available. */
    if (!HF_NATIVE_READY_CALLBACK_APPLIED || event == HF_EVENT_ENTITIES_READY)
    {
        HF_NATIVE_READY_CALLBACK_APPLIED = 1;
        ApplyReady(mod, event, user);
    }
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api, HfMod* mod)
{
    const uint32_t required_size = (uint32_t)(offsetof(HfApi, set_stat) + sizeof(api->set_stat));
    const char* build;

    if (api == NULL || mod == NULL || api->version < 11u || api->size < required_size ||
        api->on == NULL || api->build_id == NULL || api->log == NULL || api->set_live == NULL ||
        api->set_stat == NULL || api->get_live == NULL)
        return 1;

    build = api->build_id();
    if (build == NULL || strcmp(build, "25480438") != 0)
    {
        api->log(mod, "Refusing unsupported game build; this port was resolved for 25480438 (got %s)",
                 build != NULL ? build : "unknown");
        return 1;
    }

    if (HF_PORT_EDIT_COUNT == 0u && !HF_PORT_SICKLE_LASER_AR && !HF_PORT_VG_70_VARIABLE &&
        !HF_PORT_ARC_3_RAPID_SUPERCHARGE && !HF_PORT_PLAS_39_ACCELERATOR_RIFLE &&
        !HF_PORT_PUNISHER_PLASMA && !HF_PORT_ADJUDICATOR_HEAVY &&
        !HF_PORT_GL52_DE_ESCALATOR && !HF_PORT_TENDERIZER_ARMOR_SOFTENER && !HF_PORT_RS_422_RAILGUN)
    {
        api->log(mod, "Configuration loaded: no native gameplay options are enabled; Flag preset is %u/%u",
                 (unsigned)HF_PORT_FLAG_NORMAL_DAMAGE, (unsigned)HF_PORT_FLAG_DURABLE_DAMAGE);
        return 0;
    }

#if HF_PORT_PLAS_45_EPOCH
    if (api->add_damage != NULL)
    {
        HfDamage burning;
        memset(&burning, 0, sizeof(burning));
        burning.size = (uint32_t)sizeof(burning);
        burning.base = 61u;
        burning.name = "superearth.arsenal-native-port/plas-45-epoch/full-charge-burning";
        burning.scale = 1.3f;
        HF_EPOCH_BURNING_DAMAGE_ID = api->add_damage(mod, &burning);
        if (HF_EPOCH_BURNING_DAMAGE_ID >= 650u)
            HF_REGISTRY_DAMAGE_WITNESS_ID = HF_EPOCH_BURNING_DAMAGE_ID;
    }
    if (HF_EPOCH_BURNING_DAMAGE_ID < 650u)
        api->log(mod, "PLAS-45 Epoch burning damage type is unavailable; only that option will be refused");
#endif

#if HF_PORT_PUNISHER_PLASMA && HF_PORT_ALLOW_NATIVE_RUNTIME
    if (!hf_punisher_register_content(api, mod, &HF_PUNISHER_CONTENT))
        api->log(mod, "Punisher native content registration failed; the remaining options will continue");
    else
    {
        if (HF_REGISTRY_DAMAGE_WITNESS_ID == 0u)
            HF_REGISTRY_DAMAGE_WITNESS_ID = HF_PUNISHER_CONTENT.normal_damage;
        HF_REGISTRY_PROJECTILE_WITNESS_ID = HF_PUNISHER_CONTENT.normal_projectile;
        hf_punisher_package_begin(api, mod);
    }
#endif

#if HF_PORT_PLAS_1_SCORCHER_AR && HF_PORT_ALLOW_NATIVE_RUNTIME
    if (!hf_sc_register(api, mod, &HF_SCORCHER))
        api->log(mod, "Scorcher private impact registration unavailable; only that option will be refused");
    else if (HF_REGISTRY_DAMAGE_WITNESS_ID == 0u)
        HF_REGISTRY_DAMAGE_WITNESS_ID = HF_SCORCHER.damage;
#endif

#if HF_PORT_RS_422_RAILGUN && HF_PORT_ALLOW_NATIVE_RUNTIME
    if(!hf_rg_register(api,mod,&HF_RAILGUN))
        api->log(mod,"Railgun private content unavailable; only that option will be refused");
    else {
        if(!HF_REGISTRY_DAMAGE_WITNESS_ID)HF_REGISTRY_DAMAGE_WITNESS_ID=HF_RAILGUN.damage;
        if(!HF_REGISTRY_PROJECTILE_WITNESS_ID)HF_REGISTRY_PROJECTILE_WITNESS_ID=HF_RAILGUN.projectile;
        hf_rg_curve_begin(api,mod);
    }
#endif
#if HF_PORT_SICKLE_LASER_AR && HF_PORT_ALLOW_NATIVE_RUNTIME
    sickle_heatsinks_begin(api, mod);
#endif

    if (api->on(mod, HF_EVENT_NETWORK_READY, ApplyWhenReady, (void*)api) != 0 ||
        api->on(mod, HF_EVENT_ENTITIES_READY, ApplyWhenReady, (void*)api) != 0 ||
        api->on(mod, HF_EVENT_PROJECTILES_READY, ApplyWhenReady, (void*)api) != 0)
    {
        api->log(mod, "Could not register all native data-ready callbacks: %s",
                 api->last_log ? api->last_log(mod) : "HellForge event registration failed");
        return 1;
    }
    api->log(mod, "Queued %u catalogue-resolved field edits for game build %s; waiting for network, entity and projectile data",
             (unsigned)HF_PORT_EDIT_COUNT, build);
    return 0;
}
