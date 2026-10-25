/* Guarded ARC-3 Supercharge profiles, private-row transactions, and mode helpers. */
#define HF_ARC3_SC_MAX_EDITS 512u
#define HF_ARC3_SC_MAX_GUARDS 512u
#define HF_ARC3_SC_MAX_CONTEXTS 512u
#define HF_ARC3_SC_GUARD_BYTES 224u

#define HF_ARC3_SC_DAMAGE_DELTA 26144u
#define HF_ARC3_SC_ARC_DELTA 1352u

#define HF_ARC3_SC_CHARGE_CAPACITY 0x04u
#define HF_ARC3_SC_CHARGE_COUNT 0x88u
#define HF_ARC3_SC_CHARGE_FORWARD 0x50u
#define HF_ARC3_SC_CHARGE_REVERSE 0x70u
#define HF_ARC3_SC_CHARGE_ROWS 0x90u
#define HF_ARC3_SC_CHARGE_STRIDE 216u
#define HF_ARC3_SC_CHARGE_HEADER 160u

#define HF_ARC3_SC_ARC_CAPACITY 0x2Cu
#define HF_ARC3_SC_ARC_COUNT 0xC0u
#define HF_ARC3_SC_ARC_FORWARD 0x88u
#define HF_ARC3_SC_ARC_REVERSE 0xA8u
#define HF_ARC3_SC_ARC_ROWS 0xC8u
#define HF_ARC3_SC_ARC_STRIDE 80u
#define HF_ARC3_SC_ARC_HEADER 208u

typedef struct HfArc3ScEdit
{
    uintptr_t address;
    uint8_t before[4];
    uint8_t after[4];
    uint32_t protected_write;
    uint32_t attempted;
} HfArc3ScEdit;

typedef struct HfArc3ScGuard
{
    uintptr_t address;
    uint32_t size;
    uint8_t bytes[HF_ARC3_SC_GUARD_BYTES];
} HfArc3ScGuard;

typedef struct HfArc3ScTransaction
{
    HfArc3ScEdit edits[HF_ARC3_SC_MAX_EDITS];
    HfArc3ScGuard guards[HF_ARC3_SC_MAX_GUARDS];
    size_t edit_count;
    size_t guard_count;
} HfArc3ScTransaction;

typedef struct HfArc3ScProfiles
{
    uintptr_t game;
    uint8_t weapon[sizeof(HF_ARC3_EXPECT_WEAPON)];
    uint8_t charge[sizeof(HF_ARC3_EXPECT_CHARGE)];
    uint8_t charge_super[sizeof(HF_ARC3_EXPECT_CHARGE)];
    uint8_t arc[sizeof(HF_ARC3_EXPECT_ARCWEAPON)];
    uint8_t arc_super[sizeof(HF_ARC3_EXPECT_ARCWEAPON)];
} HfArc3ScProfiles;

typedef struct HfArc3ScManager
{
    uintptr_t manager;
    uint8_t header[HF_ARC3_SC_ARC_HEADER];
    uint32_t header_size;
    uint32_t kind;
} HfArc3ScManager;

typedef struct HfArc3ScMapResult
{
    uint32_t found;
    uint32_t index;
    uintptr_t slot;
} HfArc3ScMapResult;

typedef struct HfArc3ScEntity
{
    uint32_t id;
    uintptr_t entity;
    uint8_t identity[24];
    uint8_t entity_ref[8];
    uintptr_t weapon_ref_address;
    uintptr_t charge_ref_address;
    uintptr_t mode_address;
    uintptr_t state_address;
    uint32_t applied_mode;
} HfArc3ScEntity;

typedef struct HfArc3ScRuntime
{
    HfArc3ScProfiles profiles;
    uintptr_t weapon_manager;
    uintptr_t charge_manager;
    uintptr_t weapon_mode_array;
    uintptr_t charge_state_array;
    HfArc3ScEntity entities[HF_ARC3_SC_MAX_CONTEXTS];
    size_t entity_count;
    uint64_t frame;
    char status[128];
    uint32_t prepared;
    uint32_t stopped;
} HfArc3ScRuntime;

static uint32_t hf_arc3_sc_u32(const uint8_t* bytes, size_t offset)
{
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1u] << 8) |
           ((uint32_t)bytes[offset + 2u] << 16) | ((uint32_t)bytes[offset + 3u] << 24);
}

static uint64_t hf_arc3_sc_u64(const uint8_t* bytes, size_t offset)
{
    uint64_t value = 0u;
    size_t i;
    for (i = 0u; i < 8u; ++i)
        value |= (uint64_t)bytes[offset + i] << (i * 8u);
    return value;
}

static void hf_arc3_sc_word(uint32_t value, uint8_t bytes[4])
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static int hf_arc3_sc_add(uintptr_t base, size_t offset, uintptr_t* result)
{
    if (base == 0u || offset > (size_t)(UINTPTR_MAX - base))
        return 0;
    *result = base + offset;
    return 1;
}

static void hf_arc3_sc_reason(char* reason, size_t capacity, const char* message)
{
    if (reason != NULL && capacity > 0u)
        (void)snprintf(reason, capacity, "%s", message != NULL ? message : "ARC-3 Supercharge refused");
}

static int hf_arc3_sc_read(const HfArc3MemoryOps* ops, uintptr_t address,
                           void* output, size_t size)
{
    return ops != NULL && ops->read != NULL && ops->data_access != NULL &&
           output != NULL && address != 0u && size != 0u &&
           ops->data_access(ops->context, address, size) &&
           ops->read(ops->context, address, output, size);
}

static int hf_arc3_sc_guard(HfArc3ScTransaction* txn, const HfArc3MemoryOps* ops,
                            uintptr_t address, size_t size)
{
    HfArc3ScGuard* guard;
    if (txn == NULL || txn->guard_count >= HF_ARC3_SC_MAX_GUARDS ||
        size == 0u || size > HF_ARC3_SC_GUARD_BYTES)
        return 0;
    guard = &txn->guards[txn->guard_count];
    guard->address = address;
    guard->size = (uint32_t)size;
    if (!hf_arc3_sc_read(ops, address, guard->bytes, size))
        return 0;
    ++txn->guard_count;
    return 1;
}

static int hf_arc3_sc_add_edit(HfArc3ScTransaction* txn, uintptr_t address,
                               const uint8_t before[4], const uint8_t after[4],
                               int protected_write)
{
    HfArc3ScEdit* edit;
    size_t i;
    if (txn == NULL || before == NULL || after == NULL)
        return 0;
    if (memcmp(before, after, 4u) == 0)
        return 1;
    if (txn->edit_count >= HF_ARC3_SC_MAX_EDITS)
        return 0;
    for (i = 0u; i < txn->edit_count; ++i)
        if (txn->edits[i].address == address)
            return 0;
    edit = &txn->edits[txn->edit_count++];
    edit->address = address;
    memcpy(edit->before, before, 4u);
    memcpy(edit->after, after, 4u);
    edit->protected_write = protected_write != 0;
    edit->attempted = 0u;
    return 1;
}

static int hf_arc3_sc_word_edit(HfArc3ScTransaction* txn, const HfArc3MemoryOps* ops,
                                uintptr_t address, const uint8_t after[4], int protected_write)
{
    uint8_t before[4];
    return hf_arc3_sc_read(ops, address, before, sizeof(before)) &&
           hf_arc3_sc_add_edit(txn, address, before, after, protected_write);
}

static int hf_arc3_sc_stage_row(HfArc3ScTransaction* txn, const HfArc3MemoryOps* ops,
                                uintptr_t address, const uint8_t* before,
                                const uint8_t* after, size_t size, int protected_write)
{
    size_t offset;
    (void)ops;
    if (size == 0u || size % 4u != 0u)
        return 0;
    for (offset = 0u; offset < size; offset += 4u)
        if (!hf_arc3_sc_add_edit(txn, address + offset, before + offset,
                                 after + offset, protected_write))
            return 0;
    return 1;
}

static int hf_arc3_sc_restore(const HfArc3MemoryOps* ops, HfArc3ScTransaction* txn,
                              char* reason, size_t capacity)
{
    size_t i = txn->edit_count;
    int restored = 1;
    while (i > 0u)
    {
        HfArc3ScEdit* edit = &txn->edits[--i];
        uint8_t current[4];
        if (!edit->attempted)
            continue;
        if (!hf_arc3_sc_read(ops, edit->address, current, sizeof(current)))
        {
            restored = 0;
            continue;
        }
        if (memcmp(current, edit->after, sizeof(current)) == 0)
        {
            if (!ops->write(ops->context, edit->address, edit->before, sizeof(edit->before),
                            (int)edit->protected_write) ||
                !hf_arc3_sc_read(ops, edit->address, current, sizeof(current)) ||
                memcmp(current, edit->before, sizeof(current)) != 0)
                restored = 0;
        }
        else if (memcmp(current, edit->before, sizeof(current)) != 0)
            restored = 0;
    }
    if (!restored)
        hf_arc3_sc_reason(reason, capacity, "FATAL: ARC-3 Supercharge transaction rollback was incomplete");
    return restored;
}

static int hf_arc3_sc_commit(const HfArc3MemoryOps* ops, HfArc3ScTransaction* txn,
                             uint32_t* changed, char* reason, size_t capacity)
{
    size_t i;
    if (changed != NULL)
        *changed = 0u;
    if (ops == NULL || ops->read == NULL || ops->write == NULL ||
        ops->can_write == NULL || ops->data_access == NULL || txn == NULL)
    {
        hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge memory operations are unavailable");
        return HF_ARC3_REFUSED;
    }
    for (i = 0u; i < txn->guard_count; ++i)
    {
        HfArc3ScGuard* guard = &txn->guards[i];
        uint8_t current[HF_ARC3_SC_GUARD_BYTES];
        if (!hf_arc3_sc_read(ops, guard->address, current, guard->size) ||
            memcmp(current, guard->bytes, guard->size) != 0)
        {
            hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge transaction guard changed");
            return HF_ARC3_REFUSED;
        }
    }
    for (i = 0u; i < txn->edit_count; ++i)
    {
        HfArc3ScEdit* edit = &txn->edits[i];
        uint8_t current[4];
        if (!hf_arc3_sc_read(ops, edit->address, current, sizeof(current)) ||
            memcmp(current, edit->before, sizeof(current)) != 0 ||
            !ops->can_write(ops->context, edit->address, sizeof(current), (int)edit->protected_write))
        {
            hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge target or writer permission changed");
            return HF_ARC3_REFUSED;
        }
    }
    for (i = 0u; i < txn->edit_count; ++i)
    {
        HfArc3ScEdit* edit = &txn->edits[i];
        uint8_t current[4];
        edit->attempted = 1u;
        if (!ops->write(ops->context, edit->address, edit->after, sizeof(edit->after),
                        (int)edit->protected_write) ||
            !hf_arc3_sc_read(ops, edit->address, current, sizeof(current)) ||
            memcmp(current, edit->after, sizeof(current)) != 0)
        {
            if (!hf_arc3_sc_restore(ops, txn, reason, capacity))
                return HF_ARC3_FATAL;
            hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge write failed; earlier writes were restored");
            return HF_ARC3_REFUSED;
        }
        if (changed != NULL)
            ++*changed;
    }
    hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge transaction applied");
    return HF_ARC3_APPLIED;
}

static int hf_arc3_sc_map(const HfArc3MemoryOps* ops, const uint8_t* header,
                          size_t header_size, size_t offset, uint32_t key,
                          HfArc3ScTransaction* txn, HfArc3ScMapResult* result)
{
    uintptr_t array;
    uint32_t capacity, empty, multiplier;
    uint64_t product;
    uintptr_t tombstone = 0u;
    uint32_t probe, limit;
    if (header == NULL || result == NULL || offset > header_size ||
        header_size - offset < 20u)
        return -1;
    memset(result, 0, sizeof(*result));
    array = (uintptr_t)hf_arc3_sc_u64(header, offset);
    capacity = hf_arc3_sc_u32(header, offset + 8u);
    empty = hf_arc3_sc_u32(header, offset + 12u);
    multiplier = hf_arc3_sc_u32(header, offset + 16u);
    if (array == 0u || capacity == 0u || capacity > 4096u ||
        (capacity & (capacity - 1u)) != 0u)
        return -1;
    product = (uint64_t)(key & 0xFFFFu) * multiplier +
              (uint64_t)(key >> 16) * (multiplier & 0xFFFFu) * UINT64_C(65536);
    limit = capacity < 64u ? capacity : 64u;
    for (probe = 0u; probe < limit; ++probe)
    {
        uint32_t slot_index = ((uint32_t)product + probe) & (capacity - 1u);
        uintptr_t slot_address;
        uint8_t row[8];
        uint32_t identity, index;
        if (!hf_arc3_sc_add(array, (size_t)slot_index * 8u, &slot_address) ||
            !hf_arc3_sc_read(ops, slot_address, row, sizeof(row)))
            return -1;
        if (txn != NULL && !hf_arc3_sc_guard(txn, ops, slot_address, sizeof(row)))
            return -1;
        identity = hf_arc3_sc_u32(row, 0u);
        index = hf_arc3_sc_u32(row, 4u);
        if (identity == key && index != UINT32_MAX)
        {
            result->found = 1u;
            result->index = index;
            result->slot = slot_address;
            return 1;
        }
        if (index == UINT32_MAX && tombstone == 0u)
            tombstone = slot_address;
        if (identity == empty || identity == key)
        {
            result->slot = tombstone != 0u ? tombstone : slot_address;
            return 0;
        }
    }
    return -1;
}

/* Only these reviewed module data slots bypass the private-allocation filter.
 * All donor records and every mutation still use the private row guards. */
static int hf_arc3_sc_module_pointer(const HfArc3MemoryOps* ops, uintptr_t game,
                                     size_t rva, uint8_t output[8])
{
    uintptr_t address;
    if (ops == NULL || ops->read == NULL || output == NULL ||
        (rva != 0x346BF98u && rva != 0x3326CE0u && rva != 0x3326C20u && rva != 0x3326C10u) ||
        game < (uintptr_t)65536u || !hf_arc3_sc_add(game, rva, &address) ||
        address >= (uintptr_t)UINT64_C(0x800000000000) - 8u)
        return 0;
    return ops->read(ops->context, address, output, 8u);
}

static int hf_arc3_sc_source_arcweapon(const HfArc3MemoryOps* ops, uintptr_t game,
                                       uintptr_t* row_address)
{
    uint8_t pointer_bytes[8];
    uintptr_t root, base;
    uint32_t probe;
    if (row_address == NULL || !hf_arc3_sc_module_pointer(ops, game, 0x346BF98u, pointer_bytes))
        return 0;
    root = (uintptr_t)hf_arc3_sc_u64(pointer_bytes, 0u);
    if (!hf_arc3_sc_read(ops, root + UINT64_C(0xF12CC0), pointer_bytes, sizeof(pointer_bytes)))
        return 0;
    base = (uintptr_t)hf_arc3_sc_u64(pointer_bytes, 0u);
    if (base == 0u)
        return 0;
    for (probe = 0u; probe < 20u; ++probe)
    {
        uint32_t slot = (2u + probe) % 20u;
        uint8_t entry[16];
        uint32_t index;
        uintptr_t address;
        if (!hf_arc3_sc_add(base, (size_t)slot * 16u, &address) ||
            !hf_arc3_sc_read(ops, address, entry, sizeof(entry)))
            return 0;
        if (hf_arc3_sc_u32(entry, 0u) == 0u && hf_arc3_sc_u32(entry, 4u) == 0u)
            return 0;
        if (hf_arc3_sc_u32(entry, 0u) == UINT32_C(0x0F7306E6) &&
            hf_arc3_sc_u32(entry, 4u) == UINT32_C(0x96DE9CD5))
        {
            index = hf_arc3_sc_u32(entry, 8u);
            if (index == UINT32_MAX || index > (UINTPTR_MAX - base - 0x140u) / 80u)
                return 0;
            *row_address = base + 0x140u + (uintptr_t)index * 80u;
            return 1;
        }
    }
    return 0;
}

static int hf_arc3_sc_prepare(const HfArc3MemoryOps* ops, uintptr_t game,
                              uintptr_t damage_row, uintptr_t arc_row,
                              uintptr_t charge_row, uintptr_t weapon_row,
                              HfArc3ScProfiles* profiles, uint32_t* changed,
                              char* reason, size_t capacity)
{
    uint8_t weapon[sizeof(HF_ARC3_EXPECT_WEAPON)];
    uint8_t charge[sizeof(HF_ARC3_EXPECT_CHARGE)];
    uint8_t arc7[sizeof(HF_ARC3_EXPECT_ARC7)];
    uint8_t damage197[sizeof(HF_ARC3_EXPECT_DAMAGE197)];
    uint8_t arcweapon[sizeof(HF_ARC3_EXPECT_ARCWEAPON)];
    uint8_t damage547[sizeof(HF_ARC3_EXPECT_DAMAGE547)];
    uint8_t arc11[sizeof(HF_ARC3_EXPECT_ARC11)];
    uint8_t normalized_charge[sizeof(HF_ARC3_EXPECT_CHARGE)];
    uintptr_t arcweapon_address, damage547_address, arc11_address;
    HfArc3ScTransaction txn;
    size_t i;
    int result;
    if (profiles == NULL || game == 0u || damage_row == 0u || arc_row == 0u ||
        charge_row == 0u || weapon_row == 0u || ops == NULL)
    {
        hf_arc3_sc_reason(reason, capacity, "ARC-3 Rapid source addresses are unavailable");
        return HF_ARC3_REFUSED;
    }
    if (!hf_arc3_sc_read(ops, weapon_row, weapon, sizeof(weapon)) ||
        !hf_arc3_sc_read(ops, charge_row, charge, sizeof(charge)) ||
        !hf_arc3_sc_read(ops, arc_row, arc7, sizeof(arc7)) ||
        !hf_arc3_sc_read(ops, damage_row, damage197, sizeof(damage197)) ||
        memcmp(weapon, HF_ARC3_EXPECT_WEAPON, sizeof(weapon)) != 0 ||
        memcmp(arc7, HF_ARC3_EXPECT_ARC7, sizeof(arc7)) != 0 ||
        memcmp(damage197, HF_ARC3_EXPECT_DAMAGE197, sizeof(damage197)) != 0 ||
        (charge[184] != 0u && charge[184] != 1u))
    {
        hf_arc3_sc_reason(reason, capacity, "ARC-3 Rapid source changed before Supercharge preparation");
        return HF_ARC3_REFUSED;
    }
    memcpy(normalized_charge, charge, sizeof(charge));
    normalized_charge[184] = 0u;
    if (memcmp(normalized_charge, HF_ARC3_EXPECT_CHARGE, sizeof(charge)) != 0 ||
        !hf_arc3_sc_source_arcweapon(ops, game, &arcweapon_address) ||
        !hf_arc3_sc_read(ops, arcweapon_address, arcweapon, sizeof(arcweapon)) ||
        memcmp(arcweapon, HF_ARC3_EXPECT_ARCWEAPON, sizeof(arcweapon)) != 0 ||
        !hf_arc3_sc_add(damage_row, HF_ARC3_SC_DAMAGE_DELTA, &damage547_address) ||
        !hf_arc3_sc_add(arc_row, HF_ARC3_SC_ARC_DELTA, &arc11_address) ||
        !hf_arc3_sc_read(ops, damage547_address, damage547, sizeof(damage547)) ||
        !hf_arc3_sc_read(ops, arc11_address, arc11, sizeof(arc11)) ||
        memcmp(damage547, HF_ARC3_EXPECT_DAMAGE547, sizeof(damage547)) != 0 ||
        memcmp(arc11, HF_ARC3_EXPECT_ARC11, sizeof(arc11)) != 0)
    {
        hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge donor records are missing, changed, or ambiguous");
        return HF_ARC3_REFUSED;
    }
    if (hf_arc3_sc_u32(weapon, 144u) != 5u || hf_arc3_sc_u32(weapon, 148u) != 6u ||
        memcmp(weapon + 184u, "\0\0\0\0\0\0\0\0", 8u) != 0)
    {
        hf_arc3_sc_reason(reason, capacity, "ARC-3 global fire-mode source changed before Supercharge preparation");
        return HF_ARC3_REFUSED;
    }

    memset(&txn, 0, sizeof(txn));
    if (!hf_arc3_sc_guard(&txn, ops, charge_row, sizeof(charge)) ||
        !hf_arc3_sc_guard(&txn, ops, arc_row, sizeof(arc7)) ||
        !hf_arc3_sc_guard(&txn, ops, damage_row, sizeof(damage197)) ||
        !hf_arc3_sc_guard(&txn, ops, arcweapon_address, sizeof(arcweapon)) ||
        !hf_arc3_sc_guard(&txn, ops, damage547_address, sizeof(damage547)) ||
        !hf_arc3_sc_guard(&txn, ops, arc11_address, sizeof(arc11)))
    {
        hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge source guards are unreadable");
        return HF_ARC3_REFUSED;
    }
    if (!hf_arc3_sc_stage_row(&txn, ops, damage547_address, damage547,
                              HF_ARC3_SUPER_DAMAGE547, sizeof(damage547), 0) ||
        !hf_arc3_sc_stage_row(&txn, ops, arc11_address, arc11,
                              HF_ARC3_SUPER_ARC11, sizeof(arc11), 1))
    {
        hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge transaction exceeded its bounded edit capacity");
        return HF_ARC3_REFUSED;
    }
    {
        uint8_t primary[4], secondary[4];
        hf_arc3_sc_word(HF_ARC3_SELECTOR_PRIMARY, primary);
        hf_arc3_sc_word(HF_ARC3_SELECTOR_SECONDARY, secondary);
        if (!hf_arc3_sc_word_edit(&txn, ops, weapon_row + 144u, primary, 1) ||
            !hf_arc3_sc_word_edit(&txn, ops, weapon_row + 148u, secondary, 1) ||
            !hf_arc3_sc_add_edit(&txn, weapon_row + 184u, weapon + 184u,
                                 (const uint8_t*)"\x03\0\0\0", 1) ||
            !hf_arc3_sc_add_edit(&txn, weapon_row + 188u, weapon + 188u,
                                 (const uint8_t*)"\0\0\0\0", 1))
        {
            hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge selector transaction could not be staged");
            return HF_ARC3_REFUSED;
        }
    }
    result = hf_arc3_sc_commit(ops, &txn, changed, reason, capacity);
    if (result != HF_ARC3_APPLIED)
        return result;
    if (!hf_arc3_sc_read(ops, damage547_address, damage547, sizeof(damage547)) ||
        memcmp(damage547, HF_ARC3_SUPER_DAMAGE547, sizeof(damage547)) != 0 ||
        !hf_arc3_sc_read(ops, arc11_address, arc11, sizeof(arc11)) ||
        memcmp(arc11, HF_ARC3_SUPER_ARC11, sizeof(arc11)) != 0 ||
        !hf_arc3_sc_read(ops, weapon_row, weapon, sizeof(weapon)) ||
        hf_arc3_sc_u32(weapon, 144u) != HF_ARC3_SELECTOR_PRIMARY ||
        hf_arc3_sc_u32(weapon, 148u) != HF_ARC3_SELECTOR_SECONDARY ||
        memcmp(weapon + 184u, "\x03\0\0\0\0\0\0\0", 8u) != 0)
    {
        hf_arc3_sc_reason(reason, capacity, "FATAL: ARC-3 Supercharge global profile readback failed");
        return HF_ARC3_FATAL;
    }
    memset(profiles, 0, sizeof(*profiles));
    profiles->game = game;
    memcpy(profiles->weapon, weapon, sizeof(weapon));
    memcpy(profiles->charge, charge, sizeof(charge));
    memcpy(profiles->charge_super, charge, sizeof(charge));
    profiles->charge_super[0] = 0x9Au;
    profiles->charge_super[1] = 0x99u;
    profiles->charge_super[2] = 0x59u;
    profiles->charge_super[3] = 0x40u;
    profiles->charge_super[24] = 0x00u;
    profiles->charge_super[25] = 0x00u;
    profiles->charge_super[26] = 0x60u;
    profiles->charge_super[27] = 0x40u;
    profiles->charge_super[48] = 0x00u;
    profiles->charge_super[49] = 0x00u;
    profiles->charge_super[50] = 0x70u;
    profiles->charge_super[51] = 0x40u;
    for (i = 80u; i < 88u; ++i)
        profiles->charge_super[i] = 0u;
    profiles->charge_super[82] = 0x80u;
    profiles->charge_super[83] = 0x3Fu;
    profiles->charge_super[86] = 0x80u;
    profiles->charge_super[87] = 0x3Fu;
    profiles->charge_super[185] = 0u;
    memcpy(profiles->arc, arcweapon, sizeof(arcweapon));
    memcpy(profiles->arc_super, arcweapon, sizeof(arcweapon));
    profiles->arc_super[0] = 11u;
    hf_arc3_sc_reason(reason, capacity, "ARC-3 Supercharge profile prepared and source-guarded");
    return HF_ARC3_APPLIED;
}

static int hf_arc3_sc_group_layout(uint32_t kind, size_t* header_size,
                                   size_t* count_offset, size_t* capacity_offset,
                                   size_t* forward_offset, size_t* reverse_offset,
                                   size_t* rows_offset, size_t* stride)
{
    if (kind == 1u)
    {
        *header_size = HF_ARC3_SC_CHARGE_HEADER;
        *count_offset = HF_ARC3_SC_CHARGE_COUNT;
        *capacity_offset = HF_ARC3_SC_CHARGE_CAPACITY;
        *forward_offset = HF_ARC3_SC_CHARGE_FORWARD;
        *reverse_offset = HF_ARC3_SC_CHARGE_REVERSE;
        *rows_offset = HF_ARC3_SC_CHARGE_ROWS;
        *stride = HF_ARC3_SC_CHARGE_STRIDE;
        return 1;
    }
    if (kind == 2u)
    {
        *header_size = HF_ARC3_SC_ARC_HEADER;
        *count_offset = HF_ARC3_SC_ARC_COUNT;
        *capacity_offset = HF_ARC3_SC_ARC_CAPACITY;
        *forward_offset = HF_ARC3_SC_ARC_FORWARD;
        *reverse_offset = HF_ARC3_SC_ARC_REVERSE;
        *rows_offset = HF_ARC3_SC_ARC_ROWS;
        *stride = HF_ARC3_SC_ARC_STRIDE;
        return 1;
    }
    return 0;
}

static int hf_arc3_sc_plan_group(const HfArc3MemoryOps* ops,
                                HfArc3ScTransaction* txn, uint32_t kind,
                                uintptr_t manager, uint32_t id,
                                const uint8_t* rapid, const uint8_t* super,
                                const uint8_t* desired)
{
    uint8_t header[HF_ARC3_SC_ARC_HEADER];
    uint8_t row[HF_ARC3_SC_GUARD_BYTES];
    size_t header_size, count_offset, capacity_offset, forward_offset;
    size_t reverse_offset, rows_offset, stride;
    uintptr_t rows, target;
    uint32_t count, capacity, index;
    HfArc3ScMapResult forward, reverse;
    int forward_result, reverse_result;
    size_t i;
    if (!hf_arc3_sc_group_layout(kind, &header_size, &count_offset, &capacity_offset,
                                 &forward_offset, &reverse_offset, &rows_offset, &stride) ||
        !hf_arc3_sc_read(ops, manager, header, header_size) ||
        !hf_arc3_sc_guard(txn, ops, manager, header_size))
        return 0;
    count = hf_arc3_sc_u32(header, count_offset);
    capacity = hf_arc3_sc_u32(header, capacity_offset);
    if (count > capacity || capacity > 512u)
        return 0;
    rows = (uintptr_t)hf_arc3_sc_u64(header, rows_offset);
    if (rows == 0u)
        return 0;
    forward_result = hf_arc3_sc_map(ops, header, header_size, forward_offset, id, txn, &forward);
    if (forward_result < 0)
        return 0;
    if (forward_result > 0)
    {
        index = forward.index;
        if (index >= count)
            return 0;
        reverse_result = hf_arc3_sc_map(ops, header, header_size, reverse_offset,
                                        index, txn, &reverse);
        if (reverse_result <= 0 || reverse.index != id ||
            index > (UINTPTR_MAX - rows) / stride)
            return 0;
        target = rows + (uintptr_t)index * stride;
        if (!hf_arc3_sc_read(ops, target, row, stride) ||
            !hf_arc3_sc_guard(txn, ops, target, stride) ||
            (memcmp(row, rapid, stride) != 0 && memcmp(row, super, stride) != 0))
            return 0;
        for (i = 0u; i < stride; i += 4u)
            if (!hf_arc3_sc_add_edit(txn, target + i, row + i, desired + i, 0))
                return 0;
        return 1;
    }
    if (count >= capacity || forward.slot == 0u || count > (UINTPTR_MAX - rows) / stride)
        return 0;
    index = count;
    target = rows + (uintptr_t)index * stride;
    if (!hf_arc3_sc_read(ops, target, row, stride) ||
        !hf_arc3_sc_guard(txn, ops, target, stride))
        return 0;
    for (i = 0u; i < stride; i += 4u)
        if (!hf_arc3_sc_add_edit(txn, target + i, row + i, desired + i, 0))
            return 0;
    reverse_result = hf_arc3_sc_map(ops, header, header_size, reverse_offset,
                                    index, txn, &reverse);
    if (reverse_result != 0 || reverse.slot == 0u)
        return 0;
    {
        uint8_t key[4], value[4];
        hf_arc3_sc_word(index, key);
        hf_arc3_sc_word(id, value);
        if (!hf_arc3_sc_word_edit(txn, ops, reverse.slot + 4u, value, 0) ||
            !hf_arc3_sc_word_edit(txn, ops, reverse.slot, key, 0))
            return 0;
    }
    {
        uint8_t updated_count[4];
        hf_arc3_sc_word(count + 1u, updated_count);
        if (!hf_arc3_sc_word_edit(txn, ops, manager + count_offset, updated_count, 0))
            return 0;
    }
    {
        uint8_t key[4], value[4];
        hf_arc3_sc_word(id, key);
        hf_arc3_sc_word(index, value);
        if (!hf_arc3_sc_word_edit(txn, ops, forward.slot + 4u, value, 0) ||
            !hf_arc3_sc_word_edit(txn, ops, forward.slot, key, 0))
            return 0;
    }
    return 1;
}

static int hf_arc3_sc_patch_private_weapon(const HfArc3MemoryOps* ops,
                                           HfArc3ScTransaction* txn,
                                           uintptr_t manager, uint32_t id)
{
    uint8_t header[192], fields[52], one[4];
    HfArc3ScMapResult mapped;
    uintptr_t rows, row;
    int found;
    uint32_t primary, secondary;
    static const uint8_t selector[8] = {3u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    if (!hf_arc3_sc_read(ops, manager, header, sizeof(header)) ||
        !hf_arc3_sc_guard(txn, ops, manager, sizeof(header)))
        return 0;
    found = hf_arc3_sc_map(ops, header, sizeof(header), 0x70u, id, txn, &mapped);
    if (found < 0)
        return 0;
    if (!found)
        return 1;
    if (mapped.index >= hf_arc3_sc_u32(header, 0xA8u))
        return 0;
    rows = (uintptr_t)hf_arc3_sc_u64(header, 0xB0u);
    if (rows == 0u || mapped.index > (UINTPTR_MAX - rows) / 1232u)
        return 0;
    row = rows + (uintptr_t)mapped.index * 1232u;
    if (!hf_arc3_sc_read(ops, row + 140u, fields, sizeof(fields)) ||
        !hf_arc3_sc_guard(txn, ops, row + 140u, sizeof(fields)))
        return 0;
    primary = hf_arc3_sc_u32(fields, 4u);
    secondary = hf_arc3_sc_u32(fields, 8u);
    if (hf_arc3_sc_u32(fields, 0u) != 1u ||
        (primary != 5u && primary != 1u) ||
        (secondary != 6u && secondary != 1u && secondary != 2u) ||
        hf_arc3_sc_u32(fields, 12u) != 0u || hf_arc3_sc_u32(fields, 16u) != 0u ||
        (memcmp(fields + 44u, "\0\0\0\0\0\0\0\0", 8u) != 0 &&
         memcmp(fields + 44u, selector, sizeof(selector)) != 0))
        return 0;
    hf_arc3_sc_word(HF_ARC3_SELECTOR_PRIMARY, one);
    if (!hf_arc3_sc_word_edit(txn, ops, row + 144u, one, 0))
        return 0;
    hf_arc3_sc_word(HF_ARC3_SELECTOR_SECONDARY, one);
    if (!hf_arc3_sc_word_edit(txn, ops, row + 148u, one, 0))
        return 0;
    return hf_arc3_sc_add_edit(txn, row + 184u, fields + 44u, selector, 0);
}

static int hf_arc3_sc_current_profile(const HfArc3MemoryOps* ops,
                                      uintptr_t charge_manager, const uint8_t* charge_header,
                                      uintptr_t arc_manager, const uint8_t* arc_header,
                                      const HfArc3ScProfiles* profiles, uint32_t id)
{
    HfArc3ScMapResult charge_map, arc_map;
    uintptr_t charge_rows, arc_rows;
    uint8_t charge[sizeof(HF_ARC3_EXPECT_CHARGE)];
    uint8_t arc[sizeof(HF_ARC3_EXPECT_ARCWEAPON)];
    int has_charge = hf_arc3_sc_map(ops, charge_header, HF_ARC3_SC_CHARGE_HEADER,
                                    HF_ARC3_SC_CHARGE_FORWARD, id, NULL, &charge_map);
    int has_arc = hf_arc3_sc_map(ops, arc_header, HF_ARC3_SC_ARC_HEADER,
                                 HF_ARC3_SC_ARC_FORWARD, id, NULL, &arc_map);
    if (has_charge < 0 || has_arc < 0)
        return -1;
    if (!has_charge && !has_arc)
        return 1;
    if (!has_charge || !has_arc ||
        charge_map.index >= hf_arc3_sc_u32(charge_header, HF_ARC3_SC_CHARGE_COUNT) ||
        arc_map.index >= hf_arc3_sc_u32(arc_header, HF_ARC3_SC_ARC_COUNT))
        return -1;
    charge_rows = (uintptr_t)hf_arc3_sc_u64(charge_header, HF_ARC3_SC_CHARGE_ROWS);
    arc_rows = (uintptr_t)hf_arc3_sc_u64(arc_header, HF_ARC3_SC_ARC_ROWS);
    if (charge_rows == 0u || arc_rows == 0u ||
        !hf_arc3_sc_read(ops, charge_rows + (uintptr_t)charge_map.index * HF_ARC3_SC_CHARGE_STRIDE,
                         charge, sizeof(charge)) ||
        !hf_arc3_sc_read(ops, arc_rows + (uintptr_t)arc_map.index * HF_ARC3_SC_ARC_STRIDE,
                         arc, sizeof(arc)))
        return -1;
    if (memcmp(charge, profiles->charge, sizeof(charge)) == 0 &&
        memcmp(arc, profiles->arc, sizeof(arc)) == 0)
        return 1;
    if (memcmp(charge, profiles->charge_super, sizeof(charge)) == 0 &&
        memcmp(arc, profiles->arc_super, sizeof(arc)) == 0)
        return 2;
    return -1;
}

static int hf_arc3_sc_apply_profile(const HfArc3MemoryOps* ops,
                                    const HfArc3ScProfiles* profiles,
                                    uintptr_t weapon_manager, uintptr_t charge_manager,
                                    uintptr_t arc_manager, uint32_t id, uint32_t mode,
                                    uint32_t* changed, char* reason, size_t capacity)
{
    HfArc3ScTransaction txn;
    uint8_t charge_header[HF_ARC3_SC_CHARGE_HEADER];
    uint8_t arc_header[HF_ARC3_SC_ARC_HEADER];
    int result;
    if (mode != 1u && mode != 2u)
    {
        hf_arc3_sc_reason(reason, capacity, "Unsupported ARC fire mode");
        return HF_ARC3_REFUSED;
    }
    memset(&txn, 0, sizeof(txn));
    if (!hf_arc3_sc_patch_private_weapon(ops, &txn, weapon_manager, id) ||
        !hf_arc3_sc_read(ops, charge_manager, charge_header, sizeof(charge_header)) ||
        !hf_arc3_sc_read(ops, arc_manager, arc_header, sizeof(arc_header)) ||
        !hf_arc3_sc_plan_group(ops, &txn, 1u, charge_manager, id,
                               profiles->charge, profiles->charge_super,
                               mode == 2u ? profiles->charge_super : profiles->charge) ||
        !hf_arc3_sc_plan_group(ops, &txn, 2u, arc_manager, id,
                               profiles->arc, profiles->arc_super,
                               mode == 2u ? profiles->arc_super : profiles->arc))
    {
        hf_arc3_sc_reason(reason, capacity, "ARC private profile ownership, capacity, or guards changed");
        return HF_ARC3_REFUSED;
    }
    result = hf_arc3_sc_commit(ops, &txn, changed, reason, capacity);
    return result;
}

static int hf_arc3_sc_busy(const HfArc3MemoryOps* ops, uintptr_t state_address,
                           int* is_busy)
{
    uint8_t state[28];
    uint32_t charge_bits, release_bits;
    float charge_value, release_value;
    if (is_busy == NULL || !hf_arc3_sc_read(ops, state_address, state, sizeof(state)))
        return 0;
    charge_bits = hf_arc3_sc_u32(state, 4u);
    release_bits = hf_arc3_sc_u32(state, 24u);
    memcpy(&charge_value, &charge_bits, sizeof(charge_value));
    memcpy(&release_value, &release_bits, sizeof(release_value));
    if (!(charge_value == charge_value) || !(release_value == release_value) ||
        charge_value > 30.0f || charge_value < -30.0f ||
        release_value > 30.0f || release_value < -30.0f)
        return 0;
    *is_busy = charge_value != 0.0f || release_value != 0.0f;
    return 1;
}

static int hf_arc3_sc_resolve_managers(const HfArc3MemoryOps* ops,
                                       const HfArc3ScProfiles* profiles,
                                       HfArc3ScManager* weapon,
                                       HfArc3ScManager* charge,
                                       HfArc3ScManager* arc)
{
    static const size_t offsets[3] = {0x3326CE0u, 0x3326C20u, 0x3326C10u};
    HfArc3ScManager* managers[3] = {weapon, charge, arc};
    static const uint32_t sizes[3] = {192u, HF_ARC3_SC_CHARGE_HEADER, HF_ARC3_SC_ARC_HEADER};
    uint8_t pointer_bytes[8];
    size_t i;
    if (ops == NULL || profiles == NULL || weapon == NULL || charge == NULL || arc == NULL)
        return 0;
    for (i = 0u; i < 3u; ++i)
    {
        uintptr_t pointer_address, manager;
        if (!hf_arc3_sc_add(profiles->game, offsets[i], &pointer_address) ||
            !hf_arc3_sc_module_pointer(ops, profiles->game, offsets[i], pointer_bytes))
            return 0;
        manager = (uintptr_t)hf_arc3_sc_u64(pointer_bytes, 0u);
        if (manager == 0u || !hf_arc3_sc_read(ops, manager, managers[i]->header, sizes[i]))
            return 0;
        managers[i]->manager = manager;
        managers[i]->header_size = sizes[i];
        managers[i]->kind = (uint32_t)i;
    }
    return 1;
}

static int hf_arc3_sc_profile_present(const HfArc3MemoryOps* ops,
                                     const uint8_t* charge_header,
                                     const uint8_t* arc_header,
                                     uint32_t id, int* present)
{
    HfArc3ScMapResult charge_map, arc_map;
    int charge_result = hf_arc3_sc_map(ops, charge_header, HF_ARC3_SC_CHARGE_HEADER,
                                        HF_ARC3_SC_CHARGE_FORWARD, id, NULL, &charge_map);
    int arc_result = hf_arc3_sc_map(ops, arc_header, HF_ARC3_SC_ARC_HEADER,
                                    HF_ARC3_SC_ARC_FORWARD, id, NULL, &arc_map);
    if (present == NULL || charge_result < 0 || arc_result < 0 ||
        (charge_result > 0 && charge_map.index >= hf_arc3_sc_u32(charge_header, HF_ARC3_SC_CHARGE_COUNT)) ||
        (arc_result > 0 && arc_map.index >= hf_arc3_sc_u32(arc_header, HF_ARC3_SC_ARC_COUNT)))
        return 0;
    if ((charge_result > 0) != (arc_result > 0))
        return 0;
    *present = charge_result > 0;
    return 1;
}

static int hf_arc3_sc_transition_due(int present, int current_profile,
                                    uint32_t selected_mode, int is_busy)
{
    return !is_busy && (!present || current_profile != (int)selected_mode);
}

static int hf_arc3_sc_arrays_match(const HfArc3ScRuntime* runtime,
                                   const uint8_t* weapon_header,
                                   const uint8_t* charge_header)
{
    return runtime != NULL && weapon_header != NULL && charge_header != NULL &&
           (uintptr_t)hf_arc3_sc_u64(weapon_header, 0x60u) == runtime->weapon_mode_array &&
           (uintptr_t)hf_arc3_sc_u64(charge_header, 0x40u) == runtime->charge_state_array;
}

static int hf_arc3_sc_apply_entity_mode(const HfArc3MemoryOps* ops,
                                        HfArc3ScRuntime* runtime,
                                        HfArc3ScEntity* entity, uint32_t mode)
{
    HfArc3ScManager weapon, charge, arc;
    uint32_t changed = 0u;
    char reason[256];
    int result;
    if (!hf_arc3_sc_resolve_managers(ops, &runtime->profiles, &weapon, &charge, &arc))
        return 0;
    result = hf_arc3_sc_apply_profile(ops, &runtime->profiles,
                                      weapon.manager, charge.manager, arc.manager,
                                      entity->id, mode, &changed, reason, sizeof(reason));
    if (result != HF_ARC3_APPLIED)
    {
        hf_arc3_sc_reason(runtime->status, sizeof(runtime->status), reason);
        return 0;
    }
    entity->applied_mode = mode;
    return 1;
}

static int hf_arc3_sc_discover(HfArc3MemoryOps const* ops, HfArc3ScRuntime* runtime)
{
    HfArc3ScManager weapon, charge, arc;
    uintptr_t refs;
    uint32_t owned, capacity, slot;
    size_t found = 0u;
    uint8_t reference_bytes[HF_ARC3_SC_MAX_CONTEXTS * 8u];
    if (!hf_arc3_sc_resolve_managers(ops, &runtime->profiles, &weapon, &charge, &arc))
        return 0;
    owned = hf_arc3_sc_u32(charge.header, 0x10u);
    capacity = hf_arc3_sc_u32(charge.header, 0x0Cu);
    refs = (uintptr_t)hf_arc3_sc_u64(charge.header, 0x38u);
    if (owned > capacity || owned > HF_ARC3_SC_MAX_CONTEXTS || (owned > 0u && refs == 0u) ||
        (owned > 0u && !hf_arc3_sc_read(ops, refs, reference_bytes, (size_t)owned * 8u)))
        return 0;
    for (slot = 0u; slot < owned; ++slot)
    {
        uint8_t identity[24];
        uint8_t entity_reference[8];
        HfArc3ScMapResult weapon_map, state_map;
        uintptr_t entity_address, weapon_refs, state_refs, mode_array, state_array;
        uintptr_t weapon_ref_address, charge_ref_address, mode_address, state_address;
        uint32_t id, mode;
        int present, is_busy;
        int current_profile;
        HfArc3ScEntity* entity;
        size_t i;
        memcpy(entity_reference, reference_bytes + (size_t)slot * 8u, sizeof(entity_reference));
        entity_address = (uintptr_t)hf_arc3_sc_u64(entity_reference, 0u);
        if (entity_address == 0u || !hf_arc3_sc_read(ops, entity_address, identity, sizeof(identity)))
            return 0;
        if (memcmp(identity, HF_ARC3_CHARGE_OWNER, sizeof(HF_ARC3_CHARGE_OWNER)) != 0 ||
            (hf_arc3_sc_u32(identity, 20u) & 1u) == 0u)
            continue;
        id = hf_arc3_sc_u32(identity, 8u);
        for (i = 0u; i < found; ++i)
            if (runtime->entities[i].id == id)
                return 0;
        if (hf_arc3_sc_map(ops, weapon.header, sizeof(weapon.header), 0x30u, id, NULL,
                           &weapon_map) <= 0 ||
            weapon_map.index >= hf_arc3_sc_u32(weapon.header, 0x1Cu) ||
            hf_arc3_sc_map(ops, charge.header, sizeof(charge.header), 0x20u, id, NULL,
                           &state_map) <= 0 || state_map.index != slot)
            return 0;
        weapon_refs = (uintptr_t)hf_arc3_sc_u64(weapon.header, 0x48u);
        state_refs = (uintptr_t)hf_arc3_sc_u64(charge.header, 0x38u);
        mode_array = (uintptr_t)hf_arc3_sc_u64(weapon.header, 0x60u);
        state_array = (uintptr_t)hf_arc3_sc_u64(charge.header, 0x40u);
        if (weapon_refs == 0u || state_refs == 0u || mode_array == 0u || state_array == 0u ||
            weapon_map.index > (UINTPTR_MAX - weapon_refs) / 8u ||
            slot > (UINTPTR_MAX - state_refs) / 8u ||
            weapon_map.index > (UINTPTR_MAX - mode_array) / 12u ||
            slot > (UINTPTR_MAX - state_array) / 40u)
            return 0;
        weapon_ref_address = weapon_refs + (uintptr_t)weapon_map.index * 8u;
        charge_ref_address = state_refs + (uintptr_t)slot * 8u;
        mode_address = mode_array + (uintptr_t)weapon_map.index * 12u;
        state_address = state_array + (uintptr_t)slot * 40u;
        if (!hf_arc3_sc_read(ops, weapon_ref_address, reference_bytes, 8u) ||
            memcmp(reference_bytes, entity_reference, 8u) != 0 ||
            !hf_arc3_sc_read(ops, charge_ref_address, reference_bytes, 8u) ||
            memcmp(reference_bytes, entity_reference, 8u) != 0 ||
            !hf_arc3_sc_read(ops, mode_address, reference_bytes, 4u))
            return 0;
        mode = hf_arc3_sc_u32(reference_bytes, 0u);
        if (mode != 1u && mode != 2u)
            return 0;
        entity = &runtime->entities[found];
        memset(entity, 0, sizeof(*entity));
        entity->id = id;
        entity->entity = entity_address;
        memcpy(entity->identity, identity, sizeof(identity));
        memcpy(entity->entity_ref, entity_reference, sizeof(entity_reference));
        entity->weapon_ref_address = weapon_ref_address;
        entity->charge_ref_address = charge_ref_address;
        entity->mode_address = mode_address;
        entity->state_address = state_address;
        {
            HfArc3ScTransaction selector;
            uint32_t changed = 0u;
            char reason[256];
            int result;
            memset(&selector, 0, sizeof(selector));
            if (!hf_arc3_sc_patch_private_weapon(ops, &selector, weapon.manager, id))
                return 0;
            result = hf_arc3_sc_commit(ops, &selector, &changed, reason, sizeof(reason));
            if (result != HF_ARC3_APPLIED)
            {
                hf_arc3_sc_reason(runtime->status, sizeof(runtime->status), reason);
                return 0;
            }
        }
        current_profile = hf_arc3_sc_current_profile(ops, charge.manager, charge.header,
                                                      arc.manager, arc.header,
                                                      &runtime->profiles, id);
        if (current_profile < 0 ||
            !hf_arc3_sc_profile_present(ops, charge.header, arc.header, id, &present) ||
            !hf_arc3_sc_busy(ops, state_address, &is_busy))
            return 0;
        entity->applied_mode = (uint32_t)current_profile;
        if (hf_arc3_sc_transition_due(present, current_profile, mode, is_busy) &&
            !hf_arc3_sc_apply_entity_mode(ops, runtime, entity, mode))
            return 0;
        ++found;
    }
    runtime->entity_count = found;
    runtime->weapon_manager = weapon.manager;
    runtime->charge_manager = charge.manager;
    runtime->weapon_mode_array = (uintptr_t)hf_arc3_sc_u64(weapon.header, 0x60u);
    runtime->charge_state_array = (uintptr_t)hf_arc3_sc_u64(charge.header, 0x40u);
    if (found == 0u)
        (void)snprintf(runtime->status, sizeof(runtime->status), "Ready: waiting for ARC instance");
    else
        (void)snprintf(runtime->status, sizeof(runtime->status), "Ready: %u ARC instance(s)", (unsigned)found);
    return 1;
}

static int hf_arc3_sc_synchronize(const HfArc3MemoryOps* ops,
                                  HfArc3ScRuntime* runtime)
{
    HfArc3ScManager weapon, charge, arc;
    size_t i;
    if (runtime == NULL || !runtime->prepared || runtime->stopped)
        return 0;
    ++runtime->frame;
    if (runtime->entity_count == 0u || runtime->frame % 30u == 1u)
    {
        if (runtime->frame % 30u != 1u)
            return 1;
        if (!hf_arc3_sc_discover(ops, runtime))
        {
            if (runtime->status[0] == '\0')
                (void)snprintf(runtime->status, sizeof(runtime->status), "ARC instance discovery refused changed data");
            runtime->stopped = 1u;
            return 0;
        }
        return 1;
    }
    if (!hf_arc3_sc_resolve_managers(ops, &runtime->profiles, &weapon, &charge, &arc))
    {
        (void)snprintf(runtime->status, sizeof(runtime->status), "ARC manager data is unreadable; synchronization stopped");
        runtime->stopped = 1u;
        return 0;
    }
    if (!hf_arc3_sc_arrays_match(runtime, weapon.header, charge.header))
    {
        runtime->entity_count = 0u;
        (void)snprintf(runtime->status, sizeof(runtime->status), "ARC arrays changed; rebinding");
        return 1;
    }
    for (i = 0u; i < runtime->entity_count; ++i)
    {
        HfArc3ScEntity* entity = &runtime->entities[i];
        uint8_t identity[24], reference[8], mode_bytes[4];
        uint32_t mode;
        int busy, present, current_profile;
        if (!hf_arc3_sc_read(ops, entity->entity, identity, sizeof(identity)) ||
            memcmp(identity, entity->identity, sizeof(identity)) != 0 ||
            !hf_arc3_sc_read(ops, entity->weapon_ref_address, reference, sizeof(reference)) ||
            memcmp(reference, entity->entity_ref, sizeof(reference)) != 0 ||
            !hf_arc3_sc_read(ops, entity->charge_ref_address, reference, sizeof(reference)) ||
            memcmp(reference, entity->entity_ref, sizeof(reference)) != 0)
        {
            runtime->entity_count = 0u;
            (void)snprintf(runtime->status, sizeof(runtime->status), "ARC instance changed; rebinding");
            return 1;
        }
        if (!hf_arc3_sc_read(ops, entity->mode_address, mode_bytes, sizeof(mode_bytes)))
            goto refuse;
        mode = hf_arc3_sc_u32(mode_bytes, 0u);
        if ((mode != 1u && mode != 2u) || !hf_arc3_sc_busy(ops, entity->state_address, &busy) ||
            !hf_arc3_sc_profile_present(ops, charge.header, arc.header, entity->id, &present))
            goto refuse;
        current_profile = hf_arc3_sc_current_profile(ops, charge.manager, charge.header,
                                                      arc.manager, arc.header,
                                                      &runtime->profiles, entity->id);
        if (current_profile < 0)
            goto refuse;
        if ((hf_arc3_sc_transition_due(present, current_profile, mode, busy) ||
             mode != entity->applied_mode) && !busy &&
            !hf_arc3_sc_apply_entity_mode(ops, runtime, entity, mode))
            goto refuse;
        (void)snprintf(runtime->status, sizeof(runtime->status), "%s selected",
                       mode == 2u ? "Supercharge" : "Rapid Fire");
    }
    return 1;

refuse:
    if (runtime->status[0] == '\0')
        (void)snprintf(runtime->status, sizeof(runtime->status), "ARC mode or private ownership changed; synchronization stopped");
    runtime->stopped = 1u;
    return 0;
}

