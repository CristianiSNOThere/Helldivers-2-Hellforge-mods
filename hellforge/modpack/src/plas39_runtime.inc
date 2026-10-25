#include "plas39_runtime.h"

#include <stdio.h>
#include <string.h>

#include "plas39_source_guard.generated.h"

static uint32_t hf_plas39_read_u32(const uint8_t* bytes, size_t offset)
{
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1u] << 8) |
           ((uint32_t)bytes[offset + 2u] << 16) | ((uint32_t)bytes[offset + 3u] << 24);
}

static uint64_t hf_plas39_read_u64(const uint8_t* bytes, size_t offset)
{
    return (uint64_t)hf_plas39_read_u32(bytes, offset) |
           ((uint64_t)hf_plas39_read_u32(bytes, offset + 4u) << 32);
}

static void hf_plas39_write_u32(uint8_t* bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static int hf_plas39_address_add(uintptr_t base, uint64_t offset, uintptr_t* result)
{
    if (result == NULL || offset > (uint64_t)(UINTPTR_MAX - base))
        return 0;
    *result = base + (uintptr_t)offset;
    return *result >= (uintptr_t)65536u && *result < (uintptr_t)UINT64_C(0x800000000000);
}

static void hf_plas39_reason(char* reason, size_t capacity, const char* message)
{
    if (reason != NULL && capacity != 0u)
        (void)snprintf(reason, capacity, "%s", message != NULL ? message : "PLAS-39 transaction refused");
}

static int hf_plas39_ops_valid(const HfPlas39MemoryOps* ops)
{
    return ops != NULL && ops->read != NULL && ops->write != NULL &&
           ops->can_write != NULL && ops->data_access != NULL;
}

static int hf_plas39_read(const HfPlas39MemoryOps* ops, uintptr_t address,
                          void* output, size_t size)
{
    return hf_plas39_ops_valid(ops) && address >= (uintptr_t)65536u &&
           address < (uintptr_t)UINT64_C(0x800000000000) &&
           ops->read(ops->context, address, output, size);
}

static int hf_plas39_pointer(const uint8_t* bytes, size_t offset, uintptr_t* result)
{
    uint64_t value = hf_plas39_read_u64(bytes, offset);
    if (result == NULL || value < 65536u || value >= UINT64_C(0x800000000000))
        return 0;
    *result = (uintptr_t)value;
    return 1;
}

int hf_plas39_transaction_init(HfPlas39Transaction* transaction,
                               HfPlas39Edit* edits, size_t edit_capacity,
                               HfPlas39EditHashSlot* hash, size_t hash_capacity)
{
    if (transaction == NULL || edits == NULL || edit_capacity == 0u || hash == NULL ||
        hash_capacity < 2u || (hash_capacity & (hash_capacity - 1u)) != 0u ||
        edit_capacity * 2u > hash_capacity)
        return 0;
    memset(transaction, 0, sizeof(*transaction));
    transaction->edits = edits;
    transaction->edit_capacity = edit_capacity;
    transaction->hash = hash;
    transaction->hash_capacity = hash_capacity;
    transaction->generation = 1u;
    memset(hash, 0, hash_capacity * sizeof(*hash));
    return 1;
}

void hf_plas39_transaction_reset(HfPlas39Transaction* transaction)
{
    if (transaction == NULL || transaction->hash == NULL || transaction->hash_capacity == 0u)
        return;
    transaction->edit_count = 0u;
    ++transaction->generation;
    if (transaction->generation == 0u)
    {
        memset(transaction->hash, 0, transaction->hash_capacity * sizeof(*transaction->hash));
        transaction->generation = 1u;
    }
}

static size_t hf_plas39_hash_address(uintptr_t address, size_t capacity)
{
    uint64_t value = (uint64_t)(address >> 2u) * UINT64_C(11400714819323198485);
    return (size_t)(value & (uint64_t)(capacity - 1u));
}

static HfPlas39Edit* hf_plas39_find_edit(HfPlas39Transaction* transaction, uintptr_t address)
{
    size_t slot, probe;
    if (transaction == NULL || transaction->hash_capacity == 0u)
        return NULL;
    slot = hf_plas39_hash_address(address, transaction->hash_capacity);
    for (probe = 0u; probe < transaction->hash_capacity; ++probe)
    {
        HfPlas39EditHashSlot* entry = &transaction->hash[(slot + probe) & (transaction->hash_capacity - 1u)];
        if (entry->generation != transaction->generation)
            return NULL;
        if (entry->address == address && entry->index_plus_one != 0u &&
            (size_t)(entry->index_plus_one - 1u) < transaction->edit_count)
            return &transaction->edits[entry->index_plus_one - 1u];
    }
    return NULL;
}

static int hf_plas39_stage(HfPlas39Transaction* transaction, uintptr_t address,
                           const uint8_t before[4], const uint8_t after[4],
                           int clone, int merge_clone)
{
    size_t slot, probe;
    HfPlas39Edit* existing;
    if (transaction == NULL || before == NULL || after == NULL || address < (uintptr_t)65536u ||
        address >= (uintptr_t)UINT64_C(0x800000000000) || address > UINTPTR_MAX - 4u)
        return 0;
    existing = hf_plas39_find_edit(transaction, address);
    if (existing != NULL)
    {
        if (existing->clone && merge_clone && memcmp(existing->after, before, 4u) == 0)
        {
            memcpy(existing->after, after, 4u);
            return 1;
        }
        return !existing->clone && !clone && memcmp(existing->before, before, 4u) == 0 &&
               memcmp(existing->after, after, 4u) == 0;
    }
    if (transaction->edit_count >= transaction->edit_capacity ||
        transaction->edit_count >= UINT32_MAX - 1u)
        return 0;
    slot = hf_plas39_hash_address(address, transaction->hash_capacity);
    for (probe = 0u; probe < transaction->hash_capacity; ++probe)
    {
        HfPlas39EditHashSlot* entry = &transaction->hash[(slot + probe) & (transaction->hash_capacity - 1u)];
        if (entry->generation != transaction->generation)
        {
            HfPlas39Edit* edit = &transaction->edits[transaction->edit_count];
            memset(edit, 0, sizeof(*edit));
            edit->address = address;
            memcpy(edit->before, before, 4u);
            memcpy(edit->after, after, 4u);
            edit->clone = clone != 0;
            entry->address = address;
            entry->index_plus_one = (uint32_t)(transaction->edit_count + 1u);
            entry->generation = transaction->generation;
            ++transaction->edit_count;
            return 1;
        }
    }
    return 0;
}

static int hf_plas39_read_planned(const HfPlas39MemoryOps* ops,
                                 HfPlas39Transaction* transaction,
                                 uintptr_t address, uint8_t output[8])
{
    uint8_t actual[8];
    HfPlas39Edit* first = hf_plas39_find_edit(transaction, address);
    HfPlas39Edit* second = hf_plas39_find_edit(transaction, address + 4u);
    if (!hf_plas39_read(ops, address, actual, sizeof(actual)))
        return 0;
    if (first != NULL)
        memcpy(actual, first->after, 4u);
    if (second != NULL)
        memcpy(actual + 4u, second->after, 4u);
    memcpy(output, actual, sizeof(actual));
    return 1;
}

static int hf_plas39_rollback(const HfPlas39MemoryOps* ops,
                              HfPlas39Transaction* transaction, size_t attempted)
{
    int restored = 1;
    while (attempted > 0u)
    {
        HfPlas39Edit* edit = &transaction->edits[--attempted];
        uint8_t current[4];
        if (!edit->attempted)
            continue;
        if (!hf_plas39_read(ops, edit->address, current, sizeof(current)))
        {
            restored = 0;
            continue;
        }
        if (memcmp(current, edit->after, sizeof(current)) == 0)
        {
            if (!ops->write(ops->context, edit->address, edit->before, sizeof(edit->before), 1) ||
                !hf_plas39_read(ops, edit->address, current, sizeof(current)) ||
                memcmp(current, edit->before, sizeof(current)) != 0)
                restored = 0;
        }
        else if (memcmp(current, edit->before, sizeof(current)) != 0)
            restored = 0;
    }
    return restored;
}

HfPlas39ApplyResult hf_plas39_transaction_apply(const HfPlas39MemoryOps* ops,
                                                 HfPlas39Transaction* transaction,
                                                 uint32_t* changed,
                                                 char* reason, size_t reason_capacity)
{
    size_t i, attempted = 0u;
    uint32_t writes = 0u;
    if (changed != NULL)
        *changed = 0u;
    if (!hf_plas39_ops_valid(ops) || transaction == NULL || transaction->edits == NULL)
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 memory operations are unavailable");
        return HF_PLAS39_REFUSED;
    }
    for (i = 0u; i < transaction->edit_count; ++i)
    {
        HfPlas39Edit* edit = &transaction->edits[i];
        uint8_t current[4];
        if (!ops->data_access(ops->context, edit->address, sizeof(current)) ||
            !ops->can_write(ops->context, edit->address, sizeof(current), 1) ||
            !hf_plas39_read(ops, edit->address, current, sizeof(current)) ||
            memcmp(current, edit->before, sizeof(current)) != 0)
        {
            hf_plas39_reason(reason, reason_capacity,
                             "PLAS-39 target changed or became unwritable before mutation");
            return HF_PLAS39_REFUSED;
        }
    }
    for (i = 0u; i < transaction->edit_count; ++i)
    {
        HfPlas39Edit* edit = &transaction->edits[i];
        uint8_t current[4];
        if (memcmp(edit->before, edit->after, sizeof(current)) == 0)
            continue;
        edit->attempted = 1u;
        attempted = i + 1u;
        if (!ops->write(ops->context, edit->address, edit->after, sizeof(edit->after), 1) ||
            !hf_plas39_read(ops, edit->address, current, sizeof(current)) ||
            memcmp(current, edit->after, sizeof(current)) != 0)
        {
            if (!hf_plas39_rollback(ops, transaction, attempted))
            {
                hf_plas39_reason(reason, reason_capacity,
                                 "FATAL: PLAS-39 transaction rollback was incomplete");
                return HF_PLAS39_FATAL;
            }
            hf_plas39_reason(reason, reason_capacity,
                             "PLAS-39 write failed; earlier edits were restored");
            return HF_PLAS39_REFUSED;
        }
        ++writes;
    }
    if (changed != NULL)
        *changed = writes;
    return HF_PLAS39_APPLIED;
}

static HfPlas39ApplyResult hf_plas39_group_record(const HfPlas39MemoryOps* ops,
                                                   uintptr_t address, size_t total,
                                                   const uint8_t expected_header[28],
                                                   uint32_t map_count, uint32_t map_index,
                                                   uint32_t record_index, uint32_t stride,
                                                   uintptr_t* record_address,
                                                   uint8_t* row, size_t row_size,
                                                   const char* label,
                                                   char* reason, size_t reason_capacity)
{
    uintptr_t map_address, row_address;
    uint8_t header[28], mapping[16];
    uint64_t offset;
    if (address == 0u || total < sizeof(header) || row_size > stride ||
        map_index >= map_count || record_index > total / stride ||
        !ops->data_access(ops->context, address, total))
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 component group is not supported private data");
        return HF_PLAS39_REFUSED;
    }
    if (!hf_plas39_read(ops, address, header, sizeof(header)))
        return HF_PLAS39_NOT_READY;
    if (memcmp(header, expected_header, sizeof(header)) != 0)
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 component group header changed");
        return HF_PLAS39_REFUSED;
    }
    offset = 28u + (uint64_t)map_count * 16u + (uint64_t)record_index * stride;
    if (offset > total || stride > total - offset || !hf_plas39_address_add(address, offset, &row_address) ||
        !hf_plas39_address_add(address, 28u + (uint64_t)map_index * 16u, &map_address) ||
        !hf_plas39_read(ops, map_address, mapping, sizeof(mapping)))
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 component map or row is unreadable");
        return HF_PLAS39_NOT_READY;
    }
    if (memcmp(mapping, HF_PLAS39_OWNER, 8u) != 0 ||
        hf_plas39_read_u32(mapping, 8u) != record_index)
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 component resource owner/index changed");
        return HF_PLAS39_REFUSED;
    }
    if (!hf_plas39_read(ops, row_address, row, row_size))
        return HF_PLAS39_NOT_READY;
    if (record_address != NULL)
        *record_address = row_address;
    (void)label;
    return HF_PLAS39_APPLIED;
}

static HfPlas39ApplyResult hf_plas39_stage_profile_fields(HfPlas39Transaction* transaction,
                                                          uintptr_t address,
                                                          const uint8_t* before,
                                                          const uint8_t* after,
                                                          const uint32_t* offsets,
                                                          size_t offset_count,
                                                          int clone, int merge_clone)
{
    size_t i;
    for (i = 0u; i < offset_count; ++i)
        if (!hf_plas39_stage(transaction, address + offsets[i], before + offsets[i],
                             after + offsets[i], clone, merge_clone))
            return HF_PLAS39_REFUSED;
    return HF_PLAS39_APPLIED;
}

HfPlas39ApplyResult hf_plas39_native_prepare(const HfPlas39MemoryOps* ops, uintptr_t game,
                                              HfPlas39Transaction* transaction,
                                              HfPlas39Plan* plan, uint32_t* changed,
                                              char* reason, size_t reason_capacity)
{
    uintptr_t owner_field, weapon_field, charge_field, projectile_field;
    uintptr_t owner, weapon_map, charge_map, projectile_map;
    uintptr_t weapon_group, charge_group, weapon_address, charge_address;
    HfPlas39Plan staged_plan;
    uint8_t weapon_row[HF_PLAS39_WEAPON_PROFILE_SIZE];
    uint8_t weapon_after[HF_PLAS39_WEAPON_PROFILE_SIZE];
    uint8_t charge_row[HF_PLAS39_CHARGE_PROFILE_SIZE];
    uint8_t mapping[16];
    static const uint32_t weapon_offsets[] = {140u, 144u, 148u, 184u, 188u};
    uint8_t pointer_bytes[8];
    HfPlas39ApplyResult result;
    if (changed != NULL)
        *changed = 0u;
    if (!hf_plas39_ops_valid(ops) || transaction == NULL || plan == NULL || game == 0u ||
        !hf_plas39_address_add(game, UINT64_C(0x346bf98), &owner_field) ||
        !hf_plas39_address_add(game, UINT64_C(0x346bf98) + 8u, &weapon_field))
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 native memory layout is unavailable");
        return HF_PLAS39_REFUSED;
    }
    if (!hf_plas39_read(ops, owner_field, pointer_bytes, sizeof(pointer_bytes)) ||
        !hf_plas39_pointer(pointer_bytes, 0u, &owner) ||
        !hf_plas39_address_add(owner, UINT64_C(0xf12bd8), &weapon_field) ||
        !hf_plas39_address_add(owner, UINT64_C(0xf12ad8), &charge_field) ||
        !hf_plas39_address_add(owner, UINT64_C(0xf12e80), &projectile_field))
        return HF_PLAS39_NOT_READY;
    if (!hf_plas39_read(ops, weapon_field, pointer_bytes, sizeof(pointer_bytes)) ||
        !hf_plas39_pointer(pointer_bytes, 0u, &weapon_map) || weapon_map < 28u ||
        !hf_plas39_read(ops, charge_field, pointer_bytes, sizeof(pointer_bytes)) ||
        !hf_plas39_pointer(pointer_bytes, 0u, &charge_map) || charge_map < 28u ||
        !hf_plas39_read(ops, projectile_field, pointer_bytes, sizeof(pointer_bytes)) ||
        !hf_plas39_pointer(pointer_bytes, 0u, &projectile_map))
        return HF_PLAS39_NOT_READY;
    weapon_group = weapon_map - 28u;
    charge_group = charge_map - 28u;
    hf_plas39_transaction_reset(transaction);
    result = hf_plas39_group_record(ops, weapon_group, HF_PLAS39_WEAPON_TOTAL,
                                     HF_PLAS39_WEAPON_HEADER, HF_PLAS39_WEAPON_MAP_COUNT,
                                     HF_PLAS39_WEAPON_MAP_INDEX, HF_PLAS39_WEAPON_RECORD_INDEX,
                                     HF_PLAS39_WEAPON_STRIDE, &weapon_address, weapon_row,
                                     sizeof(weapon_row), "WeaponDataComponent", reason, reason_capacity);
    if (result != HF_PLAS39_APPLIED)
        return result;
    result = hf_plas39_group_record(ops, charge_group, HF_PLAS39_CHARGE_TOTAL,
                                     HF_PLAS39_CHARGE_HEADER, HF_PLAS39_CHARGE_MAP_COUNT,
                                     HF_PLAS39_CHARGE_MAP_INDEX, HF_PLAS39_CHARGE_RECORD_INDEX,
                                     HF_PLAS39_CHARGE_STRIDE, &charge_address, charge_row,
                                     sizeof(charge_row), "WeaponChargeComponent", reason, reason_capacity);
    if (result != HF_PLAS39_APPLIED)
        return result;
    if (hf_plas39_read_u32(weapon_row, 140u) != 3u ||
        memcmp(charge_row, HF_PLAS39_CHARGE_STOCK, sizeof(charge_row)) != 0)
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 stock weapon/charge profile changed");
        return HF_PLAS39_REFUSED;
    }
    if (!hf_plas39_address_add(projectile_map, 290u * 16u, &projectile_field) ||
        !hf_plas39_read(ops, projectile_field, mapping, sizeof(mapping)))
        return HF_PLAS39_NOT_READY;
    if (memcmp(mapping, HF_PLAS39_OWNER, 8u) != 0 || hf_plas39_read_u32(mapping, 8u) != 136u)
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 projectile resource owner/index changed");
        return HF_PLAS39_REFUSED;
    }
    if (!hf_plas39_address_add(projectile_map, (uint64_t)542u * 16u + (uint64_t)136u * 616u,
                                &projectile_field))
        return HF_PLAS39_REFUSED;
    if (!hf_plas39_read(ops, weapon_address, weapon_after, sizeof(weapon_after)) ||
        !hf_plas39_weapon_profile(weapon_after, sizeof(weapon_after), weapon_row))
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 weapon mode fields changed");
        return HF_PLAS39_REFUSED;
    }
    if (hf_plas39_stage_profile_fields(transaction, weapon_address, weapon_after, weapon_row,
                                       weapon_offsets, sizeof(weapon_offsets) / sizeof(weapon_offsets[0]),
                                       0, 0) != HF_PLAS39_APPLIED)
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 global weapon edits overlap");
        return HF_PLAS39_REFUSED;
    }
    result = hf_plas39_transaction_apply(ops, transaction, changed, reason, reason_capacity);
    if (result != HF_PLAS39_APPLIED)
        return result;
    memset(&staged_plan, 0, sizeof(staged_plan));
    staged_plan.game = game;
    staged_plan.owner = owner;
    staged_plan.weapon_group = weapon_group;
    staged_plan.charge_group = charge_group;
    staged_plan.projectile_map = projectile_map;
    staged_plan.weapon_address = weapon_address;
    staged_plan.charge_address = charge_address;
    staged_plan.projectile_address = projectile_field;
    *plan = staged_plan;
    return HF_PLAS39_APPLIED;
}

static HfPlas39ApplyResult hf_plas39_lookup(const HfPlas39MemoryOps* ops,
                                             const uint8_t* header, size_t offset,
                                             uint32_t key, uint32_t* index)
{
    uint32_t capacity = hf_plas39_read_u32(header, offset + 8u);
    uint32_t empty = hf_plas39_read_u32(header, offset + 12u);
    uint32_t multiplier = hf_plas39_read_u32(header, offset + 16u);
    uintptr_t array;
    uint32_t product, probe, limit;
    uint8_t pointer_bytes[8], row[8];
    if (capacity > 4096u || (capacity != 0u && (capacity & (capacity - 1u)) != 0u))
        return HF_PLAS39_REFUSED;
    if (capacity == 0u)
        return HF_PLAS39_NOT_FOUND;
    if (!hf_plas39_pointer(header, offset, &array))
        return HF_PLAS39_NOT_READY;
    (void)pointer_bytes;
    product = (uint32_t)((uint64_t)key * multiplier);
    limit = capacity < 64u ? capacity : 64u;
    for (probe = 0u; probe < limit; ++probe)
    {
        uintptr_t address;
        uint32_t identity, found_index;
        if (!hf_plas39_address_add(array, ((product + probe) & (capacity - 1u)) * 8u, &address) ||
            !hf_plas39_read(ops, address, row, sizeof(row)))
            return HF_PLAS39_NOT_READY;
        identity = hf_plas39_read_u32(row, 0u);
        found_index = hf_plas39_read_u32(row, 4u);
        if (identity == key)
        {
            if (found_index == UINT32_MAX)
                return HF_PLAS39_NOT_FOUND;
            if (index != NULL)
                *index = found_index;
            return HF_PLAS39_APPLIED;
        }
        if (identity == empty)
            return HF_PLAS39_NOT_FOUND;
    }
    return HF_PLAS39_REFUSED;
}

HfPlas39ApplyResult hf_plas39_native_discover(const HfPlas39MemoryOps* ops,
                                               HfPlas39Plan* plan,
                                               HfPlas39Instance* instances,
                                               size_t capacity, size_t* count,
                                               char* reason, size_t reason_capacity)
{
    uintptr_t owner_field, owner, data_manager, charge_manager, projectile_manager;
    uintptr_t array, modes, references, charge_states, projectile_states;
    uint8_t pointer_bytes[8], h[HF_PLAS39_DATA_HEADER_SIZE], ch[HF_PLAS39_CHARGE_HEADER_SIZE];
    uint8_t ph[HF_PLAS39_PROJECTILE_HEADER_SIZE], owner_bytes[8];
    uint8_t manager_reference[8], identity[24];
    uint32_t data_count, owned, slot;
    size_t found = 0u;
    HfPlas39Snapshot snapshot;
    if (count != NULL)
        *count = 0u;
    if (!hf_plas39_ops_valid(ops) || plan == NULL || instances == NULL || count == NULL ||
        capacity == 0u || capacity > HF_PLAS39_MAX_INSTANCES)
        return HF_PLAS39_REFUSED;
    if (!hf_plas39_address_add(plan->game, UINT64_C(0x346bf98), &owner_field) ||
        !hf_plas39_read(ops, owner_field, pointer_bytes, sizeof(pointer_bytes)) ||
        !hf_plas39_pointer(pointer_bytes, 0u, &owner))
        return HF_PLAS39_NOT_READY;
    if (owner != plan->owner)
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 native entity owner changed; restart required");
        return HF_PLAS39_REFUSED;
    }
    if (!hf_plas39_address_add(plan->game, UINT64_C(0x3326ce0), &data_manager) ||
        !hf_plas39_read(ops, data_manager, pointer_bytes, sizeof(pointer_bytes)) ||
        !hf_plas39_pointer(pointer_bytes, 0u, &snapshot.data_manager) ||
        !hf_plas39_address_add(plan->game, UINT64_C(0x3326c20), &charge_manager) ||
        !hf_plas39_read(ops, charge_manager, pointer_bytes, sizeof(pointer_bytes)) ||
        !hf_plas39_pointer(pointer_bytes, 0u, &snapshot.charge_manager) ||
        !hf_plas39_address_add(plan->game, UINT64_C(0x33266d8), &projectile_manager) ||
        !hf_plas39_read(ops, projectile_manager, pointer_bytes, sizeof(pointer_bytes)) ||
        !hf_plas39_pointer(pointer_bytes, 0u, &snapshot.projectile_manager) ||
        !hf_plas39_read(ops, snapshot.data_manager, h, sizeof(h)) ||
        !hf_plas39_read(ops, snapshot.charge_manager, ch, sizeof(ch)) ||
        !hf_plas39_read(ops, snapshot.projectile_manager, ph, sizeof(ph)))
        return HF_PLAS39_NOT_READY;
    if (!hf_plas39_address_add(plan->owner, UINT64_C(0xf12e80), &data_manager) ||
        !hf_plas39_read(ops, data_manager, pointer_bytes, sizeof(pointer_bytes)) ||
        !hf_plas39_pointer(pointer_bytes, 0u, &data_manager) || data_manager != plan->projectile_map)
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 native projectile table changed");
        return HF_PLAS39_REFUSED;
    }
    data_count = hf_plas39_read_u32(h, 0x1cu);
    owned = hf_plas39_read_u32(ch, 0x10u);
    if (data_count > HF_PLAS39_MAX_INSTANCES || data_count > hf_plas39_read_u32(h, 0x10u) ||
        owned > hf_plas39_read_u32(ch, 0x0cu) || hf_plas39_read_u32(ch, 0x0cu) > HF_PLAS39_MAX_INSTANCES)
    {
        hf_plas39_reason(reason, reason_capacity, "PLAS-39 native instance count changed");
        return HF_PLAS39_REFUSED;
    }
    if (!hf_plas39_pointer(h, 0x48u, &array) || !hf_plas39_pointer(h, 0x60u, &modes) ||
        (owned != 0u && !hf_plas39_pointer(ch, 0x38u, &references)) ||
        !hf_plas39_pointer(ch, 0x40u, &charge_states) ||
        !hf_plas39_pointer(ph, 0x68u, &projectile_states))
        return HF_PLAS39_NOT_READY;
    for (slot = 0u; slot < owned; ++slot)
    {
        uintptr_t reference_address, entity, mode_address;
        uint32_t id, data_index, mode, weapon_index, charge_index, projectile_index;
        uint32_t projectile_state_index, charge_state_index;
        uintptr_t weapon, charge, projectile, projectile_state, charge_state = 0u;
        int weapon_cached, charge_cached, projectile_cached;
        HfPlas39ApplyResult lookup_result;
        if (!hf_plas39_address_add(references, (uint64_t)slot * 8u, &reference_address) ||
            !hf_plas39_read(ops, reference_address, owner_bytes, sizeof(owner_bytes)) ||
            !hf_plas39_pointer(owner_bytes, 0u, &entity) ||
            !hf_plas39_read(ops, entity, identity, sizeof(identity)))
            return HF_PLAS39_NOT_READY;
        if (memcmp(identity, HF_PLAS39_OWNER, sizeof(owner_bytes)) != 0 ||
            (hf_plas39_read_u32(identity, 20u) & 1u) == 0u)
            continue;
        id = hf_plas39_read_u32(identity, 8u);
        lookup_result = hf_plas39_lookup(ops, h, 0x30u, id, &data_index);
        if (lookup_result != HF_PLAS39_APPLIED || data_index >= data_count)
        {
            hf_plas39_reason(reason, reason_capacity, "PLAS-39 native weapon entity index changed");
            return lookup_result == HF_PLAS39_NOT_READY ? HF_PLAS39_RETRY_RACE : HF_PLAS39_REFUSED;
        }
        if (!hf_plas39_address_add(array, (uint64_t)data_index * 8u, &mode_address) ||
            !hf_plas39_read(ops, mode_address, manager_reference, sizeof(manager_reference)) ||
            memcmp(manager_reference, owner_bytes, sizeof(owner_bytes)) != 0 ||
            !hf_plas39_address_add(modes, (uint64_t)data_index * 12u, &mode_address) ||
            !hf_plas39_read(ops, mode_address, pointer_bytes, 4u))
        {
            hf_plas39_reason(reason, reason_capacity, "PLAS-39 native firing owner changed");
            return HF_PLAS39_RETRY_RACE;
        }
        mode = hf_plas39_read_u32(pointer_bytes, 0u);
        if (mode != HF_PLAS39_MODE_SEMI && mode != HF_PLAS39_MODE_BURST)
            continue;

        weapon = plan->weapon_address;
        lookup_result = hf_plas39_lookup(ops, h, 0x70u, id, &weapon_index);
        weapon_cached = lookup_result == HF_PLAS39_APPLIED;
        if (lookup_result != HF_PLAS39_APPLIED && lookup_result != HF_PLAS39_NOT_FOUND)
            return lookup_result == HF_PLAS39_NOT_READY ? HF_PLAS39_RETRY_RACE : HF_PLAS39_REFUSED;
        if (weapon_cached)
        {
            if (weapon_index >= hf_plas39_read_u32(h, 0xa8u) ||
                !hf_plas39_pointer(h, 0xb0u, &weapon) ||
                !hf_plas39_address_add(weapon, (uint64_t)weapon_index * HF_PLAS39_WEAPON_STRIDE, &weapon))
                return HF_PLAS39_REFUSED;
        }

        charge = plan->charge_address;
        lookup_result = hf_plas39_lookup(ops, ch, 0x50u, id, &charge_index);
        charge_cached = lookup_result == HF_PLAS39_APPLIED;
        if (lookup_result != HF_PLAS39_APPLIED && lookup_result != HF_PLAS39_NOT_FOUND)
            return lookup_result == HF_PLAS39_NOT_READY ? HF_PLAS39_RETRY_RACE : HF_PLAS39_REFUSED;
        if (charge_cached)
        {
            if (charge_index >= hf_plas39_read_u32(ch, 0x88u) ||
                !hf_plas39_pointer(ch, 0x90u, &charge) ||
                !hf_plas39_address_add(charge, (uint64_t)charge_index * HF_PLAS39_CHARGE_PROFILE_SIZE, &charge))
                return HF_PLAS39_REFUSED;
        }

        projectile = plan->projectile_address;
        lookup_result = hf_plas39_lookup(ops, ph, 0x90u, id, &projectile_index);
        projectile_cached = lookup_result == HF_PLAS39_APPLIED;
        if (lookup_result != HF_PLAS39_APPLIED && lookup_result != HF_PLAS39_NOT_FOUND)
            return lookup_result == HF_PLAS39_NOT_READY ? HF_PLAS39_RETRY_RACE : HF_PLAS39_REFUSED;
        if (projectile_cached)
        {
            if (projectile_index >= hf_plas39_read_u32(ph, 0xc8u) || projectile_index >= HF_PLAS39_MAX_INSTANCES ||
                !hf_plas39_pointer(ph, 0xd0u, &projectile) ||
                !hf_plas39_address_add(projectile, (uint64_t)projectile_index * HF_PLAS39_PROJECTILE_STRIDE,
                                        &projectile))
                return HF_PLAS39_REFUSED;
        }

        lookup_result = hf_plas39_lookup(ops, ph, 0x50u, id, &projectile_state_index);
        if (lookup_result != HF_PLAS39_APPLIED || projectile_state_index >= hf_plas39_read_u32(ph, 0x38u) ||
            projectile_state_index >= HF_PLAS39_MAX_INSTANCES ||
            !hf_plas39_address_add(projectile_states, (uint64_t)projectile_state_index * 8u, &reference_address) ||
            !hf_plas39_read(ops, reference_address, manager_reference, sizeof(manager_reference)) ||
            memcmp(manager_reference, owner_bytes, sizeof(owner_bytes)) != 0 ||
            !hf_plas39_pointer(ph, 0x78u, &projectile_state) ||
            !hf_plas39_address_add(projectile_state, (uint64_t)projectile_state_index * 168u, &projectile_state))
        {
            hf_plas39_reason(reason, reason_capacity, "PLAS-39 projectile state owner/index changed");
            return lookup_result == HF_PLAS39_NOT_READY ? HF_PLAS39_RETRY_RACE : HF_PLAS39_REFUSED;
        }

        charge_state_index = UINT32_MAX;
        lookup_result = hf_plas39_lookup(ops, ch, 0x20u, id, &charge_state_index);
        if (lookup_result == HF_PLAS39_APPLIED)
        {
            if (charge_state_index != slot ||
                !hf_plas39_address_add(references, (uint64_t)charge_state_index * 8u, &reference_address) ||
                !hf_plas39_read(ops, reference_address, manager_reference, sizeof(manager_reference)) ||
                memcmp(manager_reference, owner_bytes, sizeof(owner_bytes)) != 0 ||
                !hf_plas39_address_add(charge_states, (uint64_t)charge_state_index * 40u, &charge_state))
            {
                hf_plas39_reason(reason, reason_capacity, "PLAS-39 charge state owner/index changed");
                return HF_PLAS39_RETRY_RACE;
            }
        }
        else if (lookup_result != HF_PLAS39_NOT_FOUND)
            return lookup_result == HF_PLAS39_NOT_READY ? HF_PLAS39_RETRY_RACE : HF_PLAS39_REFUSED;
        if (charge_state == 0u || charge_state_index >= owned)
            continue;
        if (found >= capacity)
        {
            hf_plas39_reason(reason, reason_capacity, "PLAS-39 live weapon count exceeds its bound");
            return HF_PLAS39_REFUSED;
        }
        memset(&instances[found], 0, sizeof(instances[found]));
        instances[found].id = id;
        instances[found].mode = mode;
        instances[found].entity = entity;
        instances[found].mode_address = mode_address;
        instances[found].weapon = weapon;
        instances[found].charge = charge;
        instances[found].charge_state = charge_state;
        instances[found].projectile = projectile;
        instances[found].projectile_state = projectile_state;
        instances[found].charge_cached = (uint32_t)charge_cached;
        instances[found].projectile_cached = (uint32_t)projectile_cached;
        memcpy(instances[found].identity, identity, sizeof(identity));
        if (!hf_plas39_read(ops, charge, instances[found].charge_row,
                            sizeof(instances[found].charge_row)) ||
            !hf_plas39_read(ops, projectile, instances[found].projectile_row,
                            sizeof(instances[found].projectile_row)))
            return HF_PLAS39_NOT_READY;
        ++found;
    }
    if (!hf_plas39_read(ops, snapshot.data_manager + 0x48u, pointer_bytes, sizeof(pointer_bytes)) ||
        memcmp(pointer_bytes, h + 0x48u, sizeof(pointer_bytes)) != 0 ||
        !hf_plas39_read(ops, snapshot.data_manager + 0x60u, pointer_bytes, sizeof(pointer_bytes)) ||
        memcmp(pointer_bytes, h + 0x60u, sizeof(pointer_bytes)) != 0)
        return HF_PLAS39_RETRY_RACE;
    memcpy(snapshot.data_header, h, sizeof(h));
    memcpy(snapshot.charge_header, ch, sizeof(ch));
    memcpy(snapshot.projectile_header, ph, sizeof(ph));
    plan->snapshot = snapshot;
    *count = found;
    (void)owner_bytes;
    return HF_PLAS39_APPLIED;
}

HfPlas39ApplyResult hf_plas39_native_validate(const HfPlas39MemoryOps* ops,
                                               const HfPlas39Plan* plan,
                                               const HfPlas39Instance* instances,
                                               size_t count)
{
    size_t i;
    uint8_t current[HF_PLAS39_PROJECTILE_HEADER_SIZE];
    if (!hf_plas39_ops_valid(ops) || plan == NULL || (count != 0u && instances == NULL))
        return HF_PLAS39_REFUSED;
    if (!hf_plas39_read(ops, plan->snapshot.data_manager, current, sizeof(plan->snapshot.data_header)) ||
        memcmp(current, plan->snapshot.data_header, sizeof(plan->snapshot.data_header)) != 0 ||
        !hf_plas39_read(ops, plan->snapshot.charge_manager, current, sizeof(plan->snapshot.charge_header)) ||
        memcmp(current, plan->snapshot.charge_header, sizeof(plan->snapshot.charge_header)) != 0 ||
        !hf_plas39_read(ops, plan->snapshot.projectile_manager, current,
                        sizeof(plan->snapshot.projectile_header)) ||
        memcmp(current, plan->snapshot.projectile_header,
               sizeof(plan->snapshot.projectile_header)) != 0)
        return HF_PLAS39_RETRY_RACE;
    for (i = 0u; i < count; ++i)
    {
        uint8_t identity[24], mode[4];
        if (!hf_plas39_read(ops, instances[i].entity, identity, sizeof(identity)) ||
            memcmp(identity, instances[i].identity, sizeof(identity)) != 0 ||
            !hf_plas39_read(ops, instances[i].mode_address, mode, sizeof(mode)) ||
            hf_plas39_read_u32(mode, 0u) != instances[i].mode)
            return HF_PLAS39_RETRY_RACE;
    }
    return HF_PLAS39_APPLIED;
}

static HfPlas39ApplyResult hf_plas39_stage_clone(const HfPlas39MemoryOps* ops,
                                                  HfPlas39Transaction* transaction,
                                                  uintptr_t target, const uint8_t* source,
                                                  size_t size)
{
    size_t offset;
    if (source == NULL || (size & 3u) != 0u ||
        !ops->data_access(ops->context, target, size))
        return HF_PLAS39_REFUSED;
    for (offset = 0u; offset < size; offset += 4u)
    {
        uint8_t before[4];
        if (!hf_plas39_read(ops, target + offset, before, sizeof(before)) ||
            !hf_plas39_stage(transaction, target + offset, before, source + offset, 1, 0))
            return HF_PLAS39_REFUSED;
    }
    return HF_PLAS39_APPLIED;
}

static HfPlas39ApplyResult hf_plas39_reserve_map_slot(const HfPlas39MemoryOps* ops,
                                                       const uint8_t* header, size_t offset,
                                                       uint32_t key, HfPlas39Transaction* transaction,
                                                       uintptr_t* output)
{
    uint32_t capacity = hf_plas39_read_u32(header, offset + 8u);
    uint32_t empty = hf_plas39_read_u32(header, offset + 12u);
    uint32_t multiplier = hf_plas39_read_u32(header, offset + 16u);
    uint32_t product, probe, limit;
    uintptr_t array, tombstone = 0u;
    if (capacity == 0u || capacity > 4096u ||
        (capacity & (capacity - 1u)) != 0u ||
        !hf_plas39_pointer(header, offset, &array))
        return HF_PLAS39_REFUSED;
    product = (uint32_t)((uint64_t)key * multiplier);
    limit = capacity < 64u ? capacity : 64u;
    for (probe = 0u; probe < limit; ++probe)
    {
        uintptr_t address;
        uint8_t row[8];
        uint32_t identity, index;
        if (!hf_plas39_address_add(array, ((product + probe) & (capacity - 1u)) * 8u, &address) ||
            !hf_plas39_read_planned(ops, transaction, address, row))
            return HF_PLAS39_NOT_READY;
        identity = hf_plas39_read_u32(row, 0u);
        index = hf_plas39_read_u32(row, 4u);
        if (identity == key && index != UINT32_MAX)
            return HF_PLAS39_REFUSED;
        if (index == UINT32_MAX && tombstone == 0u)
            tombstone = address;
        if (identity == empty || identity == key)
        {
            *output = tombstone != 0u ? tombstone : address;
            return HF_PLAS39_APPLIED;
        }
    }
    if (capacity <= 64u && tombstone != 0u)
    {
        *output = tombstone;
        return HF_PLAS39_APPLIED;
    }
    return HF_PLAS39_REFUSED;
}

typedef struct HfPlas39ForwardPublication
{
    uintptr_t address;
    uint32_t index, id;
} HfPlas39ForwardPublication;

static HfPlas39ApplyResult hf_plas39_isolate_manager(const HfPlas39MemoryOps* ops,
                                                      HfPlas39Plan* plan,
                                                      HfPlas39Transaction* transaction,
                                                      HfPlas39Instance* instances,
                                                      size_t count, int charge)
{
    const uint8_t* header = charge ? plan->snapshot.charge_header : plan->snapshot.projectile_header;
    uintptr_t manager = charge ? plan->snapshot.charge_manager : plan->snapshot.projectile_manager;
    size_t capacity_offset = charge ? 4u : 0x30u;
    size_t count_offset = charge ? 0x88u : 0xc8u;
    size_t forward_offset = charge ? 0x50u : 0x90u;
    size_t reverse_offset = charge ? 0x70u : 0xb0u;
    size_t rows_offset = charge ? 0x90u : 0xd0u;
    size_t stride = charge ? HF_PLAS39_CHARGE_PROFILE_SIZE : HF_PLAS39_PROJECTILE_STRIDE;
    uint32_t live_count = hf_plas39_read_u32(header, count_offset);
    uint32_t row_capacity = hf_plas39_read_u32(header, capacity_offset);
    uint32_t next_index = live_count;
    uintptr_t rows;
    HfPlas39ForwardPublication publications[HF_PLAS39_MAX_INSTANCES];
    size_t publication_count = 0u, i;
    uint8_t count_before[4], count_after[4];
    if (live_count > row_capacity || row_capacity > HF_PLAS39_MAX_INSTANCES ||
        !hf_plas39_pointer(header, rows_offset, &rows))
        return HF_PLAS39_REFUSED;
    for (i = 0u; i < count; ++i)
    {
        HfPlas39Instance* item = &instances[i];
        uint32_t cached = charge ? item->charge_cached : item->projectile_cached;
        uint8_t* source = charge ? item->charge_row : item->projectile_row;
        uintptr_t forward, reverse, target;
        uint8_t reverse_before[8], word_before[4], word_after[4];
        HfPlas39ApplyResult result;
        if (cached)
            continue;
        if (next_index >= row_capacity || publication_count >= HF_PLAS39_MAX_INSTANCES ||
            !hf_plas39_address_add(rows, (uint64_t)next_index * stride, &target) ||
            !ops->data_access(ops->context, target, stride))
            return HF_PLAS39_REFUSED;
        result = hf_plas39_reserve_map_slot(ops, header, forward_offset, item->id,
                                             transaction, &forward);
        if (result != HF_PLAS39_APPLIED)
            return result;
        result = hf_plas39_reserve_map_slot(ops, header, reverse_offset, next_index,
                                             transaction, &reverse);
        if (result != HF_PLAS39_APPLIED)
            return result;
        result = hf_plas39_stage_clone(ops, transaction, target, source, stride);
        if (result != HF_PLAS39_APPLIED)
            return result;
        if (!hf_plas39_read_planned(ops, transaction, reverse, reverse_before))
            return HF_PLAS39_NOT_READY;
        memcpy(word_before, reverse_before + 4u, sizeof(word_before));
        hf_plas39_write_u32(word_after, item->id);
        if (!hf_plas39_stage(transaction, reverse + 4u, word_before, word_after, 0, 0))
            return HF_PLAS39_REFUSED;
        memcpy(word_before, reverse_before, sizeof(word_before));
        hf_plas39_write_u32(word_after, next_index);
        if (!hf_plas39_stage(transaction, reverse, word_before, word_after, 0, 0))
            return HF_PLAS39_REFUSED;
        publications[publication_count].address = forward;
        publications[publication_count].index = next_index;
        publications[publication_count].id = item->id;
        ++publication_count;
        if (charge)
        {
            item->charge = target;
            item->charge_cached = 1u;
            item->charge_cloned = 1u;
        }
        else
        {
            item->projectile = target;
            item->projectile_cached = 1u;
            item->projectile_cloned = 1u;
        }
        ++next_index;
    }
    if (next_index != live_count)
    {
        uintptr_t count_address;
        if (!hf_plas39_address_add(manager, count_offset, &count_address))
            return HF_PLAS39_REFUSED;
        hf_plas39_write_u32(count_before, live_count);
        hf_plas39_write_u32(count_after, next_index);
        if (!hf_plas39_stage(transaction, count_address, count_before, count_after, 0, 0))
            return HF_PLAS39_REFUSED;
        for (i = 0u; i < publication_count; ++i)
        {
            uint8_t planned[8], before[4], after[4];
            uintptr_t address = publications[i].address;
            if (!hf_plas39_read_planned(ops, transaction, address, planned))
                return HF_PLAS39_NOT_READY;
            memcpy(before, planned + 4u, sizeof(before));
            hf_plas39_write_u32(after, publications[i].index);
            if (!hf_plas39_stage(transaction, address + 4u, before, after, 0, 0))
                return HF_PLAS39_REFUSED;
            if (!hf_plas39_read_planned(ops, transaction, address, planned))
                return HF_PLAS39_NOT_READY;
            memcpy(before, planned, sizeof(before));
            hf_plas39_write_u32(after, publications[i].id);
            if (!hf_plas39_stage(transaction, address, before, after, 0, 0))
                return HF_PLAS39_REFUSED;
        }
    }
    return HF_PLAS39_APPLIED;
}

static HfPlas39ApplyResult hf_plas39_stage_weapon(const HfPlas39MemoryOps* ops,
                                                  HfPlas39Transaction* transaction,
                                                  uintptr_t address)
{
    static const uint32_t offsets[] = {140u, 144u, 148u, 184u, 188u};
    uint8_t before[HF_PLAS39_WEAPON_PROFILE_SIZE], after[sizeof(before)];
    if (!hf_plas39_read(ops, address, before, sizeof(before)) ||
        !hf_plas39_weapon_profile(before, sizeof(before), after))
        return HF_PLAS39_REFUSED;
    return hf_plas39_stage_profile_fields(transaction, address, before, after, offsets,
                                          sizeof(offsets) / sizeof(offsets[0]), 0, 0);
}

static HfPlas39ApplyResult hf_plas39_stage_charge(HfPlas39Transaction* transaction,
                                                   HfPlas39Instance* item)
{
    static const uint32_t offsets[] = {0u, 188u};
    uint8_t after[HF_PLAS39_CHARGE_PROFILE_SIZE];
    if (!hf_plas39_charge_profile(item->charge_row, sizeof(item->charge_row), item->mode, after))
        return HF_PLAS39_REFUSED;
    return hf_plas39_stage_profile_fields(transaction, item->charge, item->charge_row, after,
                                          offsets, sizeof(offsets) / sizeof(offsets[0]),
                                          item->charge_cloned, item->charge_cloned);
}

static HfPlas39ApplyResult hf_plas39_stage_projectile(HfPlas39Transaction* transaction,
                                                       HfPlas39Instance* item)
{
    static const uint32_t offsets[] = {8u, 236u};
    uint8_t after[HF_PLAS39_PROJECTILE_PROFILE_SIZE];
    if (!hf_plas39_projectile_profile(item->projectile_row,
                                      HF_PLAS39_PROJECTILE_PROFILE_SIZE, item->mode, after))
        return HF_PLAS39_REFUSED;
    return hf_plas39_stage_profile_fields(transaction, item->projectile, item->projectile_row,
                                          after, offsets, sizeof(offsets) / sizeof(offsets[0]),
                                          item->projectile_cloned, item->projectile_cloned);
}

static HfPlas39ApplyResult hf_plas39_stage_cadence(const HfPlas39MemoryOps* ops,
                                                    HfPlas39Transaction* transaction,
                                                    const HfPlas39Instance* item)
{
    uintptr_t address;
    uint8_t before[4], after[4];
    if (!hf_plas39_address_add(item->projectile_state, 12u, &address) ||
        !hf_plas39_read(ops, address, before, sizeof(before)) ||
        !hf_plas39_cadence_profile(before, item->mode, after))
        return HF_PLAS39_REFUSED;
    return hf_plas39_stage(transaction, address, before, after, 0, 0)
        ? HF_PLAS39_APPLIED : HF_PLAS39_REFUSED;
}

static int hf_plas39_previous_mode(const HfPlas39PreviousMode* previous, size_t count,
                                    uint32_t id, uint32_t* mode)
{
    size_t i;
    for (i = 0u; i < count; ++i)
        if (previous[i].id == id)
        {
            if (mode != NULL)
                *mode = previous[i].mode;
            return 1;
        }
    return 0;
}

HfPlas39ApplyResult hf_plas39_synchronize(const HfPlas39MemoryOps* ops,
                                            HfPlas39Plan* plan,
                                            HfPlas39Instance* instances, size_t count,
                                            HfPlas39PreviousMode* previous, size_t previous_capacity,
                                            size_t* previous_count,
                                            HfPlas39Transaction* transaction,
                                            uint32_t* changed, char* status,
                                            size_t status_capacity)
{
    static const uint8_t zero[4] = {0u, 0u, 0u, 0u};
    size_t i;
    uint32_t quick = 0u, burst = 0u, changed_fields = 0u;
    HfPlas39ApplyResult result;
    char reason[192];
    if (changed != NULL)
        *changed = 0u;
    if (status != NULL && status_capacity != 0u)
        status[0] = '\0';
    if (!hf_plas39_ops_valid(ops) || plan == NULL || transaction == NULL ||
        (count != 0u && instances == NULL) || count > HF_PLAS39_MAX_INSTANCES ||
        previous == NULL || previous_count == NULL || count > previous_capacity ||
        previous_capacity < HF_PLAS39_MAX_INSTANCES)
    {
        hf_plas39_reason(status, status_capacity, "PLAS-39 mode runtime buffers are unavailable");
        return HF_PLAS39_REFUSED;
    }
    hf_plas39_transaction_reset(transaction);
    result = hf_plas39_isolate_manager(ops, plan, transaction, instances, count, 1);
    if (result == HF_PLAS39_APPLIED)
        result = hf_plas39_isolate_manager(ops, plan, transaction, instances, count, 0);
    if (result != HF_PLAS39_APPLIED)
    {
        hf_plas39_reason(status, status_capacity, "PLAS-39 private-row reservations changed or are unavailable");
        return result;
    }
    result = hf_plas39_stage_weapon(ops, transaction, plan->weapon_address);
    if (result != HF_PLAS39_APPLIED)
    {
        hf_plas39_reason(status, status_capacity, "PLAS-39 weapon mode fields changed");
        return result;
    }
    for (i = 0u; i < count; ++i)
    {
        HfPlas39Instance* item = &instances[i];
        uint32_t previous_mode = 0u;
        int had_previous = hf_plas39_previous_mode(previous, *previous_count, item->id, &previous_mode);
        uintptr_t queue_address;
        HfPlas39ApplyResult one;
        if (item->mode == HF_PLAS39_MODE_SEMI)
            ++quick;
        else if (item->mode == HF_PLAS39_MODE_BURST)
            ++burst;
        else
        {
            hf_plas39_reason(status, status_capacity, "PLAS-39 mode changed before synchronization");
            return HF_PLAS39_RETRY_RACE;
        }
        one = hf_plas39_stage_weapon(ops, transaction, item->weapon);
        if (one == HF_PLAS39_APPLIED)
            one = hf_plas39_stage_charge(transaction, item);
        if (one == HF_PLAS39_APPLIED)
            one = hf_plas39_stage_projectile(transaction, item);
        if (one == HF_PLAS39_APPLIED)
            one = hf_plas39_stage_cadence(ops, transaction, item);
        if (one != HF_PLAS39_APPLIED)
        {
            hf_plas39_reason(status, status_capacity, "PLAS-39 mode profile changed; no edit applied");
            return one;
        }
        if (hf_plas39_needs_repetition_clear(had_previous, previous_mode, item->mode) &&
            item->charge_state != 0u)
        {
            uint8_t queued[8];
            if (!hf_plas39_address_add(item->charge_state, 20u, &queue_address) ||
                !hf_plas39_read(ops, queue_address, queued, sizeof(queued)))
                return HF_PLAS39_RETRY_RACE;
            if (hf_plas39_read_u32(queued, 0u) > 2u)
            {
                hf_plas39_reason(status, status_capacity, "PLAS-39 charge repetition state changed");
                return HF_PLAS39_REFUSED;
            }
            if (!hf_plas39_stage(transaction, queue_address, queued, zero, 0, 0) ||
                !hf_plas39_stage(transaction, queue_address + 4u, queued + 4u, zero, 0, 0))
                return HF_PLAS39_REFUSED;
        }
    }
    result = hf_plas39_native_validate(ops, plan, instances, count);
    if (result != HF_PLAS39_APPLIED)
    {
        hf_plas39_reason(status, status_capacity, "PLAS-39 native instance snapshot changed before publication");
        return result;
    }
    result = hf_plas39_transaction_apply(ops, transaction, &changed_fields, reason, sizeof(reason));
    if (result != HF_PLAS39_APPLIED)
    {
        hf_plas39_reason(status, status_capacity, reason);
        return result;
    }
    for (i = 0u; i < count; ++i)
    {
        previous[i].id = instances[i].id;
        previous[i].mode = instances[i].mode;
    }
    *previous_count = count;
    if (changed != NULL)
        *changed = changed_fields;
    if (count == 0u)
        hf_plas39_reason(status, status_capacity, "Waiting for a PLAS-39 instance");
    else if (quick != 0u && burst == 0u)
        hf_plas39_reason(status, status_capacity,
                         "Semi: one shot; min_charge=0.01s; repetitions=0; base_rate=200 RPM; native cadence multiplier retained");
    else if (burst != 0u && quick == 0u)
        hf_plas39_reason(status, status_capacity,
                         "Burst: three charged shots; min_charge=0.45s; repetitions=2; ordinary firing event");
    else
        hf_plas39_reason(status, status_capacity, "Separate native PLAS-39 modes synchronized");
    return HF_PLAS39_APPLIED;
}
