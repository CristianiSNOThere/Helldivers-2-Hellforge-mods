#include "component_lookup.h"
#include "vg70_profile.h"
#include "vg70_source_guard.generated.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t HF_VG70_AUTO_RATES[3] = {550u, 1000u, 750u};
static const uint32_t HF_VG70_FIRE_NODES[7] = {
    0x51c7f869u, 0x305559a7u, 0x1f6cba2fu, 0xfa44604fu,
    0x152c80cbu, 0xf5c2ebc6u, 0xb193e9e2u
};
static const uint32_t HF_VG70_CACHED_NODES[7] = {24u, 22u, 21u, 20u, 19u, 18u, 17u};

static uint32_t get_u32(const uint8_t* row, size_t offset)
{
    return (uint32_t)row[offset] | ((uint32_t)row[offset + 1u] << 8) |
           ((uint32_t)row[offset + 2u] << 16) | ((uint32_t)row[offset + 3u] << 24);
}

static void put_u32(uint8_t* row, size_t offset, uint32_t value)
{
    row[offset] = (uint8_t)value;
    row[offset + 1u] = (uint8_t)(value >> 8);
    row[offset + 2u] = (uint8_t)(value >> 16);
    row[offset + 3u] = (uint8_t)(value >> 24);
}

static float get_f32(const uint8_t* row, size_t offset)
{
    float value;
    memcpy(&value, row + offset, sizeof(value));
    return value;
}

static void put_f32(uint8_t* row, size_t offset, float value)
{
    memcpy(row + offset, &value, sizeof(value));
}

static int close_f32(float left, float right)
{
    return isfinite(left) && isfinite(right) && fabsf(left - right) <= 0.0001f;
}

static int supported_mode(uint32_t mode)
{
    return mode == 1u || mode == 4u || mode == 7u;
}

static int supported_rpm(uint32_t rpm)
{
    return rpm == 550u || rpm == 750u || rpm == 1000u;
}

static int supported_current_rpm(uint32_t rpm)
{
    return rpm == 300u || supported_rpm(rpm);
}

static int supported_projectile(uint32_t id)
{
    return id == 94u || id == 333u || id == 335u || id == 336u || id == 337u || id == 338u;
}

static int supported_rate_vector(const uint32_t rates[3])
{
    return rates != NULL && rates[0] == 550u && rates[1] == 1000u && rates[2] == 750u;
}

static int accepted_rate_vector(const uint8_t* row, size_t offsets[3])
{
    const float a = get_f32(row, offsets[0]);
    const float b = get_f32(row, offsets[1]);
    const float c = get_f32(row, offsets[2]);
    const int stock = close_f32(a, 300.0f) && close_f32(b, 550.0f) && close_f32(c, 750.0f);
    const int automatic = close_f32(a, 550.0f) && close_f32(b, 1000.0f) && close_f32(c, 750.0f);
    const int volley = close_f32(a, 550.0f) && close_f32(b, 550.0f) && close_f32(c, 550.0f);
    return stock || automatic || volley;
}

int hf_vg70_weapon_data(const uint8_t* input, size_t size, uint32_t mode, uint8_t* output)
{
    uint32_t burst_count;
    uint32_t target_count;
    size_t i;
    static const size_t recoil_offsets[4] = {0u, 4u, 28u, 32u};
    static const float recoil_base[4] = {5.0f, 10.0f, 4.0f, 8.0f};

    if (input == NULL || output == NULL || size != HF_VG70_WEAPON_SIZE ||
        (mode != 0u && !supported_mode(mode)))
        return 0;
    burst_count = get_u32(input, 140u);
    if ((burst_count != 7u && burst_count != 15u) ||
        !close_f32(get_f32(input, 84u), 5.0f) || !close_f32(get_f32(input, 88u), 5.0f) ||
        (!close_f32(get_f32(input, 104u), 1.0f) && !close_f32(get_f32(input, 104u), 0.9f)) ||
        (!close_f32(get_f32(input, 356u), 25.0f) && !close_f32(get_f32(input, 356u), 40.0f)))
        return 0;

    for (i = 0u; i < 24u; ++i)
    {
        const uint32_t actual = get_u32(input, 204u + i * 4u);
        const uint32_t known = i < 7u ? HF_VG70_FIRE_NODES[i] : 0u;
        const uint32_t repeated = i < 15u ? HF_VG70_FIRE_NODES[i % 7u] : 0u;
        if (actual != known && actual != repeated)
            return 0;
    }
    for (i = 0u; i < 4u; ++i)
    {
        const float actual = get_f32(input, recoil_offsets[i]);
        if (!close_f32(actual, recoil_base[i]) && !close_f32(actual, recoil_base[i] * 0.85f))
            return 0;
    }

    target_count = mode == 4u ? 15u : 7u;
    memcpy(output, input, size);
    put_u32(output, 140u, target_count);
    for (i = 0u; i < 24u; ++i)
        put_u32(output, 204u + i * 4u, i < target_count ? HF_VG70_FIRE_NODES[i % 7u] : 0u);
    for (i = 0u; i < 4u; ++i)
        put_f32(output, recoil_offsets[i], recoil_base[i] * 0.85f);
    put_f32(output, 104u, 0.9f);
    put_f32(output, 356u, 40.0f);
    return 1;
}

int hf_vg70_projectile_weapon(const uint8_t* input, size_t size, uint32_t projectile,
                              const uint32_t rates[3], uint8_t* output)
{
    static size_t offsets[3] = {4u, 8u, 12u};
    if (input == NULL || output == NULL || size != HF_VG70_PROJECTILE_SIZE ||
        !supported_projectile(get_u32(input, 0u)) || !supported_projectile(projectile) ||
        !accepted_rate_vector(input, offsets) || !supported_rate_vector(rates))
        return 0;
    memcpy(output, input, size);
    put_u32(output, 0u, projectile);
    put_f32(output, 4u, (float)rates[0]);
    put_f32(output, 8u, (float)rates[1]);
    put_f32(output, 12u, (float)rates[2]);
    return 1;
}

int hf_vg70_magazine(const uint8_t* input, size_t size, uint8_t* output)
{
    if (input == NULL || output == NULL || size != HF_VG70_MAGAZINE_SIZE ||
        (get_u32(input, 136u) != 49u && get_u32(input, 136u) != 150u) ||
        get_u32(input, 140u) != 6u || get_u32(input, 144u) != 7u || get_u32(input, 148u) != 7u)
        return 0;
    memcpy(output, input, size);
    put_u32(output, 136u, 150u);
    return 1;
}

int hf_vg70_weapon_state(const uint8_t* input, size_t size, uint32_t mode, uint8_t* output)
{
    uint32_t count;
    uint32_t counter;
    uint32_t target_count;
    float mx, my, x, y;
    size_t i;
    static const size_t node_offsets[4] = {0x1cu, 0x20u, 0x38u, 0x3cu};
    static const size_t modifier_a[4] = {0x3b0u, 0x3b8u, 0x3b0u, 0x3b8u};
    static const size_t modifier_b[4] = {0x3c0u, 0x3c8u, 0x3d0u, 0x3d8u};
    static const float recoil_base[4] = {5.0f, 10.0f, 4.0f, 8.0f};

    if (input == NULL || output == NULL || size != HF_VG70_WEAPON_STATE_SIZE || !supported_mode(mode))
        return 0;
    count = get_u32(input, 0xd8u);
    counter = get_u32(input, 0xdcu);
    mx = get_f32(input, 0x3e0u);
    my = get_f32(input, 0x3e8u);
    x = get_f32(input, 0x58u);
    y = get_f32(input, 0x5cu);
    if ((count != 7u && count != 15u) || counter >= count || !isfinite(mx) || !isfinite(my) ||
        mx <= 0.0f || mx > 100.0f || my <= 0.0f || my > 100.0f ||
        (!close_f32(x, 5.0f * mx) && !close_f32(x, 0.75f)) ||
        (!close_f32(y, 5.0f * my) && !close_f32(y, 0.75f)))
        return 0;
    for (i = 0u; i < 24u; ++i)
    {
        const uint32_t actual = get_u32(input, 0x78u + i * 4u);
        const uint32_t known = i < 7u ? HF_VG70_CACHED_NODES[i] : 0u;
        const uint32_t repeated = i < 15u ? HF_VG70_CACHED_NODES[i % 7u] : 0u;
        if (actual != known && actual != repeated)
            return 0;
    }
    for (i = 0u; i < 4u; ++i)
    {
        const float first = get_f32(input, modifier_a[i]);
        const float second = get_f32(input, modifier_b[i]);
        const float actual = get_f32(input, node_offsets[i]);
        const float expected = recoil_base[i] * first * second;
        if (!isfinite(first) || !isfinite(second) || first < 0.0f || first > 10.0f ||
            second < 0.0f || second > 10.0f ||
            (!close_f32(actual, expected) && !close_f32(actual, expected * 0.85f)))
            return 0;
    }

    target_count = mode == 4u ? 15u : 7u;
    memcpy(output, input, size);
    for (i = 0u; i < 24u; ++i)
        put_u32(output, 0x78u + i * 4u, i < target_count ? HF_VG70_CACHED_NODES[i % 7u] : 0u);
    for (i = 0u; i < 4u; ++i)
    {
        const float expected = recoil_base[i] * get_f32(input, modifier_a[i]) * get_f32(input, modifier_b[i]);
        put_f32(output, node_offsets[i], expected * 0.85f);
    }
    put_u32(output, 0xd8u, target_count);
    put_u32(output, 0xdcu, counter % target_count);
    put_f32(output, 0x58u, mode == 7u ? 0.75f : 5.0f * mx);
    put_f32(output, 0x5cu, mode == 7u ? 0.75f : 5.0f * my);
    return 1;
}

int hf_vg70_rate_state(const uint8_t* input, size_t size, uint32_t slot,
                       const uint32_t rates[3], uint8_t* output)
{
    static size_t offsets[3] = {16u, 20u, 24u};
    uint32_t current_slot;
    if (input == NULL || output == NULL || size != HF_VG70_RATE_STATE_SIZE || slot > 2u)
        return 0;
    current_slot = get_u32(input, 28u);
    if (
        current_slot > 2u || !accepted_rate_vector(input, offsets) || !supported_rate_vector(rates))
        return 0;
    memcpy(output, input, size);
    put_f32(output, 16u, (float)rates[0]);
    put_f32(output, 20u, (float)rates[1]);
    put_f32(output, 24u, (float)rates[2]);
    put_u32(output, 28u, slot);
    return 1;
}

int hf_vg70_current_rpm_state(const uint8_t* input, size_t size, uint32_t rpm, uint8_t* output)
{
    float current;
    if (input == NULL || output == NULL || size != HF_VG70_CURRENT_RPM_SIZE || !supported_rpm(rpm))
        return 0;
    current = get_f32(input, 4u);
    if (!close_f32(current, 300.0f) && !close_f32(current, 550.0f) &&
        !close_f32(current, 750.0f) && !close_f32(current, 1000.0f))
        return 0;
    memcpy(output, input, size);
    put_f32(output, 4u, (float)rpm);
    return 1;
}

int hf_vg70_cadence(const uint8_t* input, size_t size, uint32_t current_rpm,
                    uint32_t target_rpm, uint8_t* output, float* multiplier)
{
    float interval;
    float ratio;
    if (input == NULL || output == NULL || multiplier == NULL || size != HF_VG70_PROJECTILE_STATE_SIZE ||
        (current_rpm != 300u && !supported_rpm(current_rpm)) || !supported_rpm(target_rpm))
        return 0;
    interval = get_f32(input, 12u);
    if (!isfinite(interval) || interval <= 0.0f || interval >= 2.0f)
        return 0;
    ratio = 60.0f / ((float)current_rpm * interval);
    if (!isfinite(ratio) || ratio < 0.5f || ratio > 2.0f)
        return 0;
    memcpy(output, input, size);
    put_f32(output, 12u, 60.0f / ((float)target_rpm * ratio));
    *multiplier = ratio;
    return 1;
}

static int accepted_current_vector(const uint32_t current[3])
{
    return current != NULL &&
        ((current[0] == 300u && current[1] == 550u && current[2] == 750u) ||
         (current[0] == 550u && current[1] == 1000u && current[2] == 750u));
}

static uint32_t profile_for(uint32_t mode, uint32_t rpm)
{
    if (mode == 4u)
        return 337u;
    if (mode == 7u)
        return 338u;
    if (rpm == 550u)
        return 333u;
    if (rpm == 750u)
        return 335u;
    return 336u;
}

int hf_vg70_mode_step(HfVg70ModeMemory* memory, uint64_t entity_id,
                      uintptr_t entity_identity, uint32_t mode, uint32_t selected_slot,
                      uint32_t current_rpm, const uint32_t current_vector[3],
                      int force_default, HfVg70Selection* output)
{
    uint32_t rpm;
    if (memory == NULL || output == NULL || entity_id == 0u || entity_identity == 0u ||
        !supported_mode(mode) || selected_slot > 2u || !supported_current_rpm(current_rpm) ||
        !accepted_current_vector(current_vector) || current_vector[selected_slot] != current_rpm)
        return 0;
    if (memory->active && (memory->entity_id != entity_id || memory->entity_identity != entity_identity))
        return 0;
    rpm = force_default || current_rpm == 300u ? 1000u : current_rpm;
    if (!supported_rpm(rpm))
        return 0;
    memory->entity_id = entity_id;
    memory->entity_identity = entity_identity;
    memory->auto_rpm = rpm;
    memory->mode = mode;
    memory->active = 1u;
    output->rpm = rpm;
    output->slot = rpm == 550u ? 0u : rpm == 1000u ? 1u : 2u;
    output->projectile = profile_for(mode, rpm);
    memcpy(output->rates, HF_VG70_AUTO_RATES, sizeof(output->rates));
    return 1;
}

typedef struct HfVg70PreparedTable
{
    const HfVg70TableSpec* spec;
    uintptr_t root_slot, root, group, rows;
    uint8_t root_slot_bytes[8], header[24], descriptor[16];
    uint8_t* before_all;
    uint8_t* after_all;
    uintptr_t registry_slots[8], registry_values[8];
    uint32_t registry_count;
} HfVg70PreparedTable;

typedef struct HfVg70WordEdit
{
    uintptr_t address;
    uint8_t before[4], after[4];
} HfVg70WordEdit;

#define HF_VG70_MAX_SHARED_EDITS 512u
#define HF_VG70_MAX_REGISTRY_ROWS 8u

static uint64_t vg70_u64(const uint8_t bytes[8])
{
    uint64_t value = 0u;
    size_t i;
    for (i = 0u; i < 8u; ++i)
        value |= (uint64_t)bytes[i] << (i * 8u);
    return value;
}

static int vg70_canonical(uintptr_t address)
{
    return address >= (uintptr_t)65536u && address < (uintptr_t)UINT64_C(0x800000000000);
}

static int vg70_address(uintptr_t base, uint64_t offset, uintptr_t* output)
{
    if (output == NULL || offset > (uint64_t)(UINTPTR_MAX - base))
        return 0;
    *output = base + (uintptr_t)offset;
    return vg70_canonical(*output);
}

static void vg70_set_reason(char* reason, size_t capacity, const char* message)
{
    if (reason != NULL && capacity > 0u)
        (void)snprintf(reason, capacity, "%s", message != NULL ? message : "VG-70 profile refused");
}

static int vg70_read(const HfVg70MemoryOps* ops, uintptr_t address, void* output, size_t size)
{
    return ops != NULL && ops->read != NULL && vg70_canonical(address) &&
           size > 0u && address <= UINTPTR_MAX - size &&
           ops->read(ops->context, address, output, size) == 0;
}

static int vg70_read_pointer(const HfVg70MemoryOps* ops, uintptr_t address, uintptr_t* output)
{
    uint8_t bytes[8];
    uint64_t value;
    if (output == NULL || sizeof(uintptr_t) != 8u || !vg70_read(ops, address, bytes, sizeof(bytes)))
        return 0;
    value = vg70_u64(bytes);
    if (value > (uint64_t)UINTPTR_MAX || !vg70_canonical((uintptr_t)value))
        return 0;
    *output = (uintptr_t)value;
    return 1;
}

static int vg70_read_nullable_pointer(const HfVg70MemoryOps* ops, uintptr_t address, uintptr_t* output)
{
    uint8_t bytes[8];
    uint64_t value;
    if (output == NULL || sizeof(uintptr_t) != 8u || !vg70_read(ops, address, bytes, sizeof(bytes)))
        return 0;
    value = vg70_u64(bytes);
    if (value == 0u)
    {
        *output = 0u;
        return 1;
    }
    if (value > (uint64_t)UINTPTR_MAX || !vg70_canonical((uintptr_t)value))
        return 0;
    *output = (uintptr_t)value;
    return 1;
}

static int vg70_data_access(const HfVg70MemoryOps* ops, uintptr_t address, size_t size)
{
    return ops != NULL && ops->data_access != NULL && vg70_canonical(address) &&
           size > 0u && address <= UINTPTR_MAX - size &&
           ops->data_access(ops->context, address, size) != 0;
}

static void vg70_prepared_free(HfVg70PreparedTable* table)
{
    if (table == NULL)
        return;
    free(table->before_all);
    free(table->after_all);
    table->before_all = NULL;
    table->after_all = NULL;
}

static int vg70_record_row(const HfVg70MemoryOps* ops, uintptr_t game,
                           HfVg70PreparedTable* prepared, const HfVg70DataRecord* record,
                           HfVg70WordEdit* edits, size_t* edit_count,
                           char* reason, size_t capacity)
{
    uint8_t normalized_before[272], normalized_after[272];
    const HfVg70TableSpec* table = prepared->spec;
    const size_t offset = (size_t)record->index * (size_t)table->stride;
    const uint8_t* actual;
    uintptr_t row_address;
    uintptr_t registry_slot;
    uintptr_t lookup;
    uint8_t lookup_bytes[8];
    uint32_t query = 0u;
    size_t i;
    int is_before;
    int is_after;

    if (record->index >= table->count || record->size != table->stride ||
        offset > (size_t)table->count * table->stride ||
        record->size > (size_t)table->count * table->stride - offset ||
        !vg70_address(prepared->rows, (uint64_t)offset, &row_address) ||
        !vg70_data_access(ops, row_address, record->size))
    {
        vg70_set_reason(reason, capacity, "VG-70 target profile row is out of bounds or inaccessible");
        return 0;
    }
    actual = prepared->before_all + offset;
    if (get_u32(actual, 0u) != record->id)
    {
        vg70_set_reason(reason, capacity, "VG-70 profile row identity or index changed");
        return 0;
    }
    if (!vg70_address(game, (uint64_t)table->registry + (uint64_t)record->id * 8u, &registry_slot) ||
        !vg70_read(ops, registry_slot, lookup_bytes, sizeof(lookup_bytes)))
    {
        vg70_set_reason(reason, capacity, "VG-70 native lookup slot is unavailable");
        return 0;
    }
    lookup = (uintptr_t)vg70_u64(lookup_bytes);
    if (lookup != row_address)
    {
        vg70_set_reason(reason, capacity, "VG-70 native lookup is redirected or owned by another mod");
        return 0;
    }
    if (prepared->registry_count >= HF_VG70_MAX_REGISTRY_ROWS)
    {
        vg70_set_reason(reason, capacity, "VG-70 profile registry guard capacity exceeded");
        return 0;
    }
    prepared->registry_slots[prepared->registry_count] = registry_slot;
    prepared->registry_values[prepared->registry_count] = row_address;
    ++prepared->registry_count;

    memcpy(normalized_before, record->before, record->size);
    memcpy(normalized_after, record->after, record->size);
    if (record->foliage_query != 0u)
    {
        query = get_u32(actual, 244u);
        if (query != 0u && query != 2u)
        {
            vg70_set_reason(reason, capacity, "VG-70 foliage-query compatibility value is unsupported");
            return 0;
        }
        put_u32(normalized_before, 244u, query);
        put_u32(normalized_after, 244u, query);
    }
    is_before = memcmp(actual, normalized_before, record->size) == 0;
    is_after = memcmp(actual, normalized_after, record->size) == 0;
    if (!is_before && !is_after)
    {
        vg70_set_reason(reason, capacity, "VG-70 shared profile row differs from accepted base and target");
        return 0;
    }

    memcpy(prepared->after_all + offset, normalized_after, record->size);
    if (!is_before)
        return 1;
    for (i = 0u; i < record->size; i += 4u)
    {
        HfVg70WordEdit* edit;
        if (memcmp(normalized_before + i, normalized_after + i, 4u) == 0)
            continue;
        if (*edit_count >= HF_VG70_MAX_SHARED_EDITS ||
            !vg70_address(row_address, (uint64_t)i, &row_address) ||
            !vg70_data_access(ops, row_address, 4u))
        {
            vg70_set_reason(reason, capacity, "VG-70 shared profile edit limit or access guard failed");
            return 0;
        }
        edit = &edits[*edit_count];
        edit->address = row_address;
        memcpy(edit->before, normalized_before + i, 4u);
        memcpy(edit->after, normalized_after + i, 4u);
        ++*edit_count;
        /* The row base is needed for the next word. */
        row_address -= (uintptr_t)i;
    }
    return 1;
}

static int vg70_prepare_table(const HfVg70MemoryOps* ops, uintptr_t game,
                              const HfVg70TableSpec* spec, HfVg70PreparedTable* prepared,
                              HfVg70WordEdit* edits, size_t* edit_count,
                              char* reason, size_t capacity)
{
    uint8_t root_header[8];
    uint8_t group_header[24];
    uint8_t descriptor[16];
    uintptr_t group;
    uintptr_t descriptor_address;
    uintptr_t described_rows;
    size_t bytes;
    uint32_t i, j;

    memset(prepared, 0, sizeof(*prepared));
    prepared->spec = spec;
    if (spec->stride == 0u || spec->count == 0u ||
        (size_t)spec->count > SIZE_MAX / (size_t)spec->stride ||
        !vg70_address(game, spec->slot, &prepared->root_slot) ||
        !vg70_read(ops, prepared->root_slot, prepared->root_slot_bytes, sizeof(prepared->root_slot_bytes)))
    {
        vg70_set_reason(reason, capacity, "VG-70 settings table root slot is unavailable");
        return 0;
    }
    if (!vg70_read_pointer(ops, prepared->root_slot, &prepared->root) ||
        !vg70_read(ops, prepared->root, root_header, sizeof(root_header)))
    {
        vg70_set_reason(reason, capacity, "VG-70 settings table root pointer or header is invalid");
        return 0;
    }
    if (get_u32(root_header, 0u) != spec->groups ||
        !vg70_address(prepared->root, (uint64_t)(uint32_t)spec->group, &group) ||
        !vg70_read(ops, group, group_header, sizeof(group_header)))
    {
        vg70_set_reason(reason, capacity, "VG-70 settings table group count or group address changed");
        return 0;
    }
    if (memcmp(group_header, "LDLD", 4u) != 0 || get_u32(group_header, 4u) != 1u ||
        get_u32(group_header, 8u) != spec->kind || get_u32(group_header, 12u) != spec->size ||
        !vg70_address(group, 24u, &descriptor_address) ||
        !vg70_read(ops, descriptor_address, descriptor, sizeof(descriptor)))
    {
        vg70_set_reason(reason, capacity, "VG-70 settings table group header or descriptor changed");
        return 0;
    }
    if (get_u32(descriptor, 8u) != spec->count || get_u32(descriptor, 12u) != 0u ||
        !vg70_read_pointer(ops, descriptor_address, &described_rows) ||
        described_rows != descriptor_address + 16u)
    {
        vg70_set_reason(reason, capacity, "VG-70 settings table rows descriptor changed");
        return 0;
    }
    prepared->group = group;
    prepared->rows = described_rows;
    memcpy(prepared->header, group_header, sizeof(prepared->header));
    memcpy(prepared->descriptor, descriptor, sizeof(prepared->descriptor));
    bytes = (size_t)spec->count * (size_t)spec->stride;
    if (bytes > UINT32_MAX || !vg70_canonical(prepared->rows) ||
        prepared->rows > UINTPTR_MAX - bytes || !vg70_data_access(ops, prepared->rows, bytes))
    {
        vg70_set_reason(reason, capacity, "VG-70 settings table rows are inaccessible or out of bounds");
        return 0;
    }
    prepared->before_all = (uint8_t*)malloc(bytes);
    prepared->after_all = (uint8_t*)malloc(bytes);
    if (prepared->before_all == NULL || prepared->after_all == NULL ||
        !vg70_read(ops, prepared->rows, prepared->before_all, bytes))
    {
        vg70_set_reason(reason, capacity, "VG-70 settings table rows could not be snapshotted");
        return 0;
    }
    memcpy(prepared->after_all, prepared->before_all, bytes);
    for (i = 0u; i < spec->count; ++i)
    {
        const uint32_t id = get_u32(prepared->before_all, (size_t)i * spec->stride);
        for (j = 0u; j < i; ++j)
        {
            if (get_u32(prepared->before_all, (size_t)j * spec->stride) == id)
            {
                vg70_set_reason(reason, capacity, "VG-70 settings table contains duplicate row identities");
                return 0;
            }
        }
    }
    for (i = 0u; i < spec->record_count; ++i)
    {
        if (!vg70_record_row(ops, game, prepared, &spec->records[i], edits, edit_count,
                             reason, capacity))
            return 0;
    }
    return 1;
}

static int vg70_validate_table(const HfVg70MemoryOps* ops, const HfVg70PreparedTable* table,
                               int after)
{
    uint8_t root_slot[8], header[24], descriptor[16];
    const size_t size = (size_t)table->spec->count * (size_t)table->spec->stride;
    const uint8_t* expected = after ? table->after_all : table->before_all;
    uint8_t* actual_rows;
    uint32_t i;
    if (!vg70_read(ops, table->root_slot, root_slot, sizeof(root_slot)) ||
        memcmp(root_slot, table->root_slot_bytes, sizeof(root_slot)) != 0 ||
        !vg70_read(ops, table->group, header, sizeof(header)) ||
        memcmp(header, table->header, sizeof(header)) != 0 ||
        !vg70_read(ops, table->group + 24u, descriptor, sizeof(descriptor)) ||
        memcmp(descriptor, table->descriptor, sizeof(descriptor)) != 0)
        return 0;
    actual_rows = (uint8_t*)malloc(size);
    if (actual_rows == NULL || !vg70_read(ops, table->rows, actual_rows, size))
    {
        free(actual_rows);
        return 0;
    }
    if (memcmp(actual_rows, expected, size) != 0)
    {
        free(actual_rows);
        return 0;
    }
    free(actual_rows);
    for (i = 0u; i < table->registry_count; ++i)
    {
        uintptr_t actual;
        if (!vg70_read_pointer(ops, table->registry_slots[i], &actual) ||
            actual != table->registry_values[i])
            return 0;
    }
    return 1;
}

static int vg70_rollback(const HfVg70MemoryOps* ops, const HfVg70WordEdit* edits,
                         size_t attempted)
{
    uint8_t actual[4];
    int failed = 0;
    while (attempted > 0u)
    {
        const HfVg70WordEdit* edit = &edits[--attempted];
        if (!vg70_read(ops, edit->address, actual, sizeof(actual)))
        {
            failed = 1;
            continue;
        }
        if (memcmp(actual, edit->after, sizeof(actual)) == 0)
        {
            if (ops->write(ops->context, edit->address, edit->before, sizeof(edit->before)) != 0 ||
                !vg70_read(ops, edit->address, actual, sizeof(actual)) ||
                memcmp(actual, edit->before, sizeof(actual)) != 0)
                failed = 1;
        }
        else if (memcmp(actual, edit->before, sizeof(actual)) != 0)
            failed = 1;
    }
    return !failed;
}

static int vg70_stage_word(const HfVg70MemoryOps* ops, HfVg70WordEdit* edits,
                           size_t edit_capacity, size_t* edit_count, uintptr_t address,
                           const uint8_t after[4], char* reason, size_t reason_capacity);

static int vg70_component_owners_current(const HfVg70MemoryOps*, const HfVg70NativePlan*);

static HfVg70ApplyResult vg70_apply_profiles(const HfVg70MemoryOps* ops, uintptr_t game,
                                              const HfVg70NativePlan* component_plan,
                                              uint32_t* changed_fields,
                                              char* reason, size_t reason_capacity)
{
    HfVg70PreparedTable prepared[2];
    HfVg70WordEdit edits[HF_VG70_MAX_SHARED_EDITS];
    uint8_t component_before[HF_VG70_COMPONENT_COUNT][HF_VG70_WEAPON_SIZE];
    uint8_t component_after[HF_VG70_COMPONENT_COUNT][HF_VG70_WEAPON_SIZE];
    uint8_t component_actual[HF_VG70_COMPONENT_COUNT][HF_VG70_WEAPON_SIZE];
    uintptr_t component_addresses[HF_VG70_COMPONENT_COUNT];
    const size_t component_sizes[HF_VG70_COMPONENT_COUNT] = {
        HF_VG70_WEAPON_SIZE, HF_VG70_PROJECTILE_SIZE, HF_VG70_MAGAZINE_SIZE
    };
    uint8_t actual[4];
    uint32_t rates[3] = {550u, 1000u, 750u};
    size_t edit_count = 0u;
    size_t attempted = 0u;
    size_t i;
    int rollback_ok;
    HfVg70ApplyResult result = HF_VG70_REFUSED;

    memset(prepared, 0, sizeof(prepared));
    if (changed_fields != NULL)
        *changed_fields = 0u;
    if (ops == NULL || ops->read == NULL || ops->write == NULL || ops->data_access == NULL ||
        !vg70_canonical(game) || HF_VG70_TABLE_COUNT != 2u)
    {
        vg70_set_reason(reason, reason_capacity, "HellForge memory API or VG-70 table profile is unavailable");
        return HF_VG70_REFUSED;
    }
    for (i = 0u; i < HF_VG70_TABLE_COUNT; ++i)
    {
        if (!vg70_prepare_table(ops, game, &HF_VG70_TABLES[i], &prepared[i], edits,
                                &edit_count, reason, reason_capacity))
            goto done;
    }
    if (component_plan != NULL)
    {
        if (!vg70_component_owners_current(ops,component_plan)) {
            result=attempted?HF_VG70_FATAL:HF_VG70_REFUSED;
            vg70_set_reason(reason,reason_capacity,attempted?
                "FATAL: VG-70 active component ownership changed during commit":"VG-70 active component ownership changed");goto done;
        }
        component_addresses[0] = component_plan->weapon_address;
        component_addresses[1] = component_plan->projectile_address;
        component_addresses[2] = component_plan->magazine_address;
        for (i = 0u; i < HF_VG70_COMPONENT_COUNT; ++i)
        {
            const size_t size = component_sizes[i];
            size_t offset;
            if (!vg70_data_access(ops, component_addresses[i], size) ||
                !vg70_read(ops, component_addresses[i], component_before[i], size) ||
                (memcmp(component_before[i], HF_VG70_COMPONENTS[i].before, size) != 0 &&
                 memcmp(component_before[i], HF_VG70_COMPONENTS[i].after, size) != 0))
            {
                vg70_set_reason(reason, reason_capacity,
                                "VG-70 component source row changed before the global profile transaction");
                goto done;
            }
            if (i == 0u)
            {
                if (!hf_vg70_weapon_data(component_before[i], size, 0u, component_after[i]))
                    goto component_refused;
            }
            else if (i == 1u)
            {
                if (!hf_vg70_projectile_weapon(component_before[i], size, 336u, rates, component_after[i]))
                    goto component_refused;
            }
            else if (!hf_vg70_magazine(component_before[i], size, component_after[i]))
                goto component_refused;
            for (offset = 0u; offset < size; offset += 4u)
            {
                uintptr_t field;
                if (memcmp(component_before[i] + offset, component_after[i] + offset, 4u) == 0)
                    continue;
                if (!vg70_address(component_addresses[i], offset, &field) ||
                    !vg70_stage_word(ops, edits, HF_VG70_MAX_SHARED_EDITS, &edit_count,
                                     field, component_after[i] + offset, reason, reason_capacity))
                    goto done;
            }
        }
    }
    for (i = 0u; i < HF_VG70_TABLE_COUNT; ++i)
    {
        if (!vg70_validate_table(ops, &prepared[i], 0))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 table snapshot changed before transaction");
            goto done;
        }
    }
    if (component_plan != NULL)
    {
        if (!vg70_component_owners_current(ops,component_plan)) {
            result=attempted?HF_VG70_FATAL:HF_VG70_REFUSED;
            vg70_set_reason(reason,reason_capacity,attempted?
                "FATAL: VG-70 active component ownership changed during commit":"VG-70 active component ownership changed");goto done;
        }
        for (i = 0u; i < HF_VG70_COMPONENT_COUNT; ++i)
        {
            const size_t size = component_sizes[i];
            if (!vg70_data_access(ops, component_addresses[i], size) ||
                !vg70_read(ops, component_addresses[i], component_actual[i], size) ||
                memcmp(component_actual[i], component_before[i], size) != 0)
            {
                vg70_set_reason(reason, reason_capacity,
                                "VG-70 component source row changed before the global profile commit");
                goto done;
            }
        }
    }
    for (i = 0u; i < edit_count; ++i)
    {
        if (!vg70_data_access(ops, edits[i].address, sizeof(edits[i].before)) ||
            !vg70_read(ops, edits[i].address, actual, sizeof(actual)) ||
            memcmp(actual, edits[i].before, sizeof(actual)) != 0)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 shared profile field changed before commit");
            goto done;
        }
    }
    for (i = 0u; i < edit_count; ++i)
    {
        if (component_plan && !vg70_component_owners_current(ops,component_plan)) {
            result=attempted?HF_VG70_FATAL:HF_VG70_REFUSED;
            vg70_set_reason(reason,reason_capacity,attempted?
                "FATAL: VG-70 active component ownership changed during commit":"VG-70 active component ownership changed");goto done;
        }
        attempted = i + 1u;
        if (ops->write(ops->context, edits[i].address, edits[i].after, sizeof(edits[i].after)) != 0 ||
            !vg70_read(ops, edits[i].address, actual, sizeof(actual)) ||
            memcmp(actual, edits[i].after, sizeof(actual)) != 0)
        {
            rollback_ok = vg70_rollback(ops, edits, attempted);
            vg70_set_reason(reason, reason_capacity, rollback_ok ?
                            "VG-70 shared profile write failed; prior fields were rolled back" :
                            "FATAL: VG-70 shared profile write failed and rollback was incomplete");
            result = rollback_ok ? HF_VG70_REFUSED : HF_VG70_FATAL;
            goto done;
        }
    }
    if (component_plan != NULL)
    {
        if (!vg70_component_owners_current(ops,component_plan)) {
            result=attempted?HF_VG70_FATAL:HF_VG70_REFUSED;
            vg70_set_reason(reason,reason_capacity,attempted?
                "FATAL: VG-70 active component ownership changed during commit":"VG-70 active component ownership changed");goto done;
        }
        for (i = 0u; i < HF_VG70_COMPONENT_COUNT; ++i)
        {
            const size_t size = component_sizes[i];
            if (!vg70_read(ops, component_addresses[i], component_actual[i], size) ||
                memcmp(component_actual[i], component_after[i], size) != 0)
            {
                rollback_ok = vg70_rollback(ops, edits, attempted);
                vg70_set_reason(reason, reason_capacity, rollback_ok ?
                    "VG-70 component readback failed; global profile changes were rolled back" :
                    "FATAL: VG-70 component readback failed and rollback was incomplete");
                result = rollback_ok ? HF_VG70_REFUSED : HF_VG70_FATAL;
                goto done;
            }
        }
    }
    for (i = 0u; i < HF_VG70_TABLE_COUNT; ++i)
    {
        if (!vg70_validate_table(ops, &prepared[i], 1))
        {
            rollback_ok = vg70_rollback(ops, edits, attempted);
            vg70_set_reason(reason, reason_capacity, rollback_ok ?
                            "VG-70 full-table verification failed; profile fields were rolled back" :
                            "FATAL: VG-70 full-table verification failed and rollback was incomplete");
            result = rollback_ok ? HF_VG70_REFUSED : HF_VG70_FATAL;
            goto done;
        }
    }
    if (component_plan != NULL)
    {
        if (!vg70_component_owners_current(ops,component_plan)) {
            result=attempted?HF_VG70_FATAL:HF_VG70_REFUSED;
            vg70_set_reason(reason,reason_capacity,attempted?
                "FATAL: VG-70 active component ownership changed during commit":"VG-70 active component ownership changed");goto done;
        }
        for (i = 0u; i < HF_VG70_COMPONENT_COUNT; ++i)
        {
            const size_t size = component_sizes[i];
            if (!vg70_read(ops, component_addresses[i], component_actual[i], size) ||
                memcmp(component_actual[i], component_after[i], size) != 0)
                goto component_verify_failed;
        }
    }
    if (changed_fields != NULL)
        *changed_fields = (uint32_t)edit_count;
    if (reason != NULL && reason_capacity > 0u)
        (void)snprintf(reason, reason_capacity,
                       "Applied guarded VG-70 settings and component profiles (%u fields)",
                       (unsigned)edit_count);
    result = HF_VG70_APPLIED;

done:
    for (i = 0u; i < HF_VG70_TABLE_COUNT; ++i)
        vg70_prepared_free(&prepared[i]);
    return result;

component_refused:
    vg70_set_reason(reason, reason_capacity, "VG-70 component source data differs from the accepted profile");
    goto done;

component_verify_failed:
    rollback_ok = vg70_rollback(ops, edits, attempted);
    vg70_set_reason(reason, reason_capacity, rollback_ok ?
        "VG-70 component verification failed; global profile changes were rolled back" :
        "FATAL: VG-70 component verification failed and rollback was incomplete");
    result = rollback_ok ? HF_VG70_REFUSED : HF_VG70_FATAL;
    goto done;
}

HfVg70ApplyResult hf_vg70_apply_shared_profiles(const HfVg70MemoryOps* ops, uintptr_t game,
                                                 uint32_t* changed_fields,
                                                 char* reason, size_t reason_capacity)
{
    return vg70_apply_profiles(ops, game, NULL, changed_fields, reason, reason_capacity);
}

HfVg70ApplyResult hf_vg70_apply_all_profiles(const HfVg70MemoryOps* ops, uintptr_t game,
                                              HfVg70NativePlan* plan,
                                              uint32_t* changed_fields,
                                              char* reason, size_t reason_capacity)
{
    if (!hf_vg70_native_prepare(ops, game, plan, reason, reason_capacity))
        return HF_VG70_REFUSED;
    return vg70_apply_profiles(ops, game, plan, changed_fields, reason, reason_capacity);
}

#define HF_VG70_OWNER_RESOURCE UINT64_C(0x7F8A7C5797CE92F9)
#define HF_VG70_OWNER_SLOT UINT64_C(0x346BF98)
#define HF_VG70_OWNER_WEAPON_MAP UINT64_C(0xF12BD8)
#define HF_VG70_OWNER_PROJECTILE_MAP UINT64_C(0xF12E80)
#define HF_VG70_WEAPON_MANAGER_SLOT UINT64_C(0x3326CE0)
#define HF_VG70_PROJECTILE_MANAGER_SLOT UINT64_C(0x33266D8)
#define HF_VG70_MAGAZINE_MANAGER_SLOT UINT64_C(0x3326648)

static inline int vg70_add_signed(uintptr_t base, int32_t offset, uintptr_t* output)
{
    if (offset < 0)
    {
        const uint32_t back = (uint32_t)(-(int64_t)offset);
        if (base < back)
            return 0;
        *output = base - back;
        return vg70_canonical(*output);
    }
    return vg70_address(base, (uint32_t)offset, output);
}

static int vg70_map_lookup(const HfVg70MemoryOps* ops, uintptr_t manager, uint32_t map_offset,
                           uint32_t key, int* found, uint32_t* value)
{
    uint8_t header[20];
    uintptr_t array;
    uint32_t capacity, empty, multiplier, probe;
    uint64_t product;
    if (found == NULL || value == NULL ||
        !vg70_address(manager, map_offset, &array) ||
        !vg70_read(ops, array, header, sizeof(header)))
        return 0;
    capacity = get_u32(header, 8u);
    empty = get_u32(header, 12u);
    multiplier = get_u32(header, 16u);
    *found = 0;
    *value = 0u;
    if (capacity == 0u)
        return 1;
    if (capacity > 4096u || (capacity & (capacity - 1u)) != 0u)
        return 0;
    if (!vg70_read_pointer(ops, array, &array))
        return 0;
    product = (uint64_t)(key & 0xFFFFu) * multiplier +
              (uint64_t)(key >> 16) * (uint64_t)(multiplier & 0xFFFFu) * 65536u;
    for (probe = 0u; probe < capacity && probe < 64u; ++probe)
    {
        uint8_t row[8];
        uintptr_t address;
        const uint32_t slot = ((uint32_t)product + probe) & (capacity - 1u);
        if (!vg70_address(array, (uint64_t)slot * 8u, &address) ||
            !vg70_read(ops, address, row, sizeof(row)))
            return 0;
        if (get_u32(row, 0u) == key)
        {
            const uint32_t index = get_u32(row, 4u);
            if (index != UINT32_MAX)
            {
                *found = 1;
                *value = index;
            }
            return 1;
        }
        if (get_u32(row, 0u) == empty)
            return 1;
    }
    if (capacity > 64u)
        return 0;
    return 1;
}

static int vg70_component_prepare(const HfVg70MemoryOps* ops, uintptr_t root,
                                  const HfVg70ComponentSpec* spec, uintptr_t* row,
                                  char* reason, size_t capacity)
{
    uintptr_t address;
    uint8_t source[HF_VG70_WEAPON_SIZE];
    if (!hf_component_record(ops->read,ops->context,root,(uint32_t)(26151684+spec->group),
            spec->header,(uint32_t)(26151684+spec->map),spec->owner,spec->stride,&address) ||
        spec->stride > sizeof(source) || !vg70_data_access(ops, address, spec->stride) ||
        !vg70_read(ops, address, source, spec->stride) ||
        (memcmp(source, spec->before, spec->stride) != 0 &&
         memcmp(source, spec->after, spec->stride) != 0))
    {
        vg70_set_reason(reason, capacity, "VG-70 exact component source row or owner mapping changed");
        return 0;
    }
    *row = address;
    return 1;
}

int hf_vg70_native_prepare(const HfVg70MemoryOps* ops, uintptr_t game,
                           HfVg70NativePlan* plan, char* reason, size_t reason_capacity)
{
    uintptr_t owner_slot, projectile_slot;
    if (reason != NULL && reason_capacity > 0u)
        reason[0] = '\0';
    if (ops == NULL || ops->read == NULL || ops->write == NULL || ops->data_access == NULL ||
        plan == NULL || !vg70_canonical(game) || HF_VG70_COMPONENT_COUNT != 3u)
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 native memory API or component guard is unavailable");
        return 0;
    }
    memset(plan, 0, sizeof(*plan));
    plan->game = game;
    if (!vg70_address(game, HF_VG70_OWNER_SLOT, &owner_slot) ||
        !vg70_read_pointer(ops, owner_slot, &plan->owner) ||
        !vg70_address(plan->owner, HF_VG70_OWNER_WEAPON_MAP, &owner_slot) ||
        !vg70_read_pointer(ops, owner_slot, &plan->entities) ||
        !vg70_address(plan->owner, HF_VG70_OWNER_PROJECTILE_MAP, &projectile_slot) ||
        !vg70_read_pointer(ops, projectile_slot, &plan->projectile_map))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 game entity owner or component maps are unavailable");
        return 0;
    }
    if (!vg70_component_prepare(ops, plan->owner, &HF_VG70_COMPONENTS[0], &plan->weapon_address,
                                reason, reason_capacity) ||
        !vg70_component_prepare(ops, plan->owner, &HF_VG70_COMPONENTS[1], &plan->projectile_address,
                                reason, reason_capacity) ||
        !vg70_component_prepare(ops, plan->owner, &HF_VG70_COMPONENTS[2], &plan->magazine_address,
                                reason, reason_capacity))
    {
        if (reason != NULL && reason_capacity > 0u && reason[0] == '\0')
            vg70_set_reason(reason, reason_capacity, "VG-70 component owner-table anchor changed");
        return 0;
    }
    return 1;
}

static int vg70_component_owners_current(const HfVg70MemoryOps* ops,const HfVg70NativePlan* plan) {
    uintptr_t root=0;const uintptr_t rows[3]={plan->weapon_address,plan->projectile_address,plan->magazine_address};
    if (!vg70_read_pointer(ops,plan->game+0x346bf98u,&root) || root!=plan->owner) return 0;
    for (size_t i=0;i<3;i++) {
        const HfVg70ComponentSpec* s=&HF_VG70_COMPONENTS[i];uintptr_t row=0;
        if (!hf_component_record(ops->read,ops->context,root,(uint32_t)(26151684+s->group),s->header,
                (uint32_t)(26151684+s->map),s->owner,s->stride,&row) || row!=rows[i]) return 0;
    }
    return 1;
}

static int vg70_manager_snapshot(const HfVg70MemoryOps* ops, uintptr_t game, uint64_t slot,
                                 uint32_t header_size, uintptr_t* manager, uint8_t* header)
{
    uintptr_t address;
    return vg70_address(game, slot, &address) && vg70_read_pointer(ops, address, manager) &&
           vg70_read(ops, *manager, header, header_size);
}

static int vg70_entity_references(const HfVg70MemoryOps* ops, uintptr_t references,
                                  uint32_t index, uintptr_t expected)
{
    uintptr_t address, entity;
    return vg70_address(references, (uint64_t)index * 8u, &address) &&
           vg70_read_pointer(ops, address, &entity) && entity == expected;
}

static int vg70_cache_lookup(const HfVg70MemoryOps* ops, uintptr_t manager,
                             uint32_t forward, uint32_t reverse, uint32_t id,
                             uint32_t capacity_offset, uint32_t count_offset,
                             uint32_t* index, uint32_t* cached)
{
    int found;
    uint32_t value, reverse_id, limit;
    uintptr_t address;
    if (!vg70_map_lookup(ops, manager, forward, id, &found, &value))
        return 0;
    if (!found)
    {
        *cached = 0u;
        *index = 0u;
        return 1;
    }
    if (!vg70_map_lookup(ops, manager, reverse, value, &found, &reverse_id) || !found || reverse_id != id ||
        !vg70_address(manager, capacity_offset, &address) ||
        !vg70_read(ops, address, &limit, sizeof(limit)))
        return 0;
    if (value >= limit || !vg70_address(manager, count_offset, &address) ||
        !vg70_read(ops, address, &limit, sizeof(limit)) || value >= limit)
        return 0;
    *cached = 1u;
    *index = value;
    return 1;
}

int hf_vg70_native_discover(const HfVg70MemoryOps* ops, HfVg70NativePlan* plan,
                            HfVg70Instance* instances, size_t capacity, size_t* count,
                            char* reason, size_t reason_capacity)
{
    uintptr_t manager_slot, owner_slot, weapon_map_slot, projectile_map_slot, current_owner;
    uintptr_t current_entities, current_projectile_map, wm, pm, mm;
    uintptr_t refs, states, selections, weapon_rows;
    uintptr_t psi_refs, projectile_states, rate_states, rpm_states, projectile_rows;
    uintptr_t magazine_refs, magazine_ammo, magazine_effective, magazine_rows;
    uint32_t weapon_count, projectile_count, magazine_count, i;
    uint8_t identity[24];
    size_t found_count = 0u;

    if (count != NULL)
        *count = 0u;
    if (reason != NULL && reason_capacity > 0u)
        reason[0] = '\0';
    if (ops == NULL || plan == NULL || !vg70_component_owners_current(ops,plan) || instances == NULL || count == NULL || capacity == 0u || capacity > 512u ||
        !vg70_address(plan->game, HF_VG70_OWNER_SLOT, &owner_slot) ||
        !vg70_read_pointer(ops, owner_slot, &current_owner) || current_owner != plan->owner ||
        !vg70_address(current_owner, HF_VG70_OWNER_WEAPON_MAP, &weapon_map_slot) ||
        !vg70_read_pointer(ops, weapon_map_slot, &current_entities) || current_entities != plan->entities ||
        !vg70_address(current_owner, HF_VG70_OWNER_PROJECTILE_MAP, &projectile_map_slot) ||
        !vg70_read_pointer(ops, projectile_map_slot, &current_projectile_map) ||
        current_projectile_map != plan->projectile_map ||
        !vg70_manager_snapshot(ops, plan->game, HF_VG70_WEAPON_MANAGER_SLOT, 192u, &wm, plan->weapon_header) ||
        !vg70_manager_snapshot(ops, plan->game, HF_VG70_PROJECTILE_MANAGER_SLOT, 224u, &pm, plan->projectile_header) ||
        !vg70_manager_snapshot(ops, plan->game, HF_VG70_MAGAZINE_MANAGER_SLOT, 256u, &mm, plan->magazine_header))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 native managers are unavailable");
        return 0;
    }
    plan->weapon_manager = wm;
    plan->projectile_manager = pm;
    plan->magazine_manager = mm;
    weapon_count = get_u32(plan->weapon_header, 0x1cu);
    projectile_count = get_u32(plan->projectile_header, 0x38u);
    magazine_count = get_u32(plan->magazine_header, 0x10u);
    if (weapon_count > 512u || weapon_count > get_u32(plan->weapon_header, 0x10u) ||
        projectile_count > 512u || magazine_count > 512u ||
        magazine_count > get_u32(plan->magazine_header, 0x00u) ||
        !vg70_read_pointer(ops, wm + 0x48u, &refs) ||
        !vg70_read_pointer(ops, wm + 0x58u, &states) ||
        !vg70_read_pointer(ops, wm + 0x60u, &selections) ||
        !vg70_read_pointer(ops, wm + 0xb0u, &weapon_rows) ||
        !vg70_read_pointer(ops, pm + 0x68u, &psi_refs) ||
        !vg70_read_pointer(ops, pm + 0x78u, &projectile_states) ||
        !vg70_read_pointer(ops, pm + 0x70u, &rate_states) ||
        !vg70_read_pointer(ops, pm + 0x80u, &rpm_states) ||
        !vg70_read_pointer(ops, pm + 0xd0u, &projectile_rows) ||
        !vg70_read_pointer(ops, mm + 0x38u, &magazine_refs) ||
        !vg70_read_pointer(ops, mm + 0x50u, &magazine_ammo) ||
        !vg70_read_pointer(ops, mm + 0x48u, &magazine_effective) ||
        !vg70_read_pointer(ops, mm + 0xa0u, &magazine_rows))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 manager capacities or row-array pointers changed");
        return 0;
    }
    for (i = 0u; i < weapon_count; ++i)
    {
        uintptr_t ref_slot, reference, entity;
        uint32_t id, mode, weapon_index, psi_index, magazine_index;
        uint32_t weapon_cached, projectile_cached, magazine_cached;
        uint32_t weapon_private_index, projectile_private_index, magazine_private_index;
        int found;
        HfVg70Instance* item;

        if (!vg70_address(refs, (uint64_t)i * 8u, &ref_slot) ||
            !vg70_read_nullable_pointer(ops, ref_slot, &reference))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 weapon owner reference is unavailable");
            return 0;
        }
        if (reference == 0u)
            continue;
        entity = reference;
        if (!vg70_read(ops, entity, identity, sizeof(identity)))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 entity identity reference is invalid");
            return 0;
        }
        if (memcmp(identity, (const uint8_t[]){0x7F,0x8A,0x7C,0x57,0x97,0xCE,0x92,0xF9}, 8u) != 0)
            continue;
        if ((get_u32(identity, 20u) & 1u) == 0u)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 owned weapon entity is not player-owned");
            return 0;
        }
        id = get_u32(identity, 8u);
        {
            size_t prior;
            for (prior = 0u; prior < found_count; ++prior)
            {
                if (instances[prior].id == id || instances[prior].entity == entity)
                {
                    vg70_set_reason(reason, reason_capacity, "VG-70 entity manager contains duplicate owned identities");
                    return 0;
                }
            }
        }
        if (!vg70_map_lookup(ops, wm, 0x30u, id, &found, &weapon_index) || !found ||
            weapon_index >= weapon_count || !vg70_entity_references(ops, refs, weapon_index, entity))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 WeaponData owner index or reverse reference changed");
            return 0;
        }
        if (!vg70_address(selections, (uint64_t)weapon_index * 12u, &manager_slot) ||
            !vg70_read(ops, manager_slot, &mode, sizeof(mode)) || !supported_mode(mode))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 weapon mode is missing or unsupported");
            return 0;
        }
        if (!vg70_cache_lookup(ops, wm, 0x70u, 0x90u, id, 0x14u, 0xa8u,
                               &weapon_private_index, &weapon_cached) ||
            !vg70_cache_lookup(ops, pm, 0x90u, 0xb0u, id, 0x30u, 0xc8u,
                               &projectile_private_index, &projectile_cached) ||
            !vg70_map_lookup(ops, pm, 0x50u, id, &found, &psi_index) || !found ||
            psi_index >= projectile_count || !vg70_entity_references(ops, psi_refs, psi_index, entity) ||
            !vg70_map_lookup(ops, mm, 0x20u, id, &found, &magazine_index) || !found ||
            magazine_index >= magazine_count || !vg70_entity_references(ops, magazine_refs, magazine_index, entity) ||
            !vg70_cache_lookup(ops, mm, 0x60u, 0x80u, id, 4u, 0x98u,
                               &magazine_private_index, &magazine_cached))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 projectile state, magazine or private cache mapping changed");
            return 0;
        }
        if (found_count >= capacity)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 live instance count exceeds bounded output capacity");
            return 0;
        }
        item = &instances[found_count];
        memset(item, 0, sizeof(*item));
        item->id = id;
        item->mode = mode;
        item->slot = i;
        item->weapon_index = weapon_index;
        item->projectile_index = psi_index;
        item->magazine_index = magazine_index;
        item->weapon_private_index = weapon_private_index;
        item->projectile_private_index = projectile_private_index;
        item->magazine_private_index = magazine_private_index;
        item->entity = entity;
        item->mode_address = manager_slot;
        if (!vg70_address(states, (uint64_t)weapon_index * HF_VG70_WEAPON_STATE_SIZE, &item->weapon_state) ||
            !vg70_address(projectile_states, (uint64_t)psi_index * HF_VG70_PROJECTILE_STATE_SIZE, &item->projectile_state) ||
            !vg70_address(rate_states, (uint64_t)psi_index * HF_VG70_RATE_STATE_SIZE, &item->rate_state) ||
            !vg70_address(rpm_states, (uint64_t)psi_index * HF_VG70_CURRENT_RPM_SIZE, &item->current_rpm_state) ||
            !vg70_address(magazine_ammo, (uint64_t)magazine_index * 12u, &item->magazine_ammo) ||
            !vg70_address(magazine_effective, (uint64_t)magazine_index * 16u, &item->magazine_effective))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 state-row address overflowed");
            return 0;
        }
        item->weapon_cached = weapon_cached;
        item->projectile_cached = projectile_cached;
        item->magazine_cached = magazine_cached;
        memcpy(item->identity, identity, sizeof(item->identity));
        item->weapon = plan->weapon_address;
        if (weapon_cached &&
            !vg70_address(weapon_rows, (uint64_t)weapon_private_index * HF_VG70_WEAPON_SIZE, &item->weapon))
            goto invalid_cache;
        if (projectile_cached)
        {
            if (!vg70_address(projectile_rows,
                              (uint64_t)projectile_private_index * HF_VG70_PROJECTILE_SIZE, &item->projectile))
                goto invalid_cache;
        }
        else
            item->projectile = plan->projectile_address;
        if (magazine_cached)
        {
            if (!vg70_address(magazine_rows,
                              (uint64_t)magazine_private_index * HF_VG70_MAGAZINE_SIZE, &item->magazine))
                goto invalid_cache;
        }
        else
            item->magazine = plan->magazine_address;
        if (!vg70_read(ops, item->weapon, item->weapon_row, sizeof(item->weapon_row)) ||
            !vg70_read(ops, item->projectile, item->projectile_row, sizeof(item->projectile_row)) ||
            !vg70_read(ops, item->magazine, item->magazine_row, sizeof(item->magazine_row)) ||
            !vg70_read(ops, item->weapon_state, item->weapon_state_row, sizeof(item->weapon_state_row)) ||
            !vg70_read(ops, item->rate_state, item->rate_state_row, sizeof(item->rate_state_row)) ||
            !vg70_read(ops, item->current_rpm_state, item->current_rpm_row, sizeof(item->current_rpm_row)) ||
            !vg70_read(ops, item->projectile_state, item->projectile_state_row,
                       sizeof(item->projectile_state_row)) ||
            !vg70_read(ops, item->entity, identity, sizeof(identity)) ||
            memcmp(item->identity, identity, sizeof(identity)) != 0)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 entity or component data changed during discovery");
            return 0;
        }
        ++found_count;
        continue;

invalid_cache:
        vg70_set_reason(reason, reason_capacity, "VG-70 private component cache index is unavailable");
        return 0;
    }
    *count = found_count;
    return 1;
}

int hf_vg70_native_validate(const HfVg70MemoryOps* ops, const HfVg70NativePlan* plan,
                            const HfVg70Instance* instances, size_t count)
{
    uint8_t header[256], identity[24];
    uintptr_t manager, address, owner, entities, projectile_map;
    uintptr_t weapon_refs, weapon_selections, weapon_rows;
    uintptr_t psi_refs, projectile_states, rate_states, rpm_states, projectile_rows;
    uintptr_t magazine_refs, magazine_ammo, magazine_effective, magazine_rows;
    size_t i;
    if (ops == NULL || plan == NULL || !vg70_component_owners_current(ops,plan) || count > 512u || (count > 0u && instances == NULL) ||
        !vg70_address(plan->game, HF_VG70_OWNER_SLOT, &address) ||
        !vg70_read_pointer(ops, address, &owner) || owner != plan->owner ||
        !vg70_address(owner, HF_VG70_OWNER_WEAPON_MAP, &address) ||
        !vg70_read_pointer(ops, address, &entities) || entities != plan->entities ||
        !vg70_address(owner, HF_VG70_OWNER_PROJECTILE_MAP, &address) ||
        !vg70_read_pointer(ops, address, &projectile_map) || projectile_map != plan->projectile_map ||
        !vg70_manager_snapshot(ops, plan->game, HF_VG70_WEAPON_MANAGER_SLOT, 192u, &manager, header) ||
        manager != plan->weapon_manager || memcmp(header, plan->weapon_header, 192u) != 0 ||
        !vg70_manager_snapshot(ops, plan->game, HF_VG70_PROJECTILE_MANAGER_SLOT, 224u, &manager, header) ||
        manager != plan->projectile_manager || memcmp(header, plan->projectile_header, 224u) != 0 ||
        !vg70_manager_snapshot(ops, plan->game, HF_VG70_MAGAZINE_MANAGER_SLOT, 256u, &manager, header) ||
        manager != plan->magazine_manager || memcmp(header, plan->magazine_header, 256u) != 0 ||
        !vg70_address(plan->weapon_manager, 0x48u, &address) ||
        !vg70_read_pointer(ops, address, &weapon_refs) ||
        !vg70_address(plan->weapon_manager, 0x60u, &address) ||
        !vg70_read_pointer(ops, address, &weapon_selections) ||
        !vg70_address(plan->weapon_manager, 0xb0u, &address) ||
        !vg70_read_pointer(ops, address, &weapon_rows) ||
        !vg70_address(plan->projectile_manager, 0x68u, &address) ||
        !vg70_read_pointer(ops, address, &psi_refs) ||
        !vg70_address(plan->projectile_manager, 0x78u, &address) ||
        !vg70_read_pointer(ops, address, &projectile_states) ||
        !vg70_address(plan->projectile_manager, 0x70u, &address) ||
        !vg70_read_pointer(ops, address, &rate_states) ||
        !vg70_address(plan->projectile_manager, 0x80u, &address) ||
        !vg70_read_pointer(ops, address, &rpm_states) ||
        !vg70_address(plan->projectile_manager, 0xd0u, &address) ||
        !vg70_read_pointer(ops, address, &projectile_rows) ||
        !vg70_address(plan->magazine_manager, 0x38u, &address) ||
        !vg70_read_pointer(ops, address, &magazine_refs) ||
        !vg70_address(plan->magazine_manager, 0x50u, &address) ||
        !vg70_read_pointer(ops, address, &magazine_ammo) ||
        !vg70_address(plan->magazine_manager, 0x48u, &address) ||
        !vg70_read_pointer(ops, address, &magazine_effective) ||
        !vg70_address(plan->magazine_manager, 0xa0u, &address) ||
        !vg70_read_pointer(ops, address, &magazine_rows))
        return 0;

    for (i = 0u; i < count; ++i)
    {
        const HfVg70Instance* item = &instances[i];
        uintptr_t expected;
        uint32_t mode, index, cached;
        int found;
        uint8_t row[HF_VG70_WEAPON_SIZE];

        if (!vg70_read(ops, item->entity, identity, sizeof(identity)) ||
            memcmp(identity, item->identity, sizeof(identity)) != 0 ||
            memcmp(identity, (const uint8_t[]){0x7F,0x8A,0x7C,0x57,0x97,0xCE,0x92,0xF9}, 8u) != 0 ||
            (get_u32(identity, 20u) & 1u) == 0u || get_u32(identity, 8u) != item->id ||
            !vg70_read(ops, item->mode_address, &mode, sizeof(mode)) || mode != item->mode || !supported_mode(mode) ||
            !vg70_entity_references(ops, weapon_refs, item->weapon_index, item->entity) ||
            !vg70_entity_references(ops, psi_refs, item->projectile_index, item->entity) ||
            !vg70_entity_references(ops, magazine_refs, item->magazine_index, item->entity))
            return 0;

        if (!vg70_map_lookup(ops, plan->weapon_manager, 0x30u, item->id, &found, &index) ||
            !found || index != item->weapon_index ||
            !vg70_map_lookup(ops, plan->projectile_manager, 0x50u, item->id, &found, &index) ||
            !found || index != item->projectile_index ||
            !vg70_map_lookup(ops, plan->magazine_manager, 0x20u, item->id, &found, &index) ||
            !found || index != item->magazine_index)
            return 0;
        if (!vg70_cache_lookup(ops, plan->weapon_manager, 0x70u, 0x90u, item->id, 0x14u, 0xa8u,
                               &index, &cached) || cached != item->weapon_cached ||
            (cached != 0u && index != item->weapon_private_index) ||
            !vg70_cache_lookup(ops, plan->projectile_manager, 0x90u, 0xb0u, item->id, 0x30u, 0xc8u,
                               &index, &cached) || cached != item->projectile_cached ||
            (cached != 0u && index != item->projectile_private_index) ||
            !vg70_cache_lookup(ops, plan->magazine_manager, 0x60u, 0x80u, item->id, 4u, 0x98u,
                               &index, &cached) || cached != item->magazine_cached ||
            (cached != 0u && index != item->magazine_private_index))
            return 0;

        if (!vg70_address(weapon_selections, (uint64_t)item->weapon_index * 12u, &expected) ||
            expected != item->mode_address ||
            (item->weapon_cached != 0u &&
             !vg70_address(weapon_rows, (uint64_t)item->weapon_private_index * HF_VG70_WEAPON_SIZE, &expected)) ||
            (item->weapon_cached == 0u && (expected = plan->weapon_address) == 0u) || expected != item->weapon)
            return 0;
        if ((item->projectile_cached != 0u &&
             !vg70_address(projectile_rows,
                           (uint64_t)item->projectile_private_index * HF_VG70_PROJECTILE_SIZE, &expected)) ||
            (item->projectile_cached == 0u && (expected = plan->projectile_address) == 0u) ||
            expected != item->projectile ||
            (item->magazine_cached != 0u &&
             !vg70_address(magazine_rows,
                           (uint64_t)item->magazine_private_index * HF_VG70_MAGAZINE_SIZE, &expected)) ||
            (item->magazine_cached == 0u && (expected = plan->magazine_address) == 0u) ||
            expected != item->magazine ||
            !vg70_address(projectile_states, (uint64_t)item->projectile_index * HF_VG70_PROJECTILE_STATE_SIZE,
                          &expected) || expected != item->projectile_state ||
            !vg70_address(rate_states, (uint64_t)item->projectile_index * HF_VG70_RATE_STATE_SIZE,
                          &expected) || expected != item->rate_state ||
            !vg70_address(rpm_states, (uint64_t)item->projectile_index * HF_VG70_CURRENT_RPM_SIZE,
                          &expected) || expected != item->current_rpm_state ||
            !vg70_address(magazine_ammo, (uint64_t)item->magazine_index * 12u, &expected) ||
            expected != item->magazine_ammo ||
            !vg70_address(magazine_effective, (uint64_t)item->magazine_index * 16u, &expected) ||
            expected != item->magazine_effective)
            return 0;
        if (!vg70_read(ops, item->weapon, row, sizeof(item->weapon_row)) ||
            memcmp(row, item->weapon_row, sizeof(item->weapon_row)) != 0 ||
            !vg70_read(ops, item->projectile, row, sizeof(item->projectile_row)) ||
            memcmp(row, item->projectile_row, sizeof(item->projectile_row)) != 0 ||
            !vg70_read(ops, item->magazine, row, sizeof(item->magazine_row)) ||
            memcmp(row, item->magazine_row, sizeof(item->magazine_row)) != 0 ||
            !vg70_read(ops, item->weapon_state, row, sizeof(item->weapon_state_row)) ||
            memcmp(row, item->weapon_state_row, sizeof(item->weapon_state_row)) != 0 ||
            !vg70_read(ops, item->rate_state, row, sizeof(item->rate_state_row)) ||
            memcmp(row, item->rate_state_row, sizeof(item->rate_state_row)) != 0 ||
            !vg70_read(ops, item->current_rpm_state, row, sizeof(item->current_rpm_row)) ||
            memcmp(row, item->current_rpm_row, sizeof(item->current_rpm_row)) != 0 ||
            !vg70_read(ops, item->projectile_state, row, sizeof(item->projectile_state_row)) ||
            memcmp(row, item->projectile_state_row, sizeof(item->projectile_state_row)) != 0)
            return 0;
    }
    return 1;
}

typedef struct HfVg70MapReservation
{
    uintptr_t address;
    uint32_t key, value;
} HfVg70MapReservation;

typedef struct HfVg70PendingMap
{
    uintptr_t forward, reverse;
    uint32_t index, id;
} HfVg70PendingMap;

typedef struct HfVg70CacheSpec
{
    uintptr_t manager;
    uint8_t* header;
    uint32_t capacity_offset, count_offset, forward_offset, reverse_offset, rows_offset, stride;
} HfVg70CacheSpec;

typedef enum HfVg70CacheKind
{
    HF_VG70_CACHE_WEAPON,
    HF_VG70_CACHE_PROJECTILE,
    HF_VG70_CACHE_MAGAZINE
} HfVg70CacheKind;

#define HF_VG70_NATIVE_MAX_EDITS \
    (512u * ((HF_VG70_WEAPON_SIZE + HF_VG70_PROJECTILE_SIZE + HF_VG70_MAGAZINE_SIZE) / 4u + 12u) + 3u)
#define HF_VG70_NATIVE_MAX_RESERVATIONS (512u * 6u)

static uint32_t* vg70_instance_cached(HfVg70Instance* item, HfVg70CacheKind kind)
{
    if (kind == HF_VG70_CACHE_WEAPON)
        return &item->weapon_cached;
    if (kind == HF_VG70_CACHE_PROJECTILE)
        return &item->projectile_cached;
    return &item->magazine_cached;
}

static uint32_t* vg70_instance_private_index(HfVg70Instance* item, HfVg70CacheKind kind)
{
    if (kind == HF_VG70_CACHE_WEAPON)
        return &item->weapon_private_index;
    if (kind == HF_VG70_CACHE_PROJECTILE)
        return &item->projectile_private_index;
    return &item->magazine_private_index;
}

static uintptr_t* vg70_instance_row_address(HfVg70Instance* item, HfVg70CacheKind kind)
{
    if (kind == HF_VG70_CACHE_WEAPON)
        return &item->weapon;
    if (kind == HF_VG70_CACHE_PROJECTILE)
        return &item->projectile;
    return &item->magazine;
}

static uint8_t* vg70_instance_row(HfVg70Instance* item, HfVg70CacheKind kind)
{
    if (kind == HF_VG70_CACHE_WEAPON)
        return item->weapon_row;
    if (kind == HF_VG70_CACHE_PROJECTILE)
        return item->projectile_row;
    return item->magazine_row;
}

static int vg70_stage_word(const HfVg70MemoryOps* ops, HfVg70WordEdit* edits,
                           size_t edit_capacity, size_t* edit_count, uintptr_t address,
                           const uint8_t after[4], char* reason, size_t reason_capacity)
{
    uint8_t before[4];
    size_t i;
    if (!vg70_data_access(ops, address, sizeof(before)) || !vg70_read(ops, address, before, sizeof(before)))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 private cache field is unreadable or not writable");
        return 0;
    }
    if (memcmp(before, after, sizeof(before)) == 0)
        return 1;
    for (i = 0u; i < *edit_count; ++i)
    {
        if (edits[i].address == address)
        {
            if (memcmp(edits[i].after, after, sizeof(edits[i].after)) == 0)
                return 1;
            vg70_set_reason(reason, reason_capacity, "VG-70 private cache transaction has conflicting writes");
            return 0;
        }
    }
    if (*edit_count >= edit_capacity)
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 private cache edit bound was exceeded");
        return 0;
    }
    edits[*edit_count].address = address;
    memcpy(edits[*edit_count].before, before, sizeof(before));
    memcpy(edits[*edit_count].after, after, sizeof(before));
    ++*edit_count;
    return 1;
}

static int vg70_find_reservation(const HfVg70MapReservation* reservations, size_t count,
                                 uintptr_t address, uint32_t* key, uint32_t* value)
{
    size_t i;
    for (i = count; i > 0u; --i)
    {
        if (reservations[i - 1u].address == address)
        {
            *key = reservations[i - 1u].key;
            *value = reservations[i - 1u].value;
            return 1;
        }
    }
    return 0;
}

static int vg70_reserve_map(const HfVg70MemoryOps* ops, uintptr_t manager, uint32_t map_offset,
                            uint32_t key, uint32_t value, HfVg70MapReservation* reservations,
                            size_t reservation_capacity, size_t* reservation_count,
                            uintptr_t* output, char* reason, size_t reason_capacity)
{
    uint8_t descriptor[20];
    uintptr_t descriptor_address, array, tombstone = 0u;
    uint32_t capacity, empty, multiplier, product, probe;
    uint64_t mixed;
    if (output == NULL || reservation_count == NULL ||
        !vg70_address(manager, map_offset, &descriptor_address) ||
        !vg70_read(ops, descriptor_address, descriptor, sizeof(descriptor)))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 private-map descriptor is unavailable");
        return 0;
    }
    capacity = get_u32(descriptor, 8u);
    empty = get_u32(descriptor, 12u);
    multiplier = get_u32(descriptor, 16u);
    if (capacity == 0u || capacity > 4096u || (capacity & (capacity - 1u)) != 0u ||
        !vg70_read_pointer(ops, descriptor_address, &array))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 private-map capacity or row array is invalid");
        return 0;
    }
    mixed = (uint64_t)(key & 0xFFFFu) * multiplier +
            (uint64_t)(key >> 16) * (uint64_t)(multiplier & 0xFFFFu) * 65536u;
    product = (uint32_t)mixed;
    for (probe = 0u; probe < capacity && probe < 64u; ++probe)
    {
        uint8_t row[8];
        uintptr_t address;
        uint32_t stored, indexed;
        const uint32_t slot = (product + probe) & (capacity - 1u);
        if (!vg70_address(array, (uint64_t)slot * 8u, &address))
            break;
        if (!vg70_find_reservation(reservations, *reservation_count, address, &stored, &indexed))
        {
            if (!vg70_read(ops, address, row, sizeof(row)))
                break;
            stored = get_u32(row, 0u);
            indexed = get_u32(row, 4u);
        }
        if (stored == key && indexed != UINT32_MAX)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 private-map identity already has an active row");
            return 0;
        }
        if (indexed == UINT32_MAX && tombstone == 0u)
            tombstone = address;
        if (stored == empty || stored == key)
        {
            const uintptr_t target = tombstone != 0u ? tombstone : address;
            if (*reservation_count >= reservation_capacity)
            {
                vg70_set_reason(reason, reason_capacity, "VG-70 private-map reservation bound was exceeded");
                return 0;
            }
            reservations[*reservation_count].address = target;
            reservations[*reservation_count].key = key;
            reservations[*reservation_count].value = value;
            ++*reservation_count;
            *output = target;
            return 1;
        }
    }
    if (tombstone != 0u)
    {
        if (*reservation_count >= reservation_capacity)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 private-map reservation bound was exceeded");
            return 0;
        }
        reservations[*reservation_count].address = tombstone;
        reservations[*reservation_count].key = key;
        reservations[*reservation_count].value = value;
        ++*reservation_count;
        *output = tombstone;
        return 1;
    }
    vg70_set_reason(reason, reason_capacity, "VG-70 private map has no bounded reservation slot");
    return 0;
}

static int vg70_isolate_manager(const HfVg70MemoryOps* ops, HfVg70CacheSpec* spec,
                                HfVg70Instance* instances, size_t count, HfVg70CacheKind kind,
                                HfVg70WordEdit* edits, size_t edit_capacity, size_t* edit_count,
                                HfVg70MapReservation* reservations, size_t reservation_capacity,
                                size_t* reservation_count, uint32_t* rows_added,
                                char* reason, size_t reason_capacity)
{
    HfVg70PendingMap* pending = NULL;
    uintptr_t rows;
    uint32_t capacity, current_count, next_index;
    uint8_t count_after[4];
    size_t pending_count = 0u, i;
    if (!vg70_address(spec->manager, spec->capacity_offset, &rows) ||
        !vg70_read(ops, rows, &capacity, sizeof(capacity)) || capacity == 0u || capacity > 512u ||
        !vg70_address(spec->manager, spec->count_offset, &rows) ||
        !vg70_read(ops, rows, &current_count, sizeof(current_count)) || current_count > capacity ||
        !vg70_address(spec->manager, spec->rows_offset, &rows) ||
        !vg70_read_pointer(ops, rows, &rows))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 private component cache capacity or row array changed");
        return 0;
    }
    if (count > 0u)
    {
        pending = (HfVg70PendingMap*)calloc(count, sizeof(*pending));
        if (pending == NULL)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 private cache staging allocation failed");
            return 0;
        }
    }
    next_index = current_count;
    for (i = 0u; i < count; ++i)
    {
        HfVg70Instance* item = &instances[i];
        uint32_t* cached = vg70_instance_cached(item, kind);
        uint32_t* private_index = vg70_instance_private_index(item, kind);
        uintptr_t* item_row = vg70_instance_row_address(item, kind);
        uint8_t* source = vg70_instance_row(item, kind);
        uintptr_t target;
        uint8_t actual_source[HF_VG70_WEAPON_SIZE];
        size_t offset;
        if (*cached != 0u)
            continue;
        if (next_index >= capacity || spec->stride == 0u || spec->stride > sizeof(actual_source) ||
            !vg70_read(ops, *item_row, actual_source, spec->stride) ||
            memcmp(actual_source, source, spec->stride) != 0 ||
            !vg70_address(rows, (uint64_t)next_index * spec->stride, &target) ||
            !vg70_data_access(ops, target, spec->stride))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 source row changed or private component cache is exhausted");
            free(pending);
            return 0;
        }
        for (offset = 0u; offset < spec->stride; offset += 4u)
        {
            uintptr_t word_address;
            if (!vg70_address(target, (uint64_t)offset, &word_address) ||
                !vg70_stage_word(ops, edits, edit_capacity, edit_count, word_address,
                                 source + offset, reason, reason_capacity))
            {
                free(pending);
                return 0;
            }
        }
        if (!vg70_reserve_map(ops, spec->manager, spec->forward_offset, item->id, next_index,
                              reservations, reservation_capacity, reservation_count,
                              &pending[pending_count].forward, reason, reason_capacity) ||
            !vg70_reserve_map(ops, spec->manager, spec->reverse_offset, next_index, item->id,
                              reservations, reservation_capacity, reservation_count,
                              &pending[pending_count].reverse, reason, reason_capacity))
        {
            free(pending);
            return 0;
        }
        pending[pending_count].index = next_index;
        pending[pending_count].id = item->id;
        ++pending_count;
        *item_row = target;
        *private_index = next_index;
        *cached = 1u;
        ++next_index;
        if (*rows_added == UINT32_MAX)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 private cache row count overflowed");
            free(pending);
            return 0;
        }
        ++*rows_added;
    }
    if (next_index != current_count)
    {
        uintptr_t count_address;
        put_u32(count_after, 0u, next_index);
        if (!vg70_address(spec->manager, spec->count_offset, &count_address) ||
            !vg70_stage_word(ops, edits, edit_capacity, edit_count, count_address,
                             count_after, reason, reason_capacity))
        {
            free(pending);
            return 0;
        }
    }
    for (i = 0u; i < pending_count; ++i)
    {
        uint8_t word[4];
        put_u32(word, 0u, pending[i].id);
        if (!vg70_stage_word(ops, edits, edit_capacity, edit_count, pending[i].reverse + 4u,
                             word, reason, reason_capacity))
        {
            free(pending);
            return 0;
        }
        put_u32(word, 0u, pending[i].index);
        if (!vg70_stage_word(ops, edits, edit_capacity, edit_count, pending[i].reverse,
                             word, reason, reason_capacity))
        {
            free(pending);
            return 0;
        }
        put_u32(word, 0u, pending[i].index);
        if (!vg70_stage_word(ops, edits, edit_capacity, edit_count, pending[i].forward + 4u,
                             word, reason, reason_capacity))
        {
            free(pending);
            return 0;
        }
        put_u32(word, 0u, pending[i].id);
        if (!vg70_stage_word(ops, edits, edit_capacity, edit_count, pending[i].forward,
                             word, reason, reason_capacity))
        {
            free(pending);
            return 0;
        }
    }
    if (next_index != current_count)
        put_u32(spec->header, spec->count_offset, next_index);
    free(pending);
    return 1;
}

HfVg70ApplyResult hf_vg70_native_isolate(const HfVg70MemoryOps* ops,
                                          HfVg70NativePlan* plan,
                                          HfVg70Instance* instances, size_t count,
                                          uint32_t* new_rows,
                                          char* reason, size_t reason_capacity)
{
    HfVg70NativePlan next_plan;
    HfVg70Instance* next_instances = NULL;
    HfVg70WordEdit* edits = NULL;
    HfVg70MapReservation* reservations = NULL;
    size_t edit_capacity, edit_count = 0u, reservation_capacity, reservation_count = 0u, i, attempted = 0u;
    size_t missing_weapon = 0u, missing_projectile = 0u, missing_magazine = 0u, missing_total;
    uint32_t rows_added = 0u;
    uint8_t actual[4];
    HfVg70ApplyResult result = HF_VG70_REFUSED;
    HfVg70CacheSpec caches[3];

    if (new_rows != NULL)
        *new_rows = 0u;
    if (reason != NULL && reason_capacity > 0u)
        reason[0] = '\0';
    if (ops == NULL || ops->read == NULL || ops->write == NULL || ops->data_access == NULL ||
        plan == NULL || (count > 0u && instances == NULL) || count > 512u ||
        !hf_vg70_native_validate(ops, plan, instances, count))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 native snapshots changed before private cache isolation");
        return HF_VG70_REFUSED;
    }
    for (i = 0u; i < count; ++i)
    {
        missing_weapon += instances[i].weapon_cached == 0u;
        missing_projectile += instances[i].projectile_cached == 0u;
        missing_magazine += instances[i].magazine_cached == 0u;
    }
    missing_total = missing_weapon + missing_projectile + missing_magazine;
    if (missing_total == 0u)
    {
        if (reason != NULL && reason_capacity > 0u)
            (void)snprintf(reason, reason_capacity, "VG-70 private component caches already exist");
        return HF_VG70_APPLIED;
    }
    edit_capacity = missing_weapon * (HF_VG70_WEAPON_SIZE / 4u + 4u) +
                    missing_projectile * (HF_VG70_PROJECTILE_SIZE / 4u + 4u) +
                    missing_magazine * (HF_VG70_MAGAZINE_SIZE / 4u + 4u) + 3u;
    reservation_capacity = missing_total * 2u;
    if (count > 0u)
    {
        next_instances = (HfVg70Instance*)malloc(count * sizeof(*next_instances));
        reservations = (HfVg70MapReservation*)calloc(reservation_capacity, sizeof(*reservations));
    }
    edits = (HfVg70WordEdit*)calloc(edit_capacity, sizeof(*edits));
    if ((count > 0u && (next_instances == NULL || reservations == NULL)) || edits == NULL)
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 private cache transaction allocation failed");
        goto cleanup;
    }
    next_plan = *plan;
    if (count > 0u)
        memcpy(next_instances, instances, count * sizeof(*next_instances));

    caches[0] = (HfVg70CacheSpec){next_plan.weapon_manager, next_plan.weapon_header,
        0x14u, 0xa8u, 0x70u, 0x90u, 0xb0u, HF_VG70_WEAPON_SIZE};
    caches[1] = (HfVg70CacheSpec){next_plan.projectile_manager, next_plan.projectile_header,
        0x30u, 0xc8u, 0x90u, 0xb0u, 0xd0u, HF_VG70_PROJECTILE_SIZE};
    caches[2] = (HfVg70CacheSpec){next_plan.magazine_manager, next_plan.magazine_header,
        4u, 0x98u, 0x60u, 0x80u, 0xa0u, HF_VG70_MAGAZINE_SIZE};
    for (i = 0u; i < 3u; ++i)
    {
        const HfVg70CacheKind kind = (HfVg70CacheKind)i;
        if (!vg70_isolate_manager(ops, &caches[i], next_instances, count, kind, edits,
                                  edit_capacity, &edit_count, reservations, reservation_capacity,
                                  &reservation_count, &rows_added, reason, reason_capacity))
            goto cleanup;
    }

    for (i = 0u; i < edit_count; ++i)
    {
        if (!vg70_data_access(ops, edits[i].address, sizeof(edits[i].before)) ||
            !vg70_read(ops, edits[i].address, actual, sizeof(actual)) ||
            memcmp(actual, edits[i].before, sizeof(actual)) != 0)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 private cache data changed before commit");
            goto cleanup;
        }
    }
    for (i = 0u; i < edit_count; ++i)
    {
        attempted = i + 1u;
        if (ops->write(ops->context, edits[i].address, edits[i].after, sizeof(edits[i].after)) != 0 ||
            !vg70_read(ops, edits[i].address, actual, sizeof(actual)) ||
            memcmp(actual, edits[i].after, sizeof(actual)) != 0)
        {
            const int rollback_ok = vg70_rollback(ops, edits, attempted);
            vg70_set_reason(reason, reason_capacity, rollback_ok ?
                "VG-70 private cache write failed; prior fields were rolled back" :
                "FATAL: VG-70 private cache write failed and rollback was incomplete");
            result = rollback_ok ? HF_VG70_REFUSED : HF_VG70_FATAL;
            goto cleanup;
        }
    }
    if (!hf_vg70_native_validate(ops, &next_plan, next_instances, count))
    {
        const int rollback_ok = vg70_rollback(ops, edits, attempted);
        vg70_set_reason(reason, reason_capacity, rollback_ok ?
            "VG-70 private cache verification failed; changes were rolled back" :
            "FATAL: VG-70 private cache verification failed and rollback was incomplete");
        result = rollback_ok ? HF_VG70_REFUSED : HF_VG70_FATAL;
        goto cleanup;
    }
    *plan = next_plan;
    if (count > 0u)
        memcpy(instances, next_instances, count * sizeof(*instances));
    if (new_rows != NULL)
        *new_rows = rows_added;
    if (reason != NULL && reason_capacity > 0u)
        (void)snprintf(reason, reason_capacity,
                       "Applied VG-70 private component caches (%u new rows, %u fields)",
                       (unsigned)rows_added, (unsigned)edit_count);
    result = HF_VG70_APPLIED;

cleanup:
    free(next_instances);
    free(reservations);
    free(edits);
    return result;
}

static int vg70_rate_value(float value, uint32_t* output)
{
    if (output == NULL || !isfinite(value))
        return 0;
    if (close_f32(value, 300.0f))
        *output = 300u;
    else if (close_f32(value, 550.0f))
        *output = 550u;
    else if (close_f32(value, 750.0f))
        *output = 750u;
    else if (close_f32(value, 1000.0f))
        *output = 1000u;
    else
        return 0;
    return 1;
}

static int vg70_stage_bytes(const HfVg70MemoryOps* ops, HfVg70WordEdit* edits,
                            size_t edit_capacity, size_t* edit_count, uintptr_t address,
                            const uint8_t* before, const uint8_t* after, size_t size,
                            char* reason, size_t reason_capacity)
{
    uint8_t actual[HF_VG70_WEAPON_SIZE];
    size_t offset;
    if (before == NULL || after == NULL || size == 0u || size > sizeof(actual) || size % 4u != 0u ||
        !vg70_data_access(ops, address, size) || !vg70_read(ops, address, actual, size) ||
        memcmp(actual, before, size) != 0)
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 instance row changed or is inaccessible before staging");
        return 0;
    }
    for (offset = 0u; offset < size; offset += 4u)
    {
        HfVg70WordEdit* edit;
        uintptr_t field;
        if (memcmp(before + offset, after + offset, 4u) == 0)
            continue;
        if (*edit_count >= edit_capacity || !vg70_address(address, offset, &field))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 instance transaction exceeded its bounded edit capacity");
            return 0;
        }
        edit = &edits[(*edit_count)++];
        edit->address = field;
        memcpy(edit->before, before + offset, sizeof(edit->before));
        memcpy(edit->after, after + offset, sizeof(edit->after));
    }
    return 1;
}

static int vg70_current_mode_memory(const HfVg70ModeMemory* memories, size_t capacity,
                                    uint64_t id, uintptr_t entity)
{
    size_t i;
    for (i = 0u; i < capacity; ++i)
        if (memories[i].active != 0u && memories[i].entity_id == id && memories[i].entity_identity == entity)
            return (int)i;
    return -1;
}

HfVg70ApplyResult hf_vg70_native_configure(const HfVg70MemoryOps* ops,
                                            const HfVg70NativePlan* plan,
                                            HfVg70Instance* instances, size_t count,
                                            HfVg70ModeMemory* memories, size_t memory_capacity,
                                            int boot_sweep_done, uint32_t* changed_fields,
                                            char* reason, size_t reason_capacity)
{
    static const size_t max_bytes_per_instance =
        HF_VG70_WEAPON_SIZE + HF_VG70_PROJECTILE_SIZE + HF_VG70_MAGAZINE_SIZE +
        HF_VG70_WEAPON_STATE_SIZE + HF_VG70_RATE_STATE_SIZE +
        HF_VG70_CURRENT_RPM_SIZE + HF_VG70_PROJECTILE_STATE_SIZE;
    HfVg70Instance* next_instances = NULL;
    HfVg70ModeMemory* next_memories = NULL;
    HfVg70WordEdit* edits = NULL;
    size_t edit_capacity, edit_count = 0u, attempted = 0u, i, j;
    uint8_t actual[4];
    HfVg70ApplyResult result = HF_VG70_REFUSED;

    if (changed_fields != NULL)
        *changed_fields = 0u;
    if (reason != NULL && reason_capacity > 0u)
        reason[0] = '\0';
    if (ops == NULL || ops->read == NULL || ops->write == NULL || ops->data_access == NULL ||
        plan == NULL || count > HF_VG70_MAX_INSTANCES ||
        (count > 0u && (instances == NULL || memories == NULL)) || memory_capacity < count ||
        memory_capacity > HF_VG70_MAX_INSTANCES ||
        !hf_vg70_native_validate(ops, plan, instances, count))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 native snapshots changed before instance configuration");
        return HF_VG70_REFUSED;
    }
    if (count > SIZE_MAX / (max_bytes_per_instance / 4u) ||
        memory_capacity > SIZE_MAX / sizeof(*next_memories))
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 instance transaction size overflowed");
        return HF_VG70_REFUSED;
    }
    edit_capacity = count * (max_bytes_per_instance / 4u);
    next_instances = count == 0u ? NULL : (HfVg70Instance*)malloc(count * sizeof(*next_instances));
    next_memories = memory_capacity == 0u ? NULL :
        (HfVg70ModeMemory*)malloc(memory_capacity * sizeof(*next_memories));
    edits = (HfVg70WordEdit*)calloc(edit_capacity == 0u ? 1u : edit_capacity, sizeof(*edits));
    if ((count > 0u && next_instances == NULL) || (memory_capacity > 0u && next_memories == NULL) || edits == NULL)
    {
        vg70_set_reason(reason, reason_capacity, "VG-70 instance transaction allocation failed");
        goto cleanup;
    }
    if (count > 0u)
        memcpy(next_instances, instances, count * sizeof(*next_instances));
    if (memory_capacity > 0u)
        memcpy(next_memories, memories, memory_capacity * sizeof(*next_memories));

    /* Remove stale identities before assigning slots to weapons equipped this tick. */
    for (i = 0u; i < memory_capacity; ++i)
    {
        int present = 0;
        if (next_memories[i].active == 0u)
            continue;
        for (j = 0u; j < count; ++j)
        {
            if (next_memories[i].entity_id == instances[j].id &&
                next_memories[i].entity_identity == instances[j].entity)
            {
                present = 1;
                break;
            }
        }
        if (!present)
            memset(&next_memories[i], 0, sizeof(next_memories[i]));
    }

    for (i = 0u; i < count; ++i)
    {
        HfVg70Instance* item = &next_instances[i];
        uint8_t weapon_after[HF_VG70_WEAPON_SIZE];
        uint8_t projectile_after[HF_VG70_PROJECTILE_SIZE];
        uint8_t magazine_after[HF_VG70_MAGAZINE_SIZE];
        uint8_t weapon_state_after[HF_VG70_WEAPON_STATE_SIZE];
        uint8_t rate_state_after[HF_VG70_RATE_STATE_SIZE];
        uint8_t current_rpm_after[HF_VG70_CURRENT_RPM_SIZE];
        uint8_t projectile_state_after[HF_VG70_PROJECTILE_STATE_SIZE];
        uint32_t current_vector[3], current_rpm, selected_slot, memory_slot;
        HfVg70ModeMemory* mode_memory;
        HfVg70Selection selection;
        float cadence_multiplier;
        int prior = vg70_current_mode_memory(next_memories, memory_capacity, item->id, item->entity);

        if (!vg70_rate_value(get_f32(item->rate_state_row, 16u), &current_vector[0]) ||
            !vg70_rate_value(get_f32(item->rate_state_row, 20u), &current_vector[1]) ||
            !vg70_rate_value(get_f32(item->rate_state_row, 24u), &current_vector[2]) ||
            !vg70_rate_value(get_f32(item->current_rpm_row, 4u), &current_rpm))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 selector contains an unsupported RPM value");
            goto cleanup;
        }
        selected_slot = get_u32(item->rate_state_row, 28u);
        if (prior >= 0)
            memory_slot = (uint32_t)prior;
        else
        {
            for (j = 0u; j < memory_capacity && next_memories[j].active != 0u; ++j)
                ;
            if (j == memory_capacity)
            {
                vg70_set_reason(reason, reason_capacity, "VG-70 mode memory is full for the bounded live instance set");
                goto cleanup;
            }
            memory_slot = (uint32_t)j;
        }
        mode_memory = &next_memories[memory_slot];
        if (!hf_vg70_mode_step(mode_memory, item->id, item->entity, item->mode,
                               selected_slot, current_rpm, current_vector,
                               boot_sweep_done && prior < 0, &selection) ||
            !hf_vg70_weapon_data(item->weapon_row, sizeof(item->weapon_row), item->mode, weapon_after) ||
            !hf_vg70_projectile_weapon(item->projectile_row, sizeof(item->projectile_row),
                                       selection.projectile, selection.rates, projectile_after) ||
            !hf_vg70_magazine(item->magazine_row, sizeof(item->magazine_row), magazine_after) ||
            !hf_vg70_weapon_state(item->weapon_state_row, sizeof(item->weapon_state_row),
                                  item->mode, weapon_state_after) ||
            !hf_vg70_rate_state(item->rate_state_row, sizeof(item->rate_state_row),
                                selection.slot, selection.rates, rate_state_after) ||
            !hf_vg70_current_rpm_state(item->current_rpm_row, sizeof(item->current_rpm_row),
                                       selection.rpm, current_rpm_after) ||
            !hf_vg70_cadence(item->projectile_state_row, sizeof(item->projectile_state_row),
                             current_rpm, selection.rpm, projectile_state_after, &cadence_multiplier))
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 component or mode state differs from the accepted profile");
            goto cleanup;
        }
        (void)cadence_multiplier;
        if (!vg70_stage_bytes(ops, edits, edit_capacity, &edit_count, item->weapon,
                              item->weapon_row, weapon_after, sizeof(weapon_after), reason, reason_capacity) ||
            !vg70_stage_bytes(ops, edits, edit_capacity, &edit_count, item->projectile,
                              item->projectile_row, projectile_after, sizeof(projectile_after), reason, reason_capacity) ||
            !vg70_stage_bytes(ops, edits, edit_capacity, &edit_count, item->magazine,
                              item->magazine_row, magazine_after, sizeof(magazine_after), reason, reason_capacity) ||
            !vg70_stage_bytes(ops, edits, edit_capacity, &edit_count, item->weapon_state,
                              item->weapon_state_row, weapon_state_after, sizeof(weapon_state_after), reason, reason_capacity) ||
            !vg70_stage_bytes(ops, edits, edit_capacity, &edit_count, item->rate_state,
                              item->rate_state_row, rate_state_after, sizeof(rate_state_after), reason, reason_capacity) ||
            !vg70_stage_bytes(ops, edits, edit_capacity, &edit_count, item->current_rpm_state,
                              item->current_rpm_row, current_rpm_after, sizeof(current_rpm_after), reason, reason_capacity) ||
            !vg70_stage_bytes(ops, edits, edit_capacity, &edit_count, item->projectile_state,
                              item->projectile_state_row, projectile_state_after, sizeof(projectile_state_after), reason, reason_capacity))
            goto cleanup;
        memcpy(item->weapon_row, weapon_after, sizeof(item->weapon_row));
        memcpy(item->projectile_row, projectile_after, sizeof(item->projectile_row));
        memcpy(item->magazine_row, magazine_after, sizeof(item->magazine_row));
        memcpy(item->weapon_state_row, weapon_state_after, sizeof(item->weapon_state_row));
        memcpy(item->rate_state_row, rate_state_after, sizeof(item->rate_state_row));
        memcpy(item->current_rpm_row, current_rpm_after, sizeof(item->current_rpm_row));
        memcpy(item->projectile_state_row, projectile_state_after, sizeof(item->projectile_state_row));
    }

    for (i = 0u; i < edit_count; ++i)
    {
        if (!vg70_data_access(ops, edits[i].address, sizeof(edits[i].before)) ||
            !vg70_read(ops, edits[i].address, actual, sizeof(actual)) ||
            memcmp(actual, edits[i].before, sizeof(actual)) != 0)
        {
            vg70_set_reason(reason, reason_capacity, "VG-70 instance fields changed before commit");
            goto cleanup;
        }
    }
    for (i = 0u; i < edit_count; ++i)
    {
        attempted = i + 1u;
        if (ops->write(ops->context, edits[i].address, edits[i].after, sizeof(edits[i].after)) != 0 ||
            !vg70_read(ops, edits[i].address, actual, sizeof(actual)) ||
            memcmp(actual, edits[i].after, sizeof(actual)) != 0)
        {
            const int rollback_ok = vg70_rollback(ops, edits, attempted);
            vg70_set_reason(reason, reason_capacity, rollback_ok ?
                "VG-70 instance write failed; earlier fields were rolled back" :
                "FATAL: VG-70 instance write failed and rollback was incomplete");
            result = rollback_ok ? HF_VG70_REFUSED : HF_VG70_FATAL;
            goto cleanup;
        }
    }
    if (!hf_vg70_native_validate(ops, plan, next_instances, count))
    {
        const int rollback_ok = vg70_rollback(ops, edits, attempted);
        vg70_set_reason(reason, reason_capacity, rollback_ok ?
            "VG-70 instance verification failed; changes were rolled back" :
            "FATAL: VG-70 instance verification failed and rollback was incomplete");
        result = rollback_ok ? HF_VG70_REFUSED : HF_VG70_FATAL;
        goto cleanup;
    }

    if (count > 0u)
        memcpy(instances, next_instances, count * sizeof(*instances));
    if (memory_capacity > 0u)
        memcpy(memories, next_memories, memory_capacity * sizeof(*memories));
    if (changed_fields != NULL)
        *changed_fields = (uint32_t)edit_count;
    if (reason != NULL && reason_capacity > 0u)
        (void)snprintf(reason, reason_capacity, "Applied VG-70 mode and instance profiles (%u fields)",
                       (unsigned)edit_count);
    result = HF_VG70_APPLIED;

cleanup:
    free(next_instances);
    free(next_memories);
    free(edits);
    return result;
}
