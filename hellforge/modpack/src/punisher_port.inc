/* Source-pinned Punisher Plasma / One-Two component profile.
 * The game supplies all source and donor bytes; this module stores only offsets and hashes. */
#include "punisher_source_guard.generated.h"

#define HF_PUNISHER_PARENT_RESOURCE UINT64_C(0x05D8D8C073B9D502)
#define HF_PUNISHER_CHILD_RESOURCE UINT64_C(0x02CD7321CD8445F5)
#define HF_PUNISHER_PRIVATE_CHILD_NAME "superearth/arsenal_native/punisher_plasma/shotgun_child"
#define HF_PUNISHER_RELOAD_DONOR_RESOURCE UINT64_C(0xDCD1C835407EF7BA)
#define HF_PUNISHER_WEAPON_SIZE 1232u
#define HF_PUNISHER_PROJECTILE_SIZE 616u
#define HF_PUNISHER_MAGAZINE_SIZE 160u
#define HF_PUNISHER_CUSTOMIZATION_SIZE 4872u
#define HF_PUNISHER_ROUNDS_SIZE 136u
#define HF_PUNISHER_RELOAD_SIZE 80u
#define HF_PUNISHER_ROW_COUNT 8u
#define HF_PUNISHER_MAX_ROW_SIZE HF_PUNISHER_CUSTOMIZATION_SIZE
#define HF_PUNISHER_MAX_PATCHES 512u
#define HF_PUNISHER_MOD_ID "superearth.arsenal-native-port"
#define HF_PUNISHER_FIRST_PROJECTILE_ID 351u
#define HF_PUNISHER_LAST_PROJECTILE_ID 414u
#define HF_PUNISHER_EXPLOSION_SIZE 152u
#define HF_PUNISHER_EXPLOSION_ARRAY_MAX 512u
#define HF_PUNISHER_PROJECTILE_REGISTRY_OFFSET UINT64_C(0x37C7670)
#define HF_PUNISHER_DAMAGE_REGISTRY_OFFSET UINT64_C(0x37C60C0)
#define HF_PUNISHER_NORMAL_EXPLOSION_ID 290u
#define HF_PUNISHER_PELLET_EXPLOSION_ID 355u

typedef enum HfPunisherRowId
{
    HF_PUNISHER_PARENT_WEAPON = 0,
    HF_PUNISHER_PARENT_PROJECTILE,
    HF_PUNISHER_PARENT_MAGAZINE,
    HF_PUNISHER_PARENT_CUSTOMIZATION,
    HF_PUNISHER_CHILD_WEAPON,
    HF_PUNISHER_CHILD_PROJECTILE,
    HF_PUNISHER_CHILD_ROUNDS,
    HF_PUNISHER_CHILD_RELOAD
} HfPunisherRowId;

typedef struct HfPunisherSourceSpec
{
    const char* component;
    uint64_t resource;
    uint32_t stride;
    const char* base_sha256;
    const char* profile_sha256;
    const char* label;
} HfPunisherSourceSpec;

typedef struct HfPunisherRecord
{
    uintptr_t address;
    uint32_t stride;
    uint8_t before[HF_PUNISHER_MAX_ROW_SIZE];
    uint8_t after[HF_PUNISHER_MAX_ROW_SIZE];
} HfPunisherRecord;

typedef struct HfPunisherPatch
{
    uintptr_t address;
    uint32_t size;
    uint8_t before[8];
    uint8_t after[8];
} HfPunisherPatch;

typedef struct HfPunisherRunner
{
    uint32_t applied;
    uint32_t poisoned;
} HfPunisherRunner;

typedef struct HfPunisherLiveEdit
{
    HfRef target;
    HfField field;
    HfValue expected_base;
    HfValue value;
    uint32_t stock_row;
    uint32_t flags;
} HfPunisherLiveEdit;

typedef struct HfPunisherContent
{
    uint64_t child_entity;
    uint32_t normal_projectile;
    uint32_t pellet_projectile;
    uint32_t normal_damage;
    uint32_t pellet_damage;
    uint32_t available;
    uint32_t configured;
    uint32_t poisoned;
    uint32_t applied_count;
    HfPunisherLiveEdit applied[24];
    uintptr_t game_base;
} HfPunisherContent;

static const HfPunisherSourceSpec HF_PUNISHER_SOURCE[HF_PUNISHER_ROW_COUNT] = {
    {"WeaponDataComponentData", HF_PUNISHER_PARENT_RESOURCE, HF_PUNISHER_WEAPON_SIZE,
     HF_PUNISHER_PARENT_WEAPON_BASE_SHA256, HF_PUNISHER_PARENT_WEAPON_PROFILE_SHA256, "parent weapon"},
    {"ProjectileWeaponComponentData", HF_PUNISHER_PARENT_RESOURCE, HF_PUNISHER_PROJECTILE_SIZE,
     HF_PUNISHER_PARENT_PROJECTILE_BASE_SHA256, HF_PUNISHER_PARENT_PROJECTILE_PROFILE_SHA256, "parent projectile"},
    {"WeaponMagazineComponentData", HF_PUNISHER_PARENT_RESOURCE, HF_PUNISHER_MAGAZINE_SIZE,
     HF_PUNISHER_PARENT_MAGAZINE_BASE_SHA256, HF_PUNISHER_PARENT_MAGAZINE_PROFILE_SHA256, "parent magazine"},
    {"WeaponCustomizationComponentData", HF_PUNISHER_PARENT_RESOURCE, HF_PUNISHER_CUSTOMIZATION_SIZE,
     HF_PUNISHER_PARENT_CUSTOMIZATION_BASE_SHA256, HF_PUNISHER_PARENT_CUSTOMIZATION_PROFILE_SHA256, "parent customization"},
    {"WeaponDataComponentData", HF_PUNISHER_CHILD_RESOURCE, HF_PUNISHER_WEAPON_SIZE,
     HF_PUNISHER_CHILD_WEAPON_BASE_SHA256, HF_PUNISHER_CHILD_WEAPON_PROFILE_SHA256, "child weapon"},
    {"ProjectileWeaponComponentData", HF_PUNISHER_CHILD_RESOURCE, HF_PUNISHER_PROJECTILE_SIZE,
     HF_PUNISHER_CHILD_PROJECTILE_BASE_SHA256, HF_PUNISHER_CHILD_PROJECTILE_PROFILE_SHA256, "child projectile"},
    {"WeaponRoundsComponentData", HF_PUNISHER_CHILD_RESOURCE, HF_PUNISHER_ROUNDS_SIZE,
     HF_PUNISHER_CHILD_ROUNDS_BASE_SHA256, HF_PUNISHER_CHILD_ROUNDS_PROFILE_SHA256, "child rounds"},
    {"WeaponReloadComponentData", HF_PUNISHER_CHILD_RESOURCE, HF_PUNISHER_RELOAD_SIZE,
     HF_PUNISHER_CHILD_RELOAD_BASE_SHA256, HF_PUNISHER_CHILD_RELOAD_PROFILE_SHA256, "child reload"}
};

#if defined(HF_PORT_TEST_PUNISHER)
extern int hf_punisher_test_hash(const void* bytes, size_t size, char output[65]);
static int hf_punisher_hash(const void* bytes, size_t size, char output[65])
{
    return hf_punisher_test_hash(bytes, size, output);
}
#else
static HfSickleCrypto HF_PUNISHER_CRYPTO;
static int hf_punisher_hash(const void* bytes, size_t size, char output[65])
{
    if (!sickle_crypto_init(&HF_PUNISHER_CRYPTO))
        return 0;
    return sickle_hash_bytes(&HF_PUNISHER_CRYPTO, bytes, size, output);
}
#endif

static int hf_punisher_hash_matches(const void* bytes, size_t size, const char* expected)
{
    char actual[65];
    return hf_punisher_hash(bytes, size, actual) && strcmp(actual, expected) == 0;
}

static uint32_t hf_punisher_u32(const uint8_t* bytes, size_t offset)
{
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1u] << 8) |
           ((uint32_t)bytes[offset + 2u] << 16) | ((uint32_t)bytes[offset + 3u] << 24);
}

static void hf_punisher_put_u32(uint8_t* bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8);
    bytes[offset + 2u] = (uint8_t)(value >> 16);
    bytes[offset + 3u] = (uint8_t)(value >> 24);
}

static float hf_punisher_f32(const uint8_t* bytes, size_t offset)
{
    float value;
    memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

static void hf_punisher_put_f32(uint8_t* bytes, size_t offset, float value)
{
    memcpy(bytes + offset, &value, sizeof(value));
}

static int hf_punisher_registry_row(const HfApi* api, const char* table, uint32_t id,
                                   size_t row_size, uintptr_t* slot, uintptr_t* row,
                                   uintptr_t* game_base, char* reason, size_t capacity)
{
    uint64_t offset;
    size_t image_size = 0u;
    uintptr_t base;
    uint64_t target = 0u;
    uint64_t slot_offset;
    uint8_t probe[8];
    uintptr_t api_row = 0u;

    if (strcmp(table, "ProjectileSettings") == 0)
        offset = HF_PUNISHER_PROJECTILE_REGISTRY_OFFSET;
    else if (strcmp(table, "DamageInfo") == 0)
        offset = HF_PUNISHER_DAMAGE_REGISTRY_OFFSET;
    else
    {
        (void)snprintf(reason, capacity, "Punisher requested an unsupported native settings table");
        return 0;
    }

    if (api == NULL || api->read == NULL || api->module_base == NULL || sizeof(uintptr_t) != 8u ||
        id > (UINT64_MAX - offset) / 8u)
    {
        (void)snprintf(reason, capacity, "HellForge module and memory APIs are unavailable for %s", table);
        return 0;
    }
    base = api->module_base("game.dll", &image_size);
    slot_offset = offset + (uint64_t)id * 8u;
    if (base < 65536u || image_size == 0u || slot_offset > image_size || image_size - (size_t)slot_offset < 8u ||
        base > UINTPTR_MAX - (uintptr_t)slot_offset)
    {
        (void)snprintf(reason, capacity, "Punisher %s lookup table is outside the supported game image", table);
        return 0;
    }
    *slot = base + (uintptr_t)slot_offset;
    if (strcmp(table, "ProjectileSettings") == 0)
        api_row = api->projectile_row != NULL ? api->projectile_row(id) : 0u;
    else
        api_row = api->damage_row != NULL ? api->damage_row(id) : 0u;
    if (api->read(*slot, &target, sizeof(target)) != 0 || target < 65536u ||
        target >= UINT64_C(0x800000000000) || (target & 3u) != 0u)
    {
        (void)snprintf(reason, capacity,
                       "Punisher %s id %u lookup at RVA 0x%llX is invalid (slot=0x%llX, HellForge row=0x%llX)",
                       table, id, (unsigned long long)slot_offset,
                       (unsigned long long)target, (unsigned long long)api_row);
        return 0;
    }
    *row = (uintptr_t)target;
    if (row_size == 0u || api->read(*row, probe, row_size > sizeof(probe) ? sizeof(probe) : row_size) != 0)
    {
        (void)snprintf(reason, capacity, "Punisher %s id %u does not point to readable settings data", table, id);
        return 0;
    }
    if (game_base != NULL)
        *game_base = base;
    return 1;
}

static int hf_punisher_profile_hash_matches(const HfPunisherRecord* row, uint32_t row_id,
                                           uint32_t normal_projectile, uint32_t pellet_projectile,
                                           uint64_t child_entity)
{
    uint8_t normalized[HF_PUNISHER_MAX_ROW_SIZE];

    if (row == NULL || row_id >= HF_PUNISHER_ROW_COUNT || row->stride > sizeof(normalized))
        return 0;
    memcpy(normalized, row->after, row->stride);
    if (row_id == HF_PUNISHER_PARENT_CUSTOMIZATION)
    {
        uint64_t actual;
        memcpy(&actual, normalized + 192u, 8u);
        if (!child_entity || child_entity == HF_PUNISHER_CHILD_RESOURCE || actual != child_entity ||
            memcmp(normalized + 16u, "\0\0\0\0\0\0\0\0", 8u))
            return 0;
        /* Compare against the accepted profile after normalizing only the
         * removed default delta and its private child resource. */
        hf_punisher_put_u32(normalized, 16u, 1u);
        hf_punisher_put_u32(normalized, 20u, UINT32_C(0xA051BF6B));
        actual = HF_PUNISHER_CHILD_RESOURCE;
        memcpy(normalized + 192u, &actual, 8u);
    }
    else if (row_id == HF_PUNISHER_PARENT_PROJECTILE)
    {
        if (hf_punisher_u32(normalized, 0u) != normal_projectile)
            return 0;
        hf_punisher_put_u32(normalized, 0u, 57u);
    }
    else if (row_id == HF_PUNISHER_CHILD_PROJECTILE)
    {
        if (hf_punisher_u32(normalized, 0u) != pellet_projectile)
            return 0;
        hf_punisher_put_u32(normalized, 0u, 171u);
    }
    else if (row_id == HF_PUNISHER_CHILD_ROUNDS)
    {
        if (hf_punisher_u32(normalized, 64u) != pellet_projectile)
            return 0;
        hf_punisher_put_u32(normalized, 64u, 171u);
    }
    return hf_punisher_hash_matches(normalized, row->stride,
                                    HF_PUNISHER_SOURCE[row_id].profile_sha256);
}

static int hf_punisher_build_profile(HfPunisherRecord rows[HF_PUNISHER_ROW_COUNT],
                                     const uint8_t reload_donor[HF_PUNISHER_RELOAD_SIZE],
                                     uint32_t normal_projectile, uint32_t pellet_projectile,
                                     uint64_t child_entity,
                                     char* reason, size_t capacity)
{
    static const uint8_t display_names[16] = {
        0xA7, 0x0D, 0x93, 0x91, 0xC0, 0xE4, 0xD9, 0x07,
        0xBB, 0x6B, 0x89, 0x2F, 0x96, 0xB0, 0x7B, 0x06
    };
    float sway;
    uint32_t i;

    if (!child_entity || child_entity == HF_PUNISHER_CHILD_RESOURCE ||
        child_entity == HF_PUNISHER_PARENT_RESOURCE ||
        normal_projectile < HF_PUNISHER_FIRST_PROJECTILE_ID ||
        normal_projectile > HF_PUNISHER_LAST_PROJECTILE_ID ||
        pellet_projectile < HF_PUNISHER_FIRST_PROJECTILE_ID ||
        pellet_projectile > HF_PUNISHER_LAST_PROJECTILE_ID ||
        normal_projectile == pellet_projectile)
    {
        (void)snprintf(reason, capacity, "HellForge returned invalid or colliding Punisher projectile ids");
        return 0;
    }

    for (i = 0u; i < HF_PUNISHER_ROW_COUNT; ++i)
    {
        if (rows[i].stride != HF_PUNISHER_SOURCE[i].stride)
        {
            (void)snprintf(reason, capacity, "%s record stride changed", HF_PUNISHER_SOURCE[i].label);
            return 0;
        }
        memcpy(rows[i].after, rows[i].before, rows[i].stride);
    }

    if (hf_punisher_u32(rows[HF_PUNISHER_PARENT_PROJECTILE].before, 0u) != 57u ||
        hf_punisher_f32(rows[HF_PUNISHER_PARENT_WEAPON].before, 84u) != 8.0f ||
        hf_punisher_f32(rows[HF_PUNISHER_PARENT_WEAPON].before, 88u) != 8.0f)
    {
        (void)snprintf(reason, capacity, "stock projectile or spread source differs from accepted profile");
        return 0;
    }

    if (memcmp(rows[HF_PUNISHER_PARENT_CUSTOMIZATION].before + 16u, "\0\0\0\0\0\0\0\0", 8u) != 0)
    {
        (void)snprintf(reason, capacity, "default One-Two customization slot is occupied");
        return 0;
    }

    sway = hf_punisher_f32(rows[HF_PUNISHER_PARENT_WEAPON].before, 104u);
    if (!(sway >= 0.0f && sway <= 100.0f))
    {
        (void)snprintf(reason, capacity, "stock sway is outside the accepted range");
        return 0;
    }

    /* Parent: exact accepted NORMAL/SHOTGUN left-function selector and handling. */
    hf_punisher_put_f32(rows[HF_PUNISHER_PARENT_WEAPON].after, 84u, 8.0f);
    hf_punisher_put_f32(rows[HF_PUNISHER_PARENT_WEAPON].after, 88u, 8.0f);
    hf_punisher_put_f32(rows[HF_PUNISHER_PARENT_WEAPON].after, 104u, sway * 0.5f);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_WEAPON].after, 140u, 1u);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_WEAPON].after, 144u, 2u);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_WEAPON].after, 148u, 0u);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_WEAPON].after, 152u, 0u);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_WEAPON].after, 156u, 8u);
    memcpy(rows[HF_PUNISHER_PARENT_WEAPON].after + 160u, display_names, sizeof(display_names));
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_WEAPON].after, 176u, UINT32_C(0xF3222616));
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_WEAPON].after, 180u, UINT32_C(0xFF4B018D));
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_WEAPON].after, 184u, 11u);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_WEAPON].after, 188u, 0u);

    /* Normal: 60 RPM, 17 loaded and one spare magazine. */
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_PROJECTILE].after, 0u, normal_projectile);
    hf_punisher_put_f32(rows[HF_PUNISHER_PARENT_PROJECTILE].after, 8u, 60.0f);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_MAGAZINE].after, 136u, 17u);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_MAGAZINE].after, 140u, 1u);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_MAGAZINE].after, 144u, 1u);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_MAGAZINE].after, 148u, 1u);

    /* The old default attachment overwrites +192 with the stock One-Two.
     * Its entire delta is already represented by the parent profile (+156,
     * +4852 and +192), so retain the empty default slot and use our entity. */
    memcpy(rows[HF_PUNISHER_PARENT_CUSTOMIZATION].after + 192u, &child_entity, 8u);
    hf_punisher_put_u32(rows[HF_PUNISHER_PARENT_CUSTOMIZATION].after, 4852u, 2u);

    /* Child: copy the accepted stock handling prefix, then apply pellet spread/cadence. */
    memcpy(rows[HF_PUNISHER_CHILD_WEAPON].after,
           rows[HF_PUNISHER_PARENT_WEAPON].before, 60u);
    hf_punisher_put_f32(rows[HF_PUNISHER_CHILD_WEAPON].after, 84u, 100.0f);
    hf_punisher_put_f32(rows[HF_PUNISHER_CHILD_WEAPON].after, 88u, 90.0f);
    hf_punisher_put_u32(rows[HF_PUNISHER_CHILD_WEAPON].after, 140u, 1u);
    hf_punisher_put_f32(rows[HF_PUNISHER_CHILD_WEAPON].after, 104u,
                        hf_punisher_f32(rows[HF_PUNISHER_PARENT_WEAPON].after, 104u));
    rows[HF_PUNISHER_CHILD_WEAPON].after[132u] = rows[HF_PUNISHER_PARENT_WEAPON].before[132u];

    /* Shotgun: nine pellets from the game-provided Loyalist profile, one round per trigger. */
    memcpy(rows[HF_PUNISHER_CHILD_PROJECTILE].after,
           rows[HF_PUNISHER_PARENT_PROJECTILE].before, HF_PUNISHER_PROJECTILE_SIZE);
    hf_punisher_put_u32(rows[HF_PUNISHER_CHILD_PROJECTILE].after, 0u, pellet_projectile);
    hf_punisher_put_f32(rows[HF_PUNISHER_CHILD_PROJECTILE].after, 8u, 100.0f);
    rows[HF_PUNISHER_CHILD_PROJECTILE].after[237u] = 0u;

    hf_punisher_put_u32(rows[HF_PUNISHER_CHILD_ROUNDS].after, 64u, pellet_projectile);
    hf_punisher_put_f32(rows[HF_PUNISHER_CHILD_ROUNDS].after, 72u, 20.0f);
    hf_punisher_put_u32(rows[HF_PUNISHER_CHILD_ROUNDS].after, 80u, 60u);
    hf_punisher_put_u32(rows[HF_PUNISHER_CHILD_ROUNDS].after, 84u, 60u);
    hf_punisher_put_u32(rows[HF_PUNISHER_CHILD_ROUNDS].after, 88u, 60u);
    rows[HF_PUNISHER_CHILD_ROUNDS].after[104u] = 0u;

    if (reload_donor == NULL || !hf_punisher_hash_matches(reload_donor, HF_PUNISHER_RELOAD_SIZE,
                                                          HF_PUNISHER_RELOAD_DONOR_SHA256))
    {
        (void)snprintf(reason, capacity, "reload donor no longer matches its source pin");
        return 0;
    }
    /* Keep the donor as a stock identity/layout witness. Its Sweeper 2773
     * ability and event sequence are not the accepted underbarrel template. */
    for (i = 0u; i < 13u; ++i)
        hf_punisher_put_u32(rows[HF_PUNISHER_CHILD_RELOAD].after, 4u + i * 4u,
                           HF_PUNISHER_RELOAD_TEMPLATE_WORDS[i]);
    hf_punisher_put_f32(rows[HF_PUNISHER_CHILD_RELOAD].after + 0u, 56u, 121.0f / 60.0f);

    for (i = 0u; i < HF_PUNISHER_ROW_COUNT; ++i)
    {
        if (!hf_punisher_profile_hash_matches(&rows[i], i, normal_projectile, pellet_projectile, child_entity))
        {
            (void)snprintf(reason, capacity, "%s transform differs from its accepted output pin",
                           HF_PUNISHER_SOURCE[i].label);
            return 0;
        }
    }
    return 1;
}

static int hf_punisher_value_equal(HfValue left, HfValue right)
{
    if (left.type != right.type)
        return 0;
    return left.type == HF_TYPE_U64 ? left.u64 == right.u64 : left.number == right.number;
}

static int hf_punisher_append_live(HfPunisherLiveEdit edits[24], size_t* count,
                                   HfRef target, HfField field,
                                   HfValue expected_base, HfValue value, uint32_t stock_row,
                                   uint32_t flags)
{
    if (count == NULL || *count >= 24u)
        return 0;
    edits[*count].target = target;
    edits[*count].field = field;
    edits[*count].expected_base = expected_base;
    edits[*count].value = value;
    edits[*count].stock_row = stock_row;
    edits[*count].flags = flags;
    ++*count;
    return 1;
}

static int hf_punisher_verify_row_hash(const HfApi* api, uintptr_t address, size_t size,
                                       const char* expected, const char* label,
                                       char* reason, size_t capacity)
{
    uint8_t bytes[HF_PUNISHER_PROJECTILE_SIZE];
    if (api == NULL || api->read == NULL || address < 65536u || size > sizeof(bytes) ||
        api->read(address, bytes, size) != 0 || !hf_punisher_hash_matches(bytes, size, expected))
    {
        (void)snprintf(reason, capacity, "%s source row is missing or differs from the accepted game data", label);
        return 0;
    }
    return 1;
}

static int hf_punisher_verify_custom_row(const HfApi* api, uintptr_t address, size_t size,
                                         uint32_t id, uint32_t base_id,
                                         uint32_t normalized_damage, uint32_t expected_damage,
                                         const char* expected, const char* label,
                                         char* reason, size_t capacity)
{
    uint8_t bytes[HF_PUNISHER_PROJECTILE_SIZE];
    if (api == NULL || api->read == NULL || address < 65536u || size > sizeof(bytes) ||
        api->read(address, bytes, size) != 0 || hf_punisher_u32(bytes, 0u) != id)
    {
        (void)snprintf(reason, capacity, "%s registered row is missing or has the wrong id", label);
        return 0;
    }
    if (expected_damage != UINT32_MAX && hf_punisher_u32(bytes, 60u) != expected_damage)
    {
        (void)snprintf(reason, capacity, "%s registered row does not point at its owned damage type", label);
        return 0;
    }
    hf_punisher_put_u32(bytes, 0u, base_id);
    if (normalized_damage != UINT32_MAX)
        hf_punisher_put_u32(bytes, 60u, normalized_damage);
    if (!hf_punisher_hash_matches(bytes, size, expected))
    {
        (void)snprintf(reason, capacity, "%s does not match its source-pinned donor row", label);
        return 0;
    }
    return 1;
}

#include "punisher_explosions.inc"

static int hf_punisher_preflight_live(const HfApi* api, HfMod* mod,
                                      const HfPunisherLiveEdit* edits, size_t count,
                                      char* reason, size_t capacity)
{
    size_t i;
    (void)mod;
    if (api == NULL || api->get_live == NULL)
    {
        (void)snprintf(reason, capacity, "HellForge live-field reads are unavailable");
        return 0;
    }
    for (i = 0u; i < count; ++i)
    {
        HfLiveValue current;
        const HfPunisherLiveEdit* edit = &edits[i];
        memset(&current, 0, sizeof(current));
        if (api->get_live(mod, edit->target, &edit->field, &current) == 0)
        {
            (void)snprintf(reason, capacity, "could not read Punisher field %s+0x%x",
                           edit->field.component, (unsigned)edit->field.offset);
            return 0;
        }
        if (!hf_punisher_value_equal(current.now, current.base) ||
            !hf_punisher_value_equal(current.base, edit->expected_base))
        {
            (void)snprintf(reason, capacity, "Punisher field %s+0x%x is changed or differs from its pinned base",
                           edit->field.component, (unsigned)edit->field.offset);
            return 0;
        }
        if (edit->stock_row != 0u)
        {
            if (current.mod != NULL && current.mod[0] != '\0')
            {
            (void)snprintf(reason, capacity, "stock settings field is already based on mod %s", current.mod);
                return 0;
            }
        }
        else if (current.mod != NULL && current.mod[0] != '\0' &&
                 strcmp(current.mod, HF_PUNISHER_MOD_ID) != 0)
        {
            (void)snprintf(reason, capacity, "Punisher-owned settings row is based on unexpected mod %s", current.mod);
            return 0;
        }
    }
    return 1;
}

static int hf_punisher_live_owned(const HfPunisherLiveEdit* edit, const HfLiveValue* current)
{
    return current->mod != NULL && strcmp(current->mod, HF_PUNISHER_MOD_ID) == 0 &&
           hf_punisher_value_equal(current->base, edit->value) &&
           (hf_punisher_value_equal(current->now, edit->value) ||
            hf_punisher_value_equal(current->now, edit->expected_base));
}

static int hf_punisher_live_base(const HfPunisherLiveEdit* edit, const HfLiveValue* current)
{
    return hf_punisher_value_equal(current->now, edit->expected_base) &&
           hf_punisher_value_equal(current->base, edit->expected_base) &&
           (current->mod == NULL || current->mod[0] == '\0' ||
            (edit->stock_row == 0u && strcmp(current->mod, HF_PUNISHER_MOD_ID) == 0));
}

static int hf_punisher_reset_content(const HfApi* api, HfMod* mod,
                                     HfPunisherContent* content, char* reason, size_t capacity)
{
    int failed = 0;
    while (content != NULL && content->applied_count != 0u)
    {
        HfPunisherLiveEdit* edit = &content->applied[content->applied_count - 1u];
        HfLiveValue current;
        memset(&current, 0, sizeof(current));
        if (api == NULL || api->get_live == NULL ||
            api->get_live(mod, edit->target, &edit->field, &current) == 0)
        {
            failed = 1;
            (void)snprintf(reason, capacity, "could not restore Punisher field %s+0x%x",
                           edit->field.component, (unsigned)edit->field.offset);
        }
        else if (hf_punisher_live_owned(edit, &current) &&
                 api->set_live != NULL &&
                 api->set_live(mod, edit->target, &edit->field, NULL,
                               edit->flags) != 0 &&
                 api->get_live(mod, edit->target, &edit->field, &current) != 0 &&
                 hf_punisher_live_base(edit, &current))
        {
            /* The caller removes only this mod's live edit; read back the base. */
        }
        else if (hf_punisher_live_base(edit, &current) && !hf_punisher_live_owned(edit, &current))
        {
            /* A refused write or a complete resource reload already restored this field. */
        }
        else
        {
            failed = 1;
            (void)snprintf(reason, capacity, "ownership or value changed before restoring Punisher field %s+0x%x",
                           edit->field.component, (unsigned)edit->field.offset);
        }
        --content->applied_count;
    }
    if (hf_punisher_explosion_reset(api, reason, capacity) < 0) failed = 1;
    if (content != NULL)
    {
        content->configured = 0u;
        if (failed)
            content->poisoned = 1u;
    }
    return failed ? -1 : 1;
}

/* A resource reload can restore live fields while keeping the mod loaded.
 * Distinguish that clean base state from a foreign or partial write before
 * the ready callback reapplies the accepted profile. */
static int hf_punisher_check_configured_content(const HfApi* api, HfMod* mod,
                                               HfPunisherContent* content,
                                               char* reason, size_t capacity)
{
    size_t i;
    int all_applied = 1;
    int all_base = 1;
    if (content == NULL || content->configured == 0u)
        return 1;
    for (i = 0u; i < content->applied_count; ++i)
    {
        HfLiveValue current;
        const HfPunisherLiveEdit* edit = &content->applied[i];
        memset(&current, 0, sizeof(current));
        if (api == NULL || api->get_live == NULL ||
            api->get_live(mod, edit->target, &edit->field, &current) == 0)
        {
            (void)snprintf(reason, capacity, "could not verify configured Punisher field %s+0x%x",
                           edit->field.component, (unsigned)edit->field.offset);
            return 0;
        }
        if (hf_punisher_live_owned(edit, &current) && hf_punisher_value_equal(current.now, edit->value))
        {
            all_base = 0;
            continue;
        }
        if (hf_punisher_live_base(edit, &current) ||
            (hf_punisher_live_owned(edit, &current) && hf_punisher_value_equal(current.now, edit->expected_base)))
        {
            all_applied = 0;
            continue;
        }
        (void)snprintf(reason, capacity, "configured Punisher field %s+0x%x has a foreign or partial value",
                       edit->field.component, (unsigned)edit->field.offset);
        return 0;
    }
    if (all_applied)
        return 2;
    if (all_base)
    {
        if (hf_punisher_reset_content(api, mod, content, reason, capacity) < 0) return -1;
        return 1;
    }
    if (hf_punisher_reset_content(api, mod, content, reason, capacity) < 0)
        return -1;
    return 1;
}

static int hf_punisher_configure_content(const HfApi* api, HfMod* mod,
                                         HfPunisherContent* content,
                                         char* reason, size_t capacity)
{
    HfPunisherLiveEdit edits[24];
    uintptr_t normal_projectile_row;
    uintptr_t pellet_projectile_row;
    uintptr_t normal_damage_row;
    uintptr_t pellet_damage_row;
    uintptr_t stock_normal_projectile;
    uintptr_t stock_pellet_projectile;
    uintptr_t stock_normal_damage;
    uintptr_t stock_pellet_damage;
    uintptr_t burning_damage;
    uintptr_t stock_slot;
    size_t count = 0u;
    size_t i;
    uint32_t api_need = (uint32_t)(offsetof(HfApi, damage_row) + sizeof(api->damage_row));

    if (content == NULL || content->poisoned != 0u)
    {
        (void)snprintf(reason, capacity, "Punisher content setup is poisoned after an incomplete rollback");
        return -1;
    }
    if (content->available == 0u)
    {
        (void)snprintf(reason, capacity, "Punisher registered projectile and damage content is unavailable");
        return 0;
    }
    {
        int configured = hf_punisher_check_configured_content(api, mod, content, reason, capacity);
        if (configured != 1)
        {
            if (configured == 2 && !hf_punisher_explosion_check(api, reason, capacity)) return 0;
            return configured;
        }
    }
    if (content->available == 0u || api == NULL || api->size < api_need ||
        api->read == NULL || api->get_live == NULL || api->set_live == NULL ||
        api->projectile_row == NULL || api->damage_row == NULL || api->module_base == NULL)
    {
        (void)snprintf(reason, capacity, "HellForge did not provide the registered Punisher content APIs");
        return 0;
    }

    if (!hf_punisher_registry_row(api, "ProjectileSettings", 57u, 272u,
                                  &stock_slot, &stock_normal_projectile, &content->game_base,
                                  reason, capacity) ||
        !hf_punisher_registry_row(api, "ProjectileSettings", 171u, 272u,
                                  &stock_slot, &stock_pellet_projectile, NULL,
                                  reason, capacity) ||
        !hf_punisher_registry_row(api, "DamageInfo", 311u, 76u,
                                  &stock_slot, &stock_normal_damage, NULL,
                                  reason, capacity) ||
        !hf_punisher_registry_row(api, "DamageInfo", 305u, 76u,
                                  &stock_slot, &stock_pellet_damage, NULL,
                                  reason, capacity) ||
        !hf_punisher_registry_row(api, "DamageInfo", 368u, 76u,
                                  &stock_slot, &burning_damage, NULL,
                                  reason, capacity))
        return 0;
    normal_projectile_row = api->projectile_row(content->normal_projectile);
    pellet_projectile_row = api->projectile_row(content->pellet_projectile);
    normal_damage_row = api->damage_row(content->normal_damage);
    pellet_damage_row = api->damage_row(content->pellet_damage);
    if (!hf_punisher_verify_row_hash(api, stock_normal_projectile, 272u,
                                     HF_PUNISHER_NORMAL_PROJECTILE_SHA256, "normal projectile",
                                     reason, capacity) ||
        !hf_punisher_verify_row_hash(api, stock_pellet_projectile, 272u,
                                     HF_PUNISHER_PELLET_PROJECTILE_SHA256, "pellet projectile",
                                     reason, capacity) ||
        !hf_punisher_verify_row_hash(api, stock_normal_damage, 76u,
                                     HF_PUNISHER_NORMAL_DAMAGE_SHA256, "normal damage",
                                     reason, capacity) ||
        !hf_punisher_verify_row_hash(api, stock_pellet_damage, 76u,
                                     HF_PUNISHER_PELLET_DAMAGE_SHA256, "pellet damage",
                                     reason, capacity) ||
        !hf_punisher_verify_row_hash(api, burning_damage, 76u,
                                     HF_PUNISHER_BURNING_DAMAGE_SHA256, "burning reference",
                                     reason, capacity) ||
        !hf_punisher_verify_custom_row(api, normal_projectile_row, 272u,
                                       content->normal_projectile, 57u, UINT32_MAX, UINT32_MAX,
                                       HF_PUNISHER_NORMAL_PROJECTILE_SHA256, "normal projectile",
                                       reason, capacity) ||
        !hf_punisher_verify_custom_row(api, pellet_projectile_row, 272u,
                                       content->pellet_projectile, 171u, 56u,
                                       content->pellet_damage,
                                       HF_PUNISHER_PELLET_PROJECTILE_SHA256, "pellet projectile",
                                       reason, capacity) ||
        !hf_punisher_verify_custom_row(api, normal_damage_row, 76u,
                                       content->normal_damage, 311u, UINT32_MAX, UINT32_MAX,
                                       HF_PUNISHER_NORMAL_DAMAGE_SHA256, "normal damage",
                                       reason, capacity) ||
        !hf_punisher_verify_custom_row(api, pellet_damage_row, 76u,
                                       content->pellet_damage, 305u, UINT32_MAX, UINT32_MAX,
                                       HF_PUNISHER_PELLET_DAMAGE_SHA256, "pellet damage",
                                       reason, capacity))
        return 0;
#define HF_PUNISHER_ADD_LIVE(target_, component_, field_, base_, value_, stock_, flags_) \
    do { \
        if (!hf_punisher_append_live(edits, &count, (HfRef)(target_), \
                (HfField)HF_FIELD(component_, field_), (HfValue)base_, \
                (HfValue)value_, (stock_), (flags_))) \
        { \
            (void)snprintf(reason, capacity, "Punisher settings edit count exceeded its bound"); \
            return 0; \
        } \
    } while (0)
#define HF_PUNISHER_ADD_LIVE_AT(target_, component_, field_, index_, base_, value_, stock_, flags_) \
    do { \
        if (!hf_punisher_append_live(edits, &count, (HfRef)(target_), \
                (HfField)HF_FIELD_AT(component_, field_, index_), (HfValue)base_, \
                (HfValue)value_, (stock_), (flags_))) \
        { \
            (void)snprintf(reason, capacity, "Punisher settings edit count exceeded its bound"); \
            return 0; \
        } \
    } while (0)

    HF_PUNISHER_ADD_LIVE(content->normal_damage, DamageInfo, standard_damage, HF_I32(225), HF_I32(350), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->normal_damage, DamageInfo, durable_damage, HF_I32(225), HF_I32(225), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->normal_damage, DamageInfo, demolition, HF_U32(10u), HF_U32(30u), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->normal_damage, DamageInfo, stagger, HF_U32(35u), HF_U32(25u), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->normal_damage, DamageInfo, push_force, HF_U32(30u), HF_U32(30u), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->normal_damage, DamageInfo, status_1_type, HF_U32(0u), HF_U32(HF_STATUS_FIRE), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->normal_damage, DamageInfo, status_1_strength, HF_F32(0.0f), HF_F32(100.0f), 0u, 0u);

    HF_PUNISHER_ADD_LIVE(content->pellet_damage, DamageInfo, durable_damage, HF_I32(25), HF_I32(9), 0u, 0u);
    HF_PUNISHER_ADD_LIVE_AT(content->pellet_damage, DamageInfo, armor_penetration, 1u, HF_U32(0u), HF_U32(3u), 0u, 0u);
    HF_PUNISHER_ADD_LIVE_AT(content->pellet_damage, DamageInfo, armor_penetration, 2u, HF_U32(0u), HF_U32(3u), 0u, 0u);
    HF_PUNISHER_ADD_LIVE_AT(content->pellet_damage, DamageInfo, armor_penetration, 3u, HF_U32(0u), HF_U32(3u), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->pellet_damage, DamageInfo, stagger, HF_U32(10u), HF_U32(25u), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->pellet_damage, DamageInfo, push_force, HF_U32(5u), HF_U32(25u), 0u, 0u);

    HF_PUNISHER_ADD_LIVE(content->normal_projectile, ProjectileSettings, drag, HF_F32(4.0f), HF_F32(1.2f), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->normal_projectile, ProjectileSettings, gravity, HF_F32(3.0f), HF_F32(1.0f), 0u, 0u);
    HF_PUNISHER_ADD_LIVE(content->pellet_projectile, ProjectileSettings, pellet_count, HF_U32(1u), HF_U32(9u), 0u, 0u);


#undef HF_PUNISHER_ADD_LIVE_AT
#undef HF_PUNISHER_ADD_LIVE

    if (count > sizeof(content->applied) / sizeof(content->applied[0]) ||
        !hf_punisher_preflight_live(api, mod, edits, count, reason, capacity))
        return 0;

    {
        int explosions = hf_punisher_explosion_bind(api, content, reason, capacity);
        if (explosions < 1) return explosions;
    }

    content->applied_count = 0u;
    for (i = 0u; i < count; ++i)
    {
        HfLiveValue current;
        content->applied[content->applied_count++] = edits[i];
        if (api->set_live(mod, edits[i].target, &edits[i].field, &edits[i].value,
                          edits[i].flags) == 0 ||
            api->get_live(mod, edits[i].target, &edits[i].field, &current) == 0 ||
            !hf_punisher_value_equal(current.now, edits[i].value) ||
            !hf_punisher_live_owned(&edits[i], &current))
        {
            (void)snprintf(reason, capacity, "Punisher settings write or readback failed at %s+0x%x",
                           edits[i].field.component, (unsigned)edits[i].field.offset);
            if (hf_punisher_reset_content(api, mod, content, reason, capacity) < 0)
                return -1;
            return 0;
        }
    }
    content->configured = 1u;
    return 1;
}

static int hf_punisher_register_content(const HfApi* api, HfMod* mod,
                                        HfPunisherContent* content)
{
    HfDamage normal_damage;
    HfDamage pellet_damage;
    HfProjectile normal_projectile;
    HfProjectile pellet_projectile;
    uint32_t need = (uint32_t)(offsetof(HfApi, add_projectile) + sizeof(api->add_projectile));
    if (content == NULL)
        return 0;
    memset(content, 0, sizeof(*content));
    if (api == NULL || api->size < need ||
        api->size < offsetof(HfApi, add_entity) + sizeof(api->add_entity) ||
        api->add_entity == NULL || api->resource_hash == NULL ||
        api->add_damage == NULL || api->add_projectile == NULL)
    {
        if (api != NULL && api->log != NULL)
            api->log(mod, "Punisher requires HellForge entity, damage and projectile registration APIs");
        return 0;
    }

    memset(&normal_damage, 0, sizeof(normal_damage));
    normal_damage.size = (uint32_t)sizeof(normal_damage);
    normal_damage.base = 311u;
    normal_damage.name = "superearth.arsenal-native-port/punisher-plasma/normal-blast";
    normal_damage.scale = 1.0f;
    content->normal_damage = api->add_damage(mod, &normal_damage);
    if (content->normal_damage < 650u)
        goto failed;

    memset(&pellet_damage, 0, sizeof(pellet_damage));
    pellet_damage.size = (uint32_t)sizeof(pellet_damage);
    pellet_damage.base = 305u;
    pellet_damage.name = "superearth.arsenal-native-port/punisher-plasma/pellet-impact";
    pellet_damage.scale = 1.0f;
    content->pellet_damage = api->add_damage(mod, &pellet_damage);
    if (content->pellet_damage < 650u || content->pellet_damage == content->normal_damage)
        goto failed;

    memset(&normal_projectile, 0, sizeof(normal_projectile));
    normal_projectile.size = (uint32_t)sizeof(normal_projectile);
    normal_projectile.base = 57u;
    normal_projectile.name = "superearth.arsenal-native-port/punisher-plasma/normal-shot";
    /* Keep stock direct-impact behavior; the accepted 350/225 profile belongs to explosion 290. */
    content->normal_projectile = api->add_projectile(mod, &normal_projectile);
    if (content->normal_projectile < HF_PUNISHER_FIRST_PROJECTILE_ID ||
        content->normal_projectile > HF_PUNISHER_LAST_PROJECTILE_ID)
        goto failed;

    memset(&pellet_projectile, 0, sizeof(pellet_projectile));
    pellet_projectile.size = (uint32_t)sizeof(pellet_projectile);
    pellet_projectile.base = 171u;
    pellet_projectile.name = "superearth.arsenal-native-port/punisher-plasma/shotgun-pellet";
    pellet_projectile.damage = content->pellet_damage;
    content->pellet_projectile = api->add_projectile(mod, &pellet_projectile);
    if (content->pellet_projectile < HF_PUNISHER_FIRST_PROJECTILE_ID ||
        content->pellet_projectile > HF_PUNISHER_LAST_PROJECTILE_ID ||
        content->pellet_projectile == content->normal_projectile)
        goto failed;

    {
        static const char* const owned[] = {
            "WeaponDataComponentData", "ProjectileWeaponComponentData",
            "WeaponRoundsComponentData", "WeaponReloadComponentData", NULL
        };
        static const HfEntity child = {
            .size = sizeof(HfEntity), .base = HF_PUNISHER_CHILD_RESOURCE,
            .name = HF_PUNISHER_PRIVATE_CHILD_NAME, .own_components = owned,
            .unit = HF_PUNISHER_PRIVATE_CHILD_NAME
        };
        content->child_entity = api->add_entity(mod, &child);
        if (!content->child_entity || content->child_entity == HF_PUNISHER_CHILD_RESOURCE ||
            content->child_entity != api->resource_hash(HF_PUNISHER_PRIVATE_CHILD_NAME))
            goto failed;
    }
    content->available = 1u;
    return 1;

failed:
    content->available = 0u;
    if (api->log != NULL)
        api->log(mod, "Punisher custom content registration was incomplete; the option will stay disabled");
    return 0;
}

static int hf_punisher_range_overlaps(uintptr_t left, size_t left_size,
                                      uintptr_t right, size_t right_size)
{
    if (left > UINTPTR_MAX - left_size || right > UINTPTR_MAX - right_size)
        return 1;
    return left < right + right_size && right < left + left_size;
}

#include "punisher_child_guard.generated.h"
#include "punisher_unit_guard.generated.h"
#include "punisher_unit_ownership.inc"
#include "punisher_child_ownership.inc"

static int hf_punisher_restore(const HfApi* api, HfPunisherPatch* patches,
                               size_t attempted, char* reason, size_t capacity)
{
    int restored = 1;
    while (attempted > 0u)
    {
        uint8_t current[8];
        HfPunisherPatch* patch = &patches[--attempted];
        if (patch->size == 0u || patch->size > sizeof(current) ||
            api->read(patch->address, current, patch->size) != 0)
        {
            restored = 0;
            continue;
        }
        if (memcmp(current, patch->before, patch->size) == 0)
            continue;
        if (memcmp(current, patch->after, patch->size) != 0 ||
            api->write(patch->address, patch->before, patch->size) != 0 ||
            api->read(patch->address, current, patch->size) != 0 ||
            memcmp(current, patch->before, patch->size) != 0)
            restored = 0;
    }
    if (!restored)
        (void)snprintf(reason, capacity, "FATAL: Punisher profile rollback was incomplete; stop further writes");
    return restored;
}

static int hf_punisher_commit(const HfApi* api, HfPunisherRecord rows[HF_PUNISHER_ROW_COUNT],
                              const uint8_t donor_before[HF_PUNISHER_RELOAD_SIZE],
                              uintptr_t donor_address, HfPunisherContent* content,
                              char* reason, size_t capacity)
{
    HfPunisherPatch patches[HF_PUNISHER_MAX_PATCHES];
    uint8_t current[HF_PUNISHER_MAX_ROW_SIZE];
    uint8_t donor_check[HF_PUNISHER_RELOAD_SIZE];
    size_t patch_count = 0u;
    size_t attempted = 0u;
    uint32_t row;

    for (row = 0u; row < HF_PUNISHER_ROW_COUNT; ++row)
    {
        uint32_t offset;
        if (api->read(rows[row].address, current, rows[row].stride) != 0 ||
            memcmp(current, rows[row].before, rows[row].stride) != 0)
        {
            (void)snprintf(reason, capacity, "%s changed during profile staging", HF_PUNISHER_SOURCE[row].label);
            return 0;
        }
        for (offset = 0u; offset < rows[row].stride; offset += 4u)
        {
            if (memcmp(rows[row].before + offset, rows[row].after + offset, 4u) == 0)
                continue;
            if (patch_count >= HF_PUNISHER_MAX_PATCHES)
            {
                (void)snprintf(reason, capacity, "Punisher profile exceeded the bounded write set");
                return 0;
            }
            patches[patch_count].address = rows[row].address + offset;
            patches[patch_count].size = 4u;
            memcpy(patches[patch_count].before, rows[row].before + offset, 4u);
            memcpy(patches[patch_count].after, rows[row].after + offset, 4u);
            ++patch_count;
        }
    }

    if (api->read(donor_address, donor_check, sizeof(donor_check)) != 0 ||
        memcmp(donor_check, donor_before, sizeof(donor_check)) != 0)
    {
        (void)snprintf(reason, capacity, "reload donor changed during profile staging");
        return 0;
    }

    for (attempted = 0u; attempted < patch_count; ++attempted)
    {
        uint8_t observed[8];
        HfPunisherPatch* patch = &patches[attempted];
        if (!hf_punisher_child_owned(api, content, rows, reason, capacity) ||
            api->read(patch->address, observed, patch->size) != 0 ||
            memcmp(observed, patch->before, patch->size) != 0)
        {
            (void)snprintf(reason, capacity, "Punisher component or lookup changed before a guarded write");
            if (!hf_punisher_restore(api, patches, attempted, reason, capacity))
            {
                if (content != NULL)
                    content->poisoned = 1u;
                return -1;
            }
            return 0;
        }
        ++attempted;
        if (api->write(patch->address, patch->after, patch->size) != 0 ||
            api->read(patch->address, observed, patch->size) != 0 ||
            memcmp(observed, patch->after, patch->size) != 0)
        {
            (void)snprintf(reason, capacity, "Punisher guarded component or lookup write failed");
            if (!hf_punisher_restore(api, patches, attempted, reason, capacity))
            {
                if (content != NULL)
                    content->poisoned = 1u;
                return -1;
            }
            return 0;
        }
        --attempted;
    }

    if (!hf_punisher_child_owned(api, content, rows, reason, capacity))
    {
        if (!hf_punisher_restore(api, patches, patch_count, reason, capacity))
        { content->poisoned = 1u; return -1; }
        return 0;
    }
    for (row = 0u; row < HF_PUNISHER_ROW_COUNT; ++row)
    {
        if (api->read(rows[row].address, current, rows[row].stride) != 0 ||
            memcmp(current, rows[row].after, rows[row].stride) != 0)
        {
            (void)snprintf(reason, capacity, "%s failed profile readback", HF_PUNISHER_SOURCE[row].label);
            if (!hf_punisher_restore(api, patches, patch_count, reason, capacity))
            {
                if (content != NULL)
                    content->poisoned = 1u;
                return -1;
            }
            return 0;
        }
    }
    if (api->read(donor_address, donor_check, sizeof(donor_check)) != 0 ||
        memcmp(donor_check, donor_before, sizeof(donor_check)) != 0)
    {
        (void)snprintf(reason, capacity, "reload donor changed during profile commit");
        if (!hf_punisher_restore(api, patches, patch_count, reason, capacity))
        {
            if (content != NULL)
                content->poisoned = 1u;
            return -1;
        }
        return 0;
    }
    return 1;
}

/* Returns 1 when applied, 2 when already applied, 0 on a clean refusal, and -1 on fatal rollback. */
static int hf_punisher_apply_profile(const HfApi* api, HfMod* mod,
                                     HfPunisherRunner* runner, HfPunisherContent* content,
                                     char* reason, size_t capacity)
{
    HfPunisherRecord rows[HF_PUNISHER_ROW_COUNT];
    uint8_t donor[HF_PUNISHER_RELOAD_SIZE];
    uint32_t stride = 0u;
    uintptr_t donor_address;
    uintptr_t end;
    uint32_t i;
    int all_base = 1;
    int all_profile = 1;
    int result;

    if (runner == NULL || runner->poisoned)
    {
        (void)snprintf(reason, capacity, "Punisher transaction is poisoned after an incomplete rollback");
        return -1;
    }
    if (content == NULL || content->available == 0u || content->poisoned != 0u)
    {
        (void)snprintf(reason, capacity, "Punisher registered projectile/damage content is unavailable");
        return 0;
    }
    if (api == NULL || api->component_record == NULL || api->read == NULL || api->write == NULL ||
        api->log == NULL || api->size < (uint32_t)(offsetof(HfApi, component_record) + sizeof(api->component_record)))
    {
        (void)snprintf(reason, capacity, "HellForge component and memory APIs are unavailable");
        return 0;
    }

    memset(rows, 0, sizeof(rows));
    for (i = 0u; i < HF_PUNISHER_ROW_COUNT; ++i)
    {
        rows[i].stride = 0u;
        rows[i].address = api->component_record(HF_PUNISHER_SOURCE[i].component,
            i >= HF_PUNISHER_CHILD_WEAPON ? content->child_entity : HF_PUNISHER_SOURCE[i].resource,
            &rows[i].stride);
        if (rows[i].address < 65536u || rows[i].stride != HF_PUNISHER_SOURCE[i].stride ||
            rows[i].address > UINTPTR_MAX - rows[i].stride ||
            (rows[i].address & 3u) != 0u ||
            api->read(rows[i].address, rows[i].before, rows[i].stride) != 0)
        {
            (void)snprintf(reason, capacity, "%s is missing, shared, ambiguous, or has an unsupported layout",
                           HF_PUNISHER_SOURCE[i].label);
            return 0;
        }
        end = rows[i].address + rows[i].stride;
        (void)end;
        if (!hf_punisher_hash_matches(rows[i].before, rows[i].stride,
                                      HF_PUNISHER_SOURCE[i].base_sha256))
            all_base = 0;
        memcpy(rows[i].after, rows[i].before, rows[i].stride);
        if (!hf_punisher_profile_hash_matches(&rows[i], i,
                                              content->normal_projectile,
                                              content->pellet_projectile, content->child_entity))
            all_profile = 0;
        {
            uint32_t prior;
            for (prior = 0u; prior < i; ++prior)
            {
                if (hf_punisher_range_overlaps(rows[i].address, rows[i].stride,
                                               rows[prior].address, rows[prior].stride))
                {
                    (void)snprintf(reason, capacity, "Punisher source component ranges overlap");
                    return 0;
                }
            }
        }
    }

    if (!hf_punisher_child_owned(api, content, rows, reason, capacity))
        return 0;
    if (all_profile)
    {
        runner->applied = 1u;
        (void)snprintf(reason, capacity, "accepted Punisher/One-Two component profile is already present");
        return 2;
    }
    if (!all_base)
    {
        (void)snprintf(reason, capacity, "Punisher source rows are mixed, changed, or owned by another mod");
        return 0;
    }

    donor_address = api->component_record("WeaponReloadComponentData",
                                           HF_PUNISHER_RELOAD_DONOR_RESOURCE, &stride);
    if (donor_address < 65536u || stride != HF_PUNISHER_RELOAD_SIZE ||
        donor_address > UINTPTR_MAX - HF_PUNISHER_RELOAD_SIZE || (donor_address & 3u) != 0u ||
        hf_punisher_range_overlaps(donor_address, HF_PUNISHER_RELOAD_SIZE,
                                   rows[0].address, rows[0].stride) ||
        api->read(donor_address, donor, sizeof(donor)) != 0 ||
        !hf_punisher_hash_matches(donor, sizeof(donor), HF_PUNISHER_RELOAD_DONOR_SHA256))
    {
        (void)snprintf(reason, capacity, "accepted One-Two reload donor is missing or changed");
        return 0;
    }
    for (i = 1u; i < HF_PUNISHER_ROW_COUNT; ++i)
    {
        if (hf_punisher_range_overlaps(donor_address, HF_PUNISHER_RELOAD_SIZE,
                                       rows[i].address, rows[i].stride))
        {
            (void)snprintf(reason, capacity, "reload donor overlaps a Punisher target record");
            return 0;
        }
    }

    if (!hf_punisher_build_profile(rows, donor, content->normal_projectile,
                                   content->pellet_projectile, content->child_entity, reason, capacity))
        return 0;
    result = hf_punisher_commit(api, rows, donor, donor_address, content, reason, capacity);
    if (result == 1)
    {
        runner->applied = 1u;
        (void)snprintf(reason, capacity, "applied Punisher profile with private child %016llx; four stock One-Two rows preserved",
                       (unsigned long long)content->child_entity);
        return 1;
    }
    if (result < 0)
        runner->poisoned = 1u;
    (void)mod;
    return result;
}

static HfPunisherRunner HF_PUNISHER_RUNNER;
static HfPunisherContent HF_PUNISHER_CONTENT;

#if !defined(HF_PORT_TEST_PUNISHER)
/* Own a session-lifetime package reference before publishing the child. Armory
 * previews release their references when choices change; source-row ownership
 * alone does not keep the private model alive through those releases. */
#define HF_PUNISHER_MODEL_PACKAGE UINT64_C(0x49F1972458F7CCEC)
static volatile LONG HF_PUNISHER_PENDING;
static volatile LONG HF_PUNISHER_PACKAGE_FAILED;
static uint64_t HF_PUNISHER_PACKAGE_WAIT_START;
static uint32_t HF_PUNISHER_PACKAGE_WAIT_LOGGED;
static void hf_punisher_apply_loaded(HfMod* mod, const HfApi* api)
{
    char reason[256];
    int result;
    result = hf_punisher_configure_content(api, mod, &HF_PUNISHER_CONTENT, reason, sizeof(reason));
    if (result < 1)
    {
        if (result < 0)
            api->log(mod, "FATAL: Punisher settings setup stopped: %s", reason);
        else
            api->log(mod, "Refused Punisher settings before component mutation: %s", reason);
        return;
    }
    result = hf_punisher_apply_profile(api, mod, &HF_PUNISHER_RUNNER,
                                       &HF_PUNISHER_CONTENT, reason, sizeof(reason));
    if (result < 1)
    {
        char reset_reason[256];
        int reset = hf_punisher_reset_content(api, mod, &HF_PUNISHER_CONTENT,
                                              reset_reason, sizeof(reset_reason));
        if (result < 0 || reset < 0)
        {
            HF_PUNISHER_RUNNER.poisoned = 1u;
            api->log(mod, "FATAL: Punisher setup stopped and requires a game restart: %s%s%s",
                     reason, reset < 0 ? "; live-row rollback failed: " : "",
                     reset < 0 ? reset_reason : "");
            return;
        }
        api->log(mod, "Refused Punisher component profile; temporary native settings were rolled back: %s", reason);
        return;
    }
    if (result == 1)
        api->log(mod, "Applied Punisher Plasma with private explosions 423/424, original Loyalist explosion 355 preserved: %s", reason);
    else if (result == 2)
        api->log(mod, "Punisher Plasma source-pinned profile is already applied: %s", reason);
}
static void hf_punisher_package_tick(HfMod* mod, float dt, void* user)
{
    const HfApi* api = (const HfApi*)user;
    (void)dt;
    if (HF_PUNISHER_PACKAGE_FAILED || !HF_PUNISHER_PENDING) return;
    int state = api->load_package(mod, HF_PUNISHER_MODEL_PACKAGE);
    if (!state || (state != HF_PACKAGE_RESIDENT &&
                   GetTickCount64() - HF_PUNISHER_PACKAGE_WAIT_START > 90000u))
    {
        InterlockedExchange(&HF_PUNISHER_PACKAGE_FAILED, 1);
        InterlockedExchange(&HF_PUNISHER_PENDING, 0);
        api->log(mod, "Refused Punisher private child: donor model package was refused or not resident within 90 seconds");
        return;
    }
    if (state != HF_PACKAGE_RESIDENT)
    {
        if (!HF_PUNISHER_PACKAGE_WAIT_LOGGED++)
            api->log(mod, "Punisher private child waiting for held donor model package 49f1972458f7ccec");
        return;
    }
    if (!hf_punisher_explosion_pins_ready(api))
    {
        if (GetTickCount64() - HF_PUNISHER_PACKAGE_WAIT_START > 90000u)
        {
            InterlockedExchange(&HF_PUNISHER_PACKAGE_FAILED, 1);
            InterlockedExchange(&HF_PUNISHER_PENDING, 0);
            api->log(mod, "Refused Punisher private explosions: lookup pins not ready or changed after 90 seconds");
        }
        return;
    }
    if (!InterlockedExchange(&HF_PUNISHER_PENDING, 0)) return;
    api->log(mod, "Punisher private child model package resident and held for the session");
    hf_punisher_apply_loaded(mod, api);
}
static void hf_punisher_package_begin(const HfApi* api, HfMod* mod)
{
    if (api->size < offsetof(HfApi, load_package) + sizeof(api->load_package) ||
        !api->load_package || !api->on_game_thread ||
        !api->on_game_thread(mod, hf_punisher_package_tick, (void*)api))
    {
        InterlockedExchange(&HF_PUNISHER_PACKAGE_FAILED, 1);
        api->log(mod, "Refused Punisher private child: package lifetime APIs or game-thread callback unavailable");
        return;
    }
    HF_PUNISHER_PACKAGE_WAIT_START = GetTickCount64();
    if (!api->load_package(mod, HF_PUNISHER_MODEL_PACKAGE))
    {
        InterlockedExchange(&HF_PUNISHER_PACKAGE_FAILED, 1);
        api->log(mod, "Refused Punisher private child: donor model package hold unavailable");
    }
}
static void hf_punisher_apply_ready(HfMod* mod, HfEvent event, void* user)
{
    (void)mod; (void)event; (void)user;
    if (HF_PUNISHER_PACKAGE_FAILED) return;
    HF_PUNISHER_PACKAGE_WAIT_START = GetTickCount64();
    HF_PUNISHER_PACKAGE_WAIT_LOGGED = 0u;
    InterlockedExchange(&HF_PUNISHER_PENDING, 1);
}
#endif
