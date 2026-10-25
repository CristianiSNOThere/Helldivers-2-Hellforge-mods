/* Native host-only Gas Overhaul enemy effects. Detection follows the accepted, build-pinned
 * read-only status scan. Acid Storm uses the accepted game's pinned request
 * function because SDK 11 apply_status excludes status 55; stun uses the SDK. */
#include "gas_source_guard.generated.h"

#define HF_GAS_ENTITY_CAPACITY 4096u
#define HF_GAS_MARK_CAPACITY 8192u
#define HF_GAS_MAX_LOOKUP_PROBES 256u
#define HF_GAS_HEALTH_COUNT_OFFSET 4128u
#define HF_GAS_HEALTH_CAPACITY_OFFSET 4112u
#define HF_GAS_HEALTH_DESCRIPTORS_OFFSET 4168u
#define HF_GAS_HEALTH_RECORDS_OFFSET 4184u
#define HF_GAS_HEALTH_RECORD_STRIDE 440u
#define HF_GAS_STATUS_BUCKETS_OFFSET 0x12C0u
#define HF_GAS_STATUS_BUCKET_COUNT_OFFSET 0x12C8u
#define HF_GAS_STATUS_EMPTY_OFFSET 0x12CCu
#define HF_GAS_STATUS_MULTIPLIER_OFFSET 0x12D0u
#define HF_GAS_STATUS_DESCRIPTORS_OFFSET 0x12D8u
#define HF_GAS_STATUS_DATA_OFFSET 0x12E8u
#define HF_GAS_STATUS_DATA_STRIDE 0x1228u
#define HF_GAS_STATUS_DATA_SIZE 128u
#if defined(HF_PORT_TEST_GAS)
extern uint64_t hf_gas_test_now_ms;
extern int hf_gas_test_hash(const void* bytes, size_t size, char output[65]);
static uint64_t hf_gas_now_ms(void) { return hf_gas_test_now_ms; }
#else
static uint64_t hf_gas_now_ms(void) { return (uint64_t)GetTickCount64(); }
#endif

typedef struct HfGasMark
{
    uint32_t used;
    uint32_t entity;
    uint32_t descriptor_a;
    uint32_t descriptor_b;
    uintptr_t descriptor;
    uint64_t last_seen_ms;
    uint64_t next_stun_ms;
    uint64_t next_acid_ms;
} HfGasMark;

typedef struct HfGasStatusTable
{
    uintptr_t buckets;
    uintptr_t descriptors;
    uintptr_t data;
    uint32_t count;
    uint32_t empty;
    uint32_t multiplier;
} HfGasStatusTable;

typedef struct HfGasRunner
{
    const HfApi* api;
    HfMod* mod;
    uint32_t registered;
    uint32_t disabled;
    uint32_t profile_checked;
    uint32_t active;
    uint32_t cursor;
    uint32_t stun_in_window;
    uint32_t acid_in_window;
    uint32_t requested;
    uint32_t acid_requested;
    uint32_t skipped_stunned;
    uint32_t session_role;
    uint32_t session_version;
    uintptr_t game_base;
    uintptr_t definition_rows[HF_GAS_STATUS_SPEC_COUNT];
    uintptr_t world_mode;
    uintptr_t world_descriptor;
    uintptr_t world_health;
    uintptr_t world_status;
    uintptr_t world_health_descriptors;
    uintptr_t world_health_records;
    uint64_t next_scan_ms;
    uint64_t window_start_ms;
    uint64_t last_log_ms;
    uint32_t module_size;
    char last_message[256];
    HfGasMark marks[HF_GAS_MARK_CAPACITY];
} HfGasRunner;

static HfGasRunner HF_GAS_RUNNER;

#if !defined(HF_PORT_TEST_GAS)
static HfSickleCrypto HF_GAS_CRYPTO;
#endif

static int hf_gas_read(const HfGasRunner* runner, uintptr_t address, void* output, size_t size)
{
    if (runner == NULL || runner->api == NULL || runner->api->read == NULL || output == NULL ||
        address < (uintptr_t)0x10000u || size == 0u || address > UINTPTR_MAX - size)
        return 0;
    return runner->api->read(address, output, size) == 0;
}

static int hf_gas_read_u32(const HfGasRunner* runner, uintptr_t address, uint32_t* output)
{
    uint8_t bytes[4];
    if (output == NULL || !hf_gas_read(runner, address, bytes, sizeof(bytes)))
        return 0;
    *output = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
              ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    return 1;
}

static int hf_gas_read_pointer(const HfGasRunner* runner, uintptr_t address, uintptr_t* output)
{
    uint8_t bytes[8];
    uint64_t value;
    if (output == NULL || !hf_gas_read(runner, address, bytes, sizeof(bytes)))
        return 0;
    value = (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) | ((uint64_t)bytes[2] << 16) |
            ((uint64_t)bytes[3] << 24) | ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) |
            ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56);
    if (value < UINT64_C(0x10000) || value >= UINT64_C(0x800000000000) || value > UINTPTR_MAX)
        return 0;
    *output = (uintptr_t)value;
    return 1;
}

static int hf_gas_read_optional_pointer(const HfGasRunner* runner, uintptr_t address, uintptr_t* output)
{
    uint8_t bytes[8];
    uint64_t value;
    if (output == NULL || !hf_gas_read(runner, address, bytes, sizeof(bytes)))
        return 0;
    value = (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) | ((uint64_t)bytes[2] << 16) |
            ((uint64_t)bytes[3] << 24) | ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) |
            ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56);
    if (value != 0u && (value < UINT64_C(0x10000) || value >= UINT64_C(0x800000000000) || value > UINTPTR_MAX))
        return 0;
    *output = (uintptr_t)value;
    return 1;
}

static int hf_gas_add_address(uintptr_t base, uintptr_t offset, uintptr_t* result)
{
    if (base < (uintptr_t)0x10000u || base > UINTPTR_MAX - offset)
        return 0;
    *result = base + offset;
    return 1;
}

static void hf_gas_log(HfGasRunner* runner, const char* message)
{
    if (runner == NULL || message == NULL || runner->api == NULL || runner->api->log == NULL ||
        strcmp(runner->last_message, message) == 0)
        return;
    (void)snprintf(runner->last_message, sizeof(runner->last_message), "%s", message);
    runner->api->log(runner->mod, "%s", message);
}

static void hf_gas_clear_world(HfGasRunner* runner)
{
    memset(runner->marks, 0, sizeof(runner->marks));
    runner->active = 0u;
    runner->cursor = 0u;
    runner->world_mode = 0u;
    runner->world_descriptor = 0u;
    runner->world_health = 0u;
    runner->world_status = 0u;
    runner->world_health_descriptors = 0u;
    runner->world_health_records = 0u;
    runner->window_start_ms = 0u;
    runner->stun_in_window = 0u;
    runner->acid_in_window = 0u;
}

static void hf_gas_refuse(HfGasRunner* runner, const char* reason)
{
    char message[256];
    runner->disabled = 1u;
    hf_gas_clear_world(runner);
    (void)snprintf(message, sizeof(message), "Refused Gas Overhaul enemy effects: %s", reason);
    hf_gas_log(runner, message);
}

static int hf_gas_sha256(const void* bytes, size_t size, char output[65])
{
#if defined(HF_PORT_TEST_GAS)
    return hf_gas_test_hash(bytes, size, output);
#else
    if (!sickle_crypto_init(&HF_GAS_CRYPTO) || HF_GAS_CRYPTO.open == NULL || HF_GAS_CRYPTO.close == NULL ||
        HF_GAS_CRYPTO.create == NULL || HF_GAS_CRYPTO.data == NULL || HF_GAS_CRYPTO.finish == NULL ||
        HF_GAS_CRYPTO.destroy == NULL)
        return 0;
    return sickle_hash_bytes(&HF_GAS_CRYPTO, bytes, size, output);
#endif
}

static int hf_gas_address_in_module(const HfGasRunner* runner, uintptr_t address, size_t size)
{
    uintptr_t module_end;
    if (runner->game_base == 0u || runner->module_size == 0u ||
        !hf_gas_add_address(runner->game_base, runner->module_size, &module_end))
        return 0;
    return address >= runner->game_base && address <= module_end && size <= module_end - address;
}

static int hf_gas_validate_code_pin(HfGasRunner* runner)
{
    uint8_t actual[sizeof(HF_GAS_REQUEST_CODE)];
    uintptr_t address;
    if (sizeof(actual) != HF_GAS_REQUEST_CODE_SIZE ||
        !hf_gas_add_address(runner->game_base, HF_GAS_REQUEST_RVA, &address) ||
        !hf_gas_address_in_module(runner, address, sizeof(actual)) ||
        !hf_gas_read(runner, address, actual, sizeof(actual)) ||
        memcmp(actual, HF_GAS_REQUEST_CODE, sizeof(actual)) != 0)
        return 0;
    return 1;
}

static int hf_gas_validate_definitions(HfGasRunner* runner)
{
    uint32_t i;
    for (i = 0u; i < HF_GAS_STATUS_SPEC_COUNT; ++i)
    {
        const HfGasStatusSpec* spec = &HF_GAS_STATUS_SPECS[i];
        uintptr_t row_slot;
        uintptr_t row;
        uint8_t raw[152];
        uint8_t normalized[152];
        uint32_t row_id;
        char actual_hash[65];
        if (!hf_gas_add_address(runner->game_base,
                                HF_GAS_STATUS_REGISTRY_RVA + (uintptr_t)spec->id * sizeof(uint64_t), &row_slot) ||
            !hf_gas_address_in_module(runner, row_slot, sizeof(uint64_t)) ||
            !hf_gas_read_pointer(runner, row_slot, &row) ||
            !hf_gas_read(runner, row, raw, sizeof(raw)))
            return 0;
        row_id = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24);
        if (row_id != spec->id ||
            (runner->definition_rows[i] != 0u && runner->definition_rows[i] != row))
            return 0;
        memcpy(normalized, raw, sizeof(normalized));
        memset(normalized + 8u, 0, 8u);
        if (!hf_gas_sha256(normalized, sizeof(normalized), actual_hash) || strcmp(actual_hash, spec->sha256) != 0)
            return 0;
        runner->definition_rows[i] = row;
    }
    return 1;
}

static int hf_gas_profile_ready(HfGasRunner* runner)
{
    if (!hf_gas_validate_code_pin(runner))
    {
        hf_gas_refuse(runner, "the reviewed status-code fingerprint differs from game build 25480438");
        return 0;
    }
    if (!hf_gas_validate_definitions(runner))
    {
        hf_gas_refuse(runner, "Medium Stun or Acid Storm is missing, redirected, or differs from its accepted fingerprint");
        return 0;
    }
    runner->profile_checked = 1u;
    hf_gas_log(runner, "Gas Overhaul enemy status fingerprints match the accepted build profile");
    return 1;
}

static int hf_gas_is_enemy(uint64_t resource)
{
    uint32_t i;
    for (i = 0u; i < HF_GAS_ENEMY_RESOURCE_COUNT; ++i)
        if (HF_GAS_ENEMY_RESOURCES[i] == resource)
            return 1;
    return 0;
}

static int hf_gas_status_table(HfGasRunner* runner, uintptr_t status, HfGasStatusTable* table)
{
    if (!hf_gas_read_u32(runner, status + HF_GAS_STATUS_BUCKET_COUNT_OFFSET, &table->count) ||
        !hf_gas_read_u32(runner, status + HF_GAS_STATUS_EMPTY_OFFSET, &table->empty) ||
        !hf_gas_read_u32(runner, status + HF_GAS_STATUS_MULTIPLIER_OFFSET, &table->multiplier) ||
        !hf_gas_read_pointer(runner, status + HF_GAS_STATUS_BUCKETS_OFFSET, &table->buckets) ||
        !hf_gas_read_pointer(runner, status + HF_GAS_STATUS_DESCRIPTORS_OFFSET, &table->descriptors) ||
        !hf_gas_read_pointer(runner, status + HF_GAS_STATUS_DATA_OFFSET, &table->data) ||
        table->count < 2u || table->count > 65536u)
        return 0;
    return 1;
}

/* 1: found, 0: no registered status slot, -1: the table could not be read safely. */
static int hf_gas_status_index(HfGasRunner* runner, const HfGasStatusTable* table,
                               uint32_t entity, uint32_t* index)
{
    uint32_t low = table->multiplier & 0xFFFFu;
    uint32_t high = table->multiplier >> 16;
    uint64_t mixed = (uint64_t)entity * low;
    uint32_t start;
    uint32_t limit = table->count < HF_GAS_MAX_LOOKUP_PROBES ? table->count : HF_GAS_MAX_LOOKUP_PROBES;
    uint32_t probe;
    mixed += (((uint64_t)entity * high) & 0xFFFFu) << 16;
    start = (uint32_t)mixed;
    for (probe = 0u; probe < limit; ++probe)
    {
        uint32_t slot = (uint32_t)(((uint64_t)start + probe) % table->count);
        uintptr_t address;
        uint8_t entry[8];
        uint32_t id, row;
        if (!hf_gas_add_address(table->buckets, (uintptr_t)slot * 8u, &address) ||
            !hf_gas_read(runner, address, entry, sizeof(entry)))
            return -1;
        id = (uint32_t)entry[0] | ((uint32_t)entry[1] << 8) | ((uint32_t)entry[2] << 16) | ((uint32_t)entry[3] << 24);
        row = (uint32_t)entry[4] | ((uint32_t)entry[5] << 8) | ((uint32_t)entry[6] << 16) | ((uint32_t)entry[7] << 24);
        if (id == entity)
        {
            if (row >= HF_GAS_ENTITY_CAPACITY)
                return -1;
            *index = row;
            return 1;
        }
        if (id == table->empty)
            return 0;
    }
    return 0;
}

static int hf_gas_activity(HfGasRunner* runner, const HfGasStatusTable* table, uint32_t index,
                           uintptr_t* data_base, uint8_t bits[HF_GAS_STATUS_DATA_SIZE],
                           int* exposed, int* confused, int* stunned)
{
    uintptr_t slot;
    uint32_t zone;
    *exposed = 0;
    *confused = 0;
    *stunned = 0;
    if (index >= HF_GAS_ENTITY_CAPACITY ||
        !hf_gas_add_address(table->data, (uintptr_t)index * HF_GAS_STATUS_DATA_STRIDE, &slot) ||
        !hf_gas_read(runner, slot, bits, HF_GAS_STATUS_DATA_SIZE))
        return 0;
    *data_base = table->data;
    for (zone = 0u; zone < 8u; ++zone)
    {
        uint32_t upper;
        uintptr_t upper_address;
        uint32_t offset = zone * 16u + 4u;
        if (!hf_gas_add_address(slot, offset, &upper_address) || !hf_gas_read_u32(runner, upper_address, &upper))
            return 0;
        if ((upper & 0x3C00u) != 0u)
            *exposed = 1;
        if ((upper & 0x3000u) != 0u)
            *confused = 1;
        if ((upper & 0x01E0u) != 0u)
            *stunned = 1;
    }
    return 1;
}

static uint32_t hf_gas_u32(const uint8_t* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static HfGasMark* hf_gas_mark(HfGasRunner* runner, uint32_t entity, uintptr_t descriptor,
                              uint32_t descriptor_a, uint32_t descriptor_b, uint64_t now)
{
    uint32_t slot = (entity * 2654435761u) & (HF_GAS_MARK_CAPACITY - 1u);
    HfGasMark* tombstone = NULL;
    uint32_t probe;
    for (probe = 0u; probe < HF_GAS_MARK_CAPACITY; ++probe)
    {
        HfGasMark* mark = &runner->marks[(slot + probe) & (HF_GAS_MARK_CAPACITY - 1u)];
        if (mark->used == 0u)
        {
            if (tombstone != NULL)
                mark = tombstone;
            mark->used = 1u;
            mark->entity = entity;
            mark->descriptor = descriptor;
            mark->descriptor_a = descriptor_a;
            mark->descriptor_b = descriptor_b;
            mark->next_stun_ms = now;
            mark->next_acid_ms = now;
            return mark;
        }
        if (mark->used == 2u)
        {
            if (tombstone == NULL)
                tombstone = mark;
            continue;
        }
        if (mark->used == 1u && mark->entity == entity)
        {
            if (now >= mark->last_seen_ms && now - mark->last_seen_ms > 30000u)
            {
                mark->used = 2u;
                if (tombstone == NULL)
                    tombstone = mark;
                continue;
            }
            if (mark->descriptor != descriptor || mark->descriptor_a != descriptor_a ||
                mark->descriptor_b != descriptor_b)
            {
                mark->descriptor = descriptor;
                mark->descriptor_a = descriptor_a;
                mark->descriptor_b = descriptor_b;
                mark->next_stun_ms = now;
                mark->next_acid_ms = now;
            }
            return mark;
        }
        if (mark->used == 1u && now >= mark->last_seen_ms && now - mark->last_seen_ms > 30000u)
        {
            mark->used = 2u;
            if (tombstone == NULL)
                tombstone = mark;
        }
    }
    if (tombstone != NULL)
    {
        memset(tombstone, 0, sizeof(*tombstone));
        tombstone->used = 1u;
        tombstone->entity = entity;
        tombstone->descriptor = descriptor;
        tombstone->descriptor_a = descriptor_a;
        tombstone->descriptor_b = descriptor_b;
        tombstone->next_stun_ms = now;
        tombstone->next_acid_ms = now;
        return tombstone;
    }
    return NULL;
}

static int hf_gas_revalidate_target(HfGasRunner* runner, uintptr_t base, uintptr_t health,
                                    uintptr_t status, uintptr_t health_descriptors,
                                    uintptr_t health_records, uintptr_t descriptor,
                                    const uint8_t original_descriptor[24],
                                    uint32_t index, uint32_t entity, uint32_t hp,
                                    uint32_t descriptor_a, uint32_t descriptor_b,
                                    const HfGasStatusTable* table, uint32_t status_index,
                                    const uint8_t old_bits[HF_GAS_STATUS_DATA_SIZE],
                                    int require_confusion)
{
    uintptr_t current, current_descriptors, current_records, current_status;
    uintptr_t current_status_descriptors, current_status_data;
    uintptr_t row_slot;
    uint32_t life, current_hp, current_index;
    uint8_t raw[24], bits[HF_GAS_STATUS_DATA_SIZE];
    uintptr_t data_base = 0u;
    int exposed, confused, stunned;
    int index_result;
    if (!hf_gas_read_pointer(runner, base + HF_GAS_HEALTH_MANAGER_RVA, &current) || current != health ||
        !hf_gas_read_pointer(runner, base + HF_GAS_STATUS_MANAGER_RVA, &current_status) || current_status != status ||
        !hf_gas_read_pointer(runner, health + HF_GAS_HEALTH_DESCRIPTORS_OFFSET, &current_descriptors) ||
        current_descriptors != health_descriptors ||
        !hf_gas_read_pointer(runner, health + HF_GAS_HEALTH_RECORDS_OFFSET, &current_records) ||
        current_records != health_records ||
        !hf_gas_read_pointer(runner, status + HF_GAS_STATUS_DESCRIPTORS_OFFSET, &current_status_descriptors) ||
        current_status_descriptors != table->descriptors ||
        !hf_gas_read_pointer(runner, status + HF_GAS_STATUS_DATA_OFFSET, &current_status_data) ||
        current_status_data != table->data ||
        !hf_gas_add_address(health_descriptors, (uintptr_t)index * sizeof(uint64_t), &row_slot) ||
        !hf_gas_read_optional_pointer(runner, row_slot, &current) || current != descriptor ||
        !hf_gas_read(runner, descriptor, raw, sizeof(raw)) ||
        memcmp(raw, original_descriptor, sizeof(raw)) != 0)
        return 0;
    if (hf_gas_u32(raw + 8u) != entity || hf_gas_u32(raw + 12u) != descriptor_a ||
        hf_gas_u32(raw + 16u) != descriptor_b)
        return 0;
    if (!hf_gas_read_u32(runner, health_records + (uintptr_t)index * HF_GAS_HEALTH_RECORD_STRIDE + 412u, &life) ||
        !hf_gas_read_u32(runner, health_records + (uintptr_t)index * HF_GAS_HEALTH_RECORD_STRIDE + 20u, &current_hp) ||
        life != 0u || current_hp != hp)
        return 0;
    index_result = hf_gas_status_index(runner, table, entity, &current_index);
    if (index_result != 1 || current_index != status_index ||
        !hf_gas_add_address(table->descriptors, (uintptr_t)status_index * sizeof(uint64_t), &row_slot) ||
        !hf_gas_read_optional_pointer(runner, row_slot, &current) || current != descriptor ||
        !hf_gas_activity(runner, table, status_index, &data_base, bits, &exposed, &confused, &stunned) ||
        !exposed || memcmp(bits, old_bits, sizeof(bits)) != 0)
        return 0;
    if (require_confusion && (!confused || stunned))
        return 0;
    if (!hf_gas_validate_code_pin(runner) || !hf_gas_validate_definitions(runner))
        return -1;
    return 1;
}

/* Called only on the game thread after target, code and definition validation.
 * Mirrors accepted request(nil,55,target,1,target,0). Never patches game code. */
static int hf_gas_request_acid(HfGasRunner* runner, uint32_t entity)
{
    uintptr_t queue, current_queue;
    uint32_t before, after, current_count;
    if (!hf_gas_read_pointer(runner, runner->game_base + HF_GAS_QUEUE_RVA, &queue) ||
        !hf_gas_read_u32(runner, queue + HF_GAS_QUEUE_COUNT_OFFSET, &before) ||
        before > HF_GAS_QUEUE_CAPACITY)
        return -1;
    if (before == HF_GAS_QUEUE_CAPACITY)
        return 2;
    if (!hf_gas_read_pointer(runner, runner->game_base + HF_GAS_QUEUE_RVA, &current_queue) ||
        current_queue != queue ||
        !hf_gas_read_u32(runner, queue + HF_GAS_QUEUE_COUNT_OFFSET, &current_count) || current_count != before)
        return 0;
#if defined(HF_PORT_TEST_GAS_REQUEST)
    {
        extern void hf_gas_test_request(uintptr_t, void*, uint32_t, uint32_t, float, uint32_t, uint32_t);
        hf_gas_test_request(runner->game_base + HF_GAS_REQUEST_RVA, NULL, 55u, entity, 1.0f, entity, 0u);
    }
#elif defined(HF_PORT_TEST)
    return -1;
#else
    {
        typedef void (*HfGasRequest)(void*, uint32_t, uint32_t, float, uint32_t, uint32_t);
        HfGasRequest request = (HfGasRequest)(runner->game_base + HF_GAS_REQUEST_RVA);
        request(NULL, 55u, entity, 1.0f, entity, 0u);
    }
#endif
    if (!hf_gas_read_pointer(runner, runner->game_base + HF_GAS_QUEUE_RVA, &current_queue) || current_queue != queue ||
        !hf_gas_read_u32(runner, queue + HF_GAS_QUEUE_COUNT_OFFSET, &after) ||
        (after != before && after != before + 1u))
        return -1;
    return after == before + 1u ? 1 : 2;
}

/* Return 1 when submitted, 0 when the target changed during preflight, -1 on an API refusal,
 * and 2 when HellForge reports that the action is waiting. */
static int hf_gas_apply(HfGasRunner* runner, uintptr_t base, uintptr_t health, uintptr_t status,
                        uintptr_t health_descriptors, uintptr_t health_records, uintptr_t descriptor,
                        const uint8_t descriptor_raw[24], uint32_t index, uint32_t entity,
                        uint32_t hp, uint32_t descriptor_a,
                        uint32_t descriptor_b, const HfGasStatusTable* table, uint32_t status_index,
                        const uint8_t bits[HF_GAS_STATUS_DATA_SIZE], int acid)
{
    uint32_t status_id = acid ? 55u : HF_STATUS_STUN_MEDIUM;
    float buildup = acid ? 1.0f : (float)HF_GAS_STUN_BUILDUP;
    int result;
    result = hf_gas_revalidate_target(runner, base, health, status, health_descriptors, health_records,
                                      descriptor, descriptor_raw, index, entity, hp, descriptor_a, descriptor_b,
                                      table, status_index, bits, !acid);
    if (result < 0)
        return -2;
    if (result == 0)
        return 0;
    if (acid)
        return hf_gas_request_acid(runner, entity);
    result = runner->api->apply_status(runner->mod, entity, status_id, buildup);
    if (result == HF_ACT_WAITING)
        return 2;
    if (result != HF_ACT_DONE && result != HF_ACT_REQUESTED)
        return -1;
    return 1;
}

static int hf_gas_target_tick(HfGasRunner* runner, uintptr_t base, uintptr_t health,
                              uintptr_t status, uintptr_t health_descriptors,
                              uintptr_t health_records, const HfGasStatusTable* table,
                              uint32_t index, uint64_t now)
{
    uintptr_t slot, descriptor, status_slot, status_descriptor, record;
    uint8_t raw[24], bits[HF_GAS_STATUS_DATA_SIZE];
    uint32_t entity, descriptor_a, descriptor_b, flags, life, hp, status_index;
    int found, exposed, confused, stunned;
    uintptr_t data_base = 0u;
    HfGasMark* mark;
    int result;
    if (!hf_gas_add_address(health_descriptors, (uintptr_t)index * sizeof(uint64_t), &slot) ||
        !hf_gas_read_optional_pointer(runner, slot, &descriptor))
        return -1;
    if (descriptor == 0u)
        return 0;
    if (!hf_gas_read(runner, descriptor, raw, sizeof(raw)))
        return -1;
    if (!hf_gas_is_enemy(((uint64_t)hf_gas_u32(raw + 4u) << 32) | hf_gas_u32(raw)))
        return 0;
    entity = hf_gas_u32(raw + 8u);
    descriptor_a = hf_gas_u32(raw + 12u);
    descriptor_b = hf_gas_u32(raw + 16u);
    flags = hf_gas_u32(raw + 20u);
    if ((flags & 3u) != 1u || entity == 0u || entity == UINT32_MAX)
        return 0;
    record = health_records + (uintptr_t)index * HF_GAS_HEALTH_RECORD_STRIDE;
    if (!hf_gas_read_u32(runner, record + 412u, &life) || !hf_gas_read_u32(runner, record + 20u, &hp))
        return -1;
    if (life != 0u || hp == 0u || hp >= 0x80000000u)
        return 0;
    found = hf_gas_status_index(runner, table, entity, &status_index);
    if (found < 0)
        return -1;
    if (found == 0)
        return 0;
    if (!hf_gas_add_address(table->descriptors, (uintptr_t)status_index * sizeof(uint64_t), &status_slot) ||
        !hf_gas_read_optional_pointer(runner, status_slot, &status_descriptor))
        return -1;
    if (status_descriptor == 0u || status_descriptor != descriptor)
        return 0;
    if (!hf_gas_activity(runner, table, status_index, &data_base, bits, &exposed, &confused, &stunned))
        return 0;
    if (!exposed)
        return 0;
    mark = hf_gas_mark(runner, entity, descriptor, descriptor_a, descriptor_b, now);
    if (mark == NULL)
        return 0;
    mark->last_seen_ms = now;
    if (now >= mark->next_acid_ms && runner->acid_in_window < HF_GAS_MAX_ACID_PER_SECOND)
    {
        result = hf_gas_apply(runner, base, health, status, health_descriptors, health_records,
                              descriptor, raw, index, entity, hp, descriptor_a, descriptor_b,
                              table, status_index, bits, 1);
        if (result == -2)
            return -3;
        if (result < 0)
            return -2;
        if (result == 2)
            return 0; /* Queue busy: defer both effects rather than asking the SDK to reject stun. */
        if (result == 1)
        {
            mark->next_acid_ms = now + HF_GAS_ACID_REFRESH_MS;
            ++runner->acid_in_window;
            ++runner->acid_requested;
        }
    }
    if (stunned)
    {
        if (runner->skipped_stunned != UINT32_MAX)
            ++runner->skipped_stunned;
        return 0;
    }
    if (confused && now >= mark->next_stun_ms && runner->stun_in_window < HF_GAS_MAX_STUN_PER_SECOND)
    {
        result = hf_gas_apply(runner, base, health, status, health_descriptors, health_records,
                              descriptor, raw, index, entity, hp, descriptor_a, descriptor_b,
                              table, status_index, bits, 0);
        if (result == -2)
            return -3;
        if (result < 0)
            return -2;
        if (result == 1)
        {
            mark->next_stun_ms = now + HF_GAS_STUN_COOLDOWN_MS;
            ++runner->stun_in_window;
            ++runner->requested;
        }
    }
    return 0;
}

static void hf_gas_native_tick(HfMod* mod, float dt, void* user)
{
    HfGasRunner* runner = (HfGasRunner*)user;
    HfSession session;
    uintptr_t base, mode, mode_descriptor, health, status, health_descriptors, health_records;
    size_t module_size = 0u;
    uint32_t mode_count, mode_flags, count, capacity;
    uint64_t now = hf_gas_now_ms();
    HfGasStatusTable table;
    char message[256];
    uint32_t visited = 0u, limit, i;
    (void)mod;
    (void)dt;
    if (runner == NULL || runner->disabled || now < runner->next_scan_ms)
        return;
    runner->next_scan_ms = now + HF_GAS_SCAN_MS;
    memset(&session, 0, sizeof(session));
    if (runner->api->session == NULL || runner->api->game_state == NULL ||
        runner->api->apply_status == NULL || runner->api->module_base == NULL || runner->api->read == NULL)
    {
        hf_gas_refuse(runner, "required HellForge SDK 11 session, memory, or status APIs are unavailable");
        return;
    }
    runner->api->session(runner->mod, &session);
    if (session.role != HF_SESSION_SOLO && session.role != HF_SESSION_HOST)
    {
        hf_gas_clear_world(runner);
        hf_gas_log(runner, "Gas Overhaul enemy effects are host-only; waiting for a solo or host mission");
        return;
    }
    if (runner->api->game_state(runner->mod) != HF_STATE_MISSION)
    {
        hf_gas_clear_world(runner);
        hf_gas_log(runner, "Gas Overhaul enemy effects are waiting for a mission");
        return;
    }
    base = runner->api->module_base("game.dll", &module_size);
    if (base == 0u || module_size == 0u || module_size > UINT32_MAX)
    {
        hf_gas_refuse(runner, "game.dll is unavailable while the mission is active");
        return;
    }
    if (base != runner->game_base || (uint32_t)module_size != runner->module_size)
    {
        runner->game_base = base;
        runner->module_size = (uint32_t)module_size;
        runner->profile_checked = 0u;
        memset(runner->definition_rows, 0, sizeof(runner->definition_rows));
    }
    if (!runner->profile_checked && !hf_gas_profile_ready(runner))
        return;
    if (!hf_gas_read_pointer(runner, base + HF_GAS_GAME_MODE_RVA, &mode) ||
        !hf_gas_read_u32(runner, mode + HF_GAS_MODE_COUNT_OFFSET, &mode_count) ||
        mode_count != 1u ||
        !hf_gas_read_pointer(runner, mode + HF_GAS_MODE_DESCRIPTOR_OFFSET, &mode_descriptor) ||
        !hf_gas_read_u32(runner, mode_descriptor + HF_GAS_MODE_FLAGS_OFFSET, &mode_flags) ||
        (mode_flags & 1u) == 0u)
    {
        hf_gas_clear_world(runner);
        hf_gas_log(runner, "Gas Overhaul enemy effects are waiting for a supported host mission mode");
        return;
    }
    if (!hf_gas_read_pointer(runner, base + HF_GAS_HEALTH_MANAGER_RVA, &health) ||
        !hf_gas_read_pointer(runner, base + HF_GAS_STATUS_MANAGER_RVA, &status))
    {
        hf_gas_clear_world(runner);
        hf_gas_log(runner, "Gas Overhaul enemy effects are waiting for live enemy data");
        return;
    }
    if (runner->world_mode != mode || runner->world_descriptor != mode_descriptor ||
        runner->world_health != health || runner->world_status != status ||
        runner->session_version != session.version || runner->session_role != session.role)
    {
        hf_gas_clear_world(runner);
        runner->world_mode = mode;
        runner->world_descriptor = mode_descriptor;
        runner->world_health = health;
        runner->world_status = status;
        runner->session_version = session.version;
        runner->session_role = session.role;
    }
    if (!hf_gas_read_u32(runner, health + HF_GAS_HEALTH_COUNT_OFFSET, &count) ||
        !hf_gas_read_u32(runner, health + HF_GAS_HEALTH_CAPACITY_OFFSET, &capacity) ||
        count > capacity || capacity > HF_GAS_ENTITY_CAPACITY ||
        !hf_gas_read_pointer(runner, health + HF_GAS_HEALTH_DESCRIPTORS_OFFSET, &health_descriptors) ||
        !hf_gas_read_pointer(runner, health + HF_GAS_HEALTH_RECORDS_OFFSET, &health_records))
    {
        hf_gas_refuse(runner, "live health-pool structure is unreadable or outside the accepted bounds");
        return;
    }
    if (runner->world_health_descriptors != health_descriptors || runner->world_health_records != health_records)
    {
        hf_gas_clear_world(runner);
        runner->world_mode = mode;
        runner->world_descriptor = mode_descriptor;
        runner->world_health = health;
        runner->world_status = status;
        runner->world_health_descriptors = health_descriptors;
        runner->world_health_records = health_records;
        runner->session_version = session.version;
        runner->session_role = session.role;
    }
    if (!hf_gas_status_table(runner, status, &table))
    {
        hf_gas_refuse(runner, "live status-pool structure differs from the accepted profile");
        return;
    }
    if (runner->window_start_ms == 0u || now < runner->window_start_ms || now - runner->window_start_ms >= 1000u)
    {
        runner->window_start_ms = now;
        runner->stun_in_window = 0u;
        runner->acid_in_window = 0u;
    }
    limit = count < 64u ? count : 64u;
    while (visited < limit)
    {
        int result;
        uint32_t index;
        if (count == 0u)
            break;
        index = runner->cursor % count;
        runner->cursor = (index + 1u) % count;
        ++visited;
        result = hf_gas_target_tick(runner, base, health, status, health_descriptors,
                                    health_records, &table, index, now);
        if (result == -1)
        {
            hf_gas_refuse(runner, "an enemy descriptor or status bucket could not be read safely");
            return;
        }
        if (result == -2)
        {
            hf_gas_refuse(runner, "HellForge refused a host status action or returned an unexpected result");
            return;
        }
        if (result == -3)
        {
            hf_gas_refuse(runner, "the status request code or status definitions changed during the mission");
            return;
        }
    }
    for (i = 0u; i < HF_GAS_MARK_CAPACITY; ++i)
        if (runner->marks[i].used && now >= runner->marks[i].last_seen_ms &&
            now - runner->marks[i].last_seen_ms > 30000u)
            runner->marks[i].used = 2u;
    runner->active = 1u;
    (void)snprintf(message, sizeof(message),
                   "Gas Overhaul enemy effects ready (host-only; Acid Storm 55/1 every 5s; Medium Stun 38/100 every 6s; %u/%u requested)",
                   runner->acid_requested, runner->requested);
    hf_gas_log(runner, message);
    if (now >= runner->last_log_ms + 15000u)
    {
        runner->last_log_ms = now;
        (void)snprintf(message, sizeof(message),
                       "Gas Overhaul host effects: Acid Storm %u, Medium Stun %u, already-stunned skips %u",
                       runner->acid_requested, runner->requested, runner->skipped_stunned);
        hf_gas_log(runner, message);
    }
}

static void hf_gas_begin_native(const HfApi* api, HfMod* mod)
{
    HfGasRunner* runner = &HF_GAS_RUNNER;
    uint32_t required = (uint32_t)(offsetof(HfApi, apply_status) + sizeof(api->apply_status));
    if (runner->registered || runner->disabled)
        return;
    runner->api = api;
    runner->mod = mod;
    if (api == NULL || api->size < required || api->on_game_thread == NULL || api->module_base == NULL ||
        api->read == NULL || api->session == NULL || api->game_state == NULL || api->apply_status == NULL ||
        api->on_game_thread(mod, hf_gas_native_tick, runner) == 0)
    {
        runner->disabled = 1u;
        if (api != NULL && api->log != NULL)
            api->log(mod, "Refused Gas Overhaul enemy effects: HellForge game-thread or SDK 11 status APIs are unavailable");
        return;
    }
    runner->registered = 1u;
    api->log(mod, "Gas Overhaul host-only enemy effects queued for the game thread; Acid Storm uses the pinned native request, Medium Stun uses apply_status");
}
