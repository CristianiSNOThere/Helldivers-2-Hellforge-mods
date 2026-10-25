/* Source-guarded ARC-3 Rapid profile transaction. */
typedef struct HfArc3MemoryOps
{
    void* context;
    int (*read)(void* context, uintptr_t address, void* output, size_t size);
    int (*write)(void* context, uintptr_t address, const void* input, size_t size,
                 int protected_write);
    int (*can_write)(void* context, uintptr_t address, size_t size, int protected_write);
    int (*data_access)(void* context, uintptr_t address, size_t size);
} HfArc3MemoryOps;

typedef enum HfArc3ApplyResult
{
    HF_ARC3_REFUSED = 0,
    HF_ARC3_APPLIED = 1,
    HF_ARC3_FATAL = -1
} HfArc3ApplyResult;

typedef struct HfArc3Patch
{
    uintptr_t address;
    uint8_t before[4];
    uint8_t after[4];
    const char* label;
    uint32_t protected_write;
    uint32_t attempted;
} HfArc3Patch;

typedef struct HfArc3Record
{
    uintptr_t address;
    uint32_t size;
    const uint8_t* source;
    uint32_t allow_autofire_bit;
    uint8_t snapshot[1232];
} HfArc3Record;

static void hf_arc3_reason(char* reason, size_t capacity, const char* message)
{
    if (reason != NULL && capacity != 0u)
        (void)snprintf(reason, capacity, "%s", message != NULL ? message : "ARC-3 transaction refused");
}

static int hf_arc3_record_matches(const HfArc3Record* record, const uint8_t* bytes)
{
    size_t i;
    if (record == NULL || bytes == NULL)
        return 0;
    for (i = 0u; i < record->size; ++i)
    {
        if (record->allow_autofire_bit && i == 184u)
        {
            if (bytes[i] != 0u && bytes[i] != 1u)
                return 0;
        }
        else if (bytes[i] != record->source[i])
            return 0;
    }
    return 1;
}

static void hf_arc3_add_patch(HfArc3Patch* patches, size_t* count, uintptr_t address,
                              const uint8_t* source, uint32_t offset,
                              const uint8_t after[4], const char* label, uint32_t protected_write)
{
    HfArc3Patch* patch = &patches[*count];
    patch->address = address + offset;
    memcpy(patch->before, source + offset, sizeof(patch->before));
    memcpy(patch->after, after, sizeof(patch->after));
    patch->label = label;
    patch->protected_write = protected_write;
    patch->attempted = 0u;
    ++*count;
}

static int hf_arc3_restore(const HfArc3MemoryOps* ops, HfArc3Patch* patches,
                           size_t attempted, char* reason, size_t capacity)
{
    int restored = 1;
    while (attempted > 0u)
    {
        HfArc3Patch* patch = &patches[--attempted];
        uint8_t current[4];
        if (!patch->attempted)
            continue;
        if (!ops->read(ops->context, patch->address, current, sizeof(current)))
        {
            restored = 0;
            continue;
        }
        if (memcmp(current, patch->after, sizeof(current)) == 0)
        {
            if (!ops->write(ops->context, patch->address, patch->before, sizeof(patch->before),
                            (int)patch->protected_write) ||
                !ops->read(ops->context, patch->address, current, sizeof(current)) ||
                memcmp(current, patch->before, sizeof(current)) != 0)
                restored = 0;
        }
        else if (memcmp(current, patch->before, sizeof(current)) != 0)
            restored = 0;
    }
    if (!restored)
        hf_arc3_reason(reason, capacity, "FATAL: ARC-3 Rapid transaction rollback was incomplete");
    return restored;
}

static HfArc3ApplyResult hf_arc3_apply_rapid(const HfArc3MemoryOps* ops,
                                             const uintptr_t record_addresses[4],
                                             uint32_t* changed, char* reason, size_t capacity)
{
    static const uint8_t u226[4] = {0xE2u, 0x00u, 0x00u, 0x00u};
    static const uint8_t u90[4] = {0x5Au, 0x00u, 0x00u, 0x00u};
    static const uint8_t u4[4] = {0x04u, 0x00u, 0x00u, 0x00u};
    static const uint8_t u25[4] = {0x19u, 0x00u, 0x00u, 0x00u};
    static const uint8_t u2[4] = {0x02u, 0x00u, 0x00u, 0x00u};
    static const uint8_t f12[4] = {0x9Au, 0x99u, 0x99u, 0x3Fu};
    static const uint8_t f45[4] = {0x00u, 0x00u, 0x34u, 0x42u};
    static const uint8_t f020_065[4] = {0xD9u, 0x89u, 0x9Du, 0x3Eu};
    static const uint8_t f022_065[4] = {0xD5u, 0x4Au, 0xADu, 0x3Eu};
    static const uint8_t f024_065[4] = {0xD1u, 0x0Bu, 0xBDu, 0x3Eu};
    static const uint8_t f085[4] = {0x9Au, 0x99u, 0x59u, 0x3Fu};
    static const uint8_t f04[4] = {0xCDu, 0xCCu, 0xCCu, 0x3Eu};
    HfArc3Record records[4];
    HfArc3Patch patches[14];
    const uint8_t* source[4] = {
        HF_ARC3_DAMAGE_ROW, HF_ARC3_ARC_ROW, HF_ARC3_CHARGE_ROW, HF_ARC3_WEAPON_ROW
    };
    const uint32_t sizes[4] = {76u, 104u, 216u, 1232u};
    size_t patch_count = 0u, attempted = 0u, i;

    if (changed != NULL)
        *changed = 0u;
    if (ops == NULL || ops->read == NULL || ops->write == NULL || ops->can_write == NULL ||
        ops->data_access == NULL ||
        record_addresses == NULL)
    {
        hf_arc3_reason(reason, capacity, "ARC-3 memory operations are unavailable");
        return HF_ARC3_REFUSED;
    }
    memset(records, 0, sizeof(records));
    for (i = 0u; i < 4u; ++i)
    {
        records[i].address = record_addresses[i];
        records[i].size = sizes[i];
        records[i].source = source[i];
        records[i].allow_autofire_bit = i == 2u;
        if (record_addresses[i] == 0u ||
            !ops->data_access(ops->context, record_addresses[i], sizes[i]) ||
            !ops->read(ops->context, record_addresses[i], records[i].snapshot, sizes[i]) ||
            !hf_arc3_record_matches(&records[i], records[i].snapshot))
        {
            hf_arc3_reason(reason, capacity, "ARC-3 Rapid source record mismatch; no edit applied");
            return HF_ARC3_REFUSED;
        }
    }

    hf_arc3_add_patch(patches, &patch_count, records[0].address, records[0].source, 4u, u226, "damage normal", 0u);
    hf_arc3_add_patch(patches, &patch_count, records[0].address, records[0].source, 8u, u90, "damage durable", 0u);
    hf_arc3_add_patch(patches, &patch_count, records[0].address, records[0].source, 28u, u4, "damage demolition", 0u);
    hf_arc3_add_patch(patches, &patch_count, records[0].address, records[0].source, 32u, u25, "damage force", 0u);
    hf_arc3_add_patch(patches, &patch_count, records[0].address, records[0].source, 36u, u2, "damage impulse", 0u);
    hf_arc3_add_patch(patches, &patch_count, records[0].address, records[0].source, 48u, f12, "damage stun buildup", 0u);
    hf_arc3_add_patch(patches, &patch_count, records[1].address, records[1].source, 8u, f45, "range", 1u);
    hf_arc3_add_patch(patches, &patch_count, records[2].address, records[2].source, 0u, f020_065, "charge first", 1u);
    hf_arc3_add_patch(patches, &patch_count, records[2].address, records[2].source, 24u, f022_065, "charge second", 1u);
    hf_arc3_add_patch(patches, &patch_count, records[2].address, records[2].source, 48u, f024_065, "charge third", 1u);
    hf_arc3_add_patch(patches, &patch_count, records[2].address, records[2].source, 80u, f085, "charge normal multiplier", 1u);
    hf_arc3_add_patch(patches, &patch_count, records[2].address, records[2].source, 84u, f085, "charge durable multiplier", 1u);
    hf_arc3_add_patch(patches, &patch_count, records[3].address, records[3].source, 76u, f04, "camera climb horizontal", 1u);
    hf_arc3_add_patch(patches, &patch_count, records[3].address, records[3].source, 80u, f04, "camera climb vertical", 1u);

    for (i = 0u; i < patch_count; ++i)
    {
        uint8_t current[4];
        if (!ops->data_access(ops->context, patches[i].address, sizeof(current)) ||
            !ops->can_write(ops->context, patches[i].address, sizeof(current),
                            (int)patches[i].protected_write) ||
            !ops->read(ops->context, patches[i].address, current, sizeof(current)) ||
            memcmp(current, patches[i].before, sizeof(current)) != 0)
        {
            hf_arc3_reason(reason, capacity, "ARC-3 Rapid field changed before mutation; no edit applied");
            return HF_ARC3_REFUSED;
        }
    }
    for (i = 0u; i < patch_count; ++i)
    {
        uint8_t current[4];
        if (memcmp(patches[i].before, patches[i].after, sizeof(current)) == 0)
            continue;
        patches[i].attempted = 1u;
        attempted = i + 1u;
        if (!ops->write(ops->context, patches[i].address, patches[i].after, sizeof(current),
                        (int)patches[i].protected_write) ||
            !ops->read(ops->context, patches[i].address, current, sizeof(current)) ||
            memcmp(current, patches[i].after, sizeof(current)) != 0)
        {
            if (!hf_arc3_restore(ops, patches, attempted, reason, capacity))
                return HF_ARC3_FATAL;
            hf_arc3_reason(reason, capacity, "ARC-3 Rapid write failed; earlier edits were restored");
            return HF_ARC3_REFUSED;
        }
        if (changed != NULL)
            ++*changed;
    }
    for (i = 0u; i < 4u; ++i)
    {
        uint8_t current[1232];
        uint8_t expected[1232];
        size_t j;
        memcpy(expected, records[i].snapshot, records[i].size);
        for (j = 0u; j < patch_count; ++j)
            if (patches[j].address >= records[i].address &&
                patches[j].address + sizeof(patches[j].after) <= records[i].address + records[i].size)
                memcpy(expected + (patches[j].address - records[i].address), patches[j].after,
                       sizeof(patches[j].after));
        if (!ops->read(ops->context, records[i].address, current, records[i].size) ||
            memcmp(current, expected, records[i].size) != 0)
        {
            if (!hf_arc3_restore(ops, patches, patch_count, reason, capacity))
                return HF_ARC3_FATAL;
            hf_arc3_reason(reason, capacity, "ARC-3 Rapid full-record readback failed; edits were restored");
            return HF_ARC3_REFUSED;
        }
    }
    hf_arc3_reason(reason, capacity,
                   "Applied: ARC-3 range 55 -> 45 m; charge 1.0/1.1/1.2 -> 0.307692/0.338462/0.369231 s; damage 250/100 -> 226/90; Stun Medium buildup 8 -> 1.2; demolition/force strength/impulse 20/35/10 -> 4/25/2; camera climb 1.0/1.0 -> 0.4/0.4.");
    return HF_ARC3_APPLIED;
}
