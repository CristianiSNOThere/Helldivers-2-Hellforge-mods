#ifndef HF_NATIVE_REGISTRY_BRIDGE_INC
#define HF_NATIVE_REGISTRY_BRIDGE_INC
/* SDK 11 compatibility for the build-25480438 lookup consumers. Keep the
 * opcode and both consumer targets exact; a relocated table additionally
 * needs a row registered by this mod joined through the SDK's own API. */
enum { HF_REGISTRY_DAMAGE = 1, HF_REGISTRY_PROJECTILE = 2 };
static uint32_t HF_REGISTRY_DAMAGE_WITNESS_ID;
static uint32_t HF_REGISTRY_PROJECTILE_WITNESS_ID;

typedef struct HfRegistryConsumer
{
    uint32_t rva;
    uint8_t opcode[3];
} HfRegistryConsumer;

static const HfRegistryConsumer HF_REGISTRY_DAMAGE_CONSUMERS[2] = {
    {0x012A23FDu, {0x48u, 0x8Du, 0x0Du}},
    {0x007D251Cu, {0x48u, 0x8Du, 0x15u}}
};
static const HfRegistryConsumer HF_REGISTRY_PROJECTILE_CONSUMERS[2] = {
    {0x005F576Au, {0x48u, 0x8Du, 0x0Du}},
    {0x00616523u, {0x48u, 0x8Du, 0x0Du}}
};

static int hf_registry_address(uintptr_t address, size_t size, uintptr_t alignment)
{
    return address >= (uintptr_t)65536u &&
           address < (uintptr_t)UINT64_C(0x800000000000) &&
           size <= (uintptr_t)UINT64_C(0x800000000000) - address &&
           (address & (alignment - 1u)) == 0u;
}

static int hf_registry_read(const HfApi* api, uintptr_t address, void* output, size_t size)
{
    return api != NULL && api->read != NULL && output != NULL && size != 0u &&
           hf_registry_address(address, size, 1u) && api->read(address, output, size) == 0;
}

static int hf_registry_root(const HfApi* api, uintptr_t game, size_t image_size,
                            int kind, uintptr_t* root)
{
    const HfRegistryConsumer* consumers;
    uint32_t stock_rva, witness_id, row_id;
    uintptr_t resolved = 0u, witness = 0u, registered = 0u;
    size_t i;
    if (root == NULL || (kind != HF_REGISTRY_DAMAGE && kind != HF_REGISTRY_PROJECTILE) ||
        !hf_registry_address(game, image_size, 1u))
        return 0;
    consumers = kind == HF_REGISTRY_DAMAGE ? HF_REGISTRY_DAMAGE_CONSUMERS
                                           : HF_REGISTRY_PROJECTILE_CONSUMERS;
    stock_rva = kind == HF_REGISTRY_DAMAGE ? 0x037C60C0u : 0x037C7670u;
    if (stock_rva >= image_size)
        return 0;
    for (i = 0u; i < 2u; ++i)
    {
        uint8_t code[7];
        int32_t displacement;
        uintptr_t next, target;
        if (consumers[i].rva > image_size || 7u > image_size - consumers[i].rva ||
            !hf_registry_read(api, game + consumers[i].rva, code, sizeof(code)) ||
            memcmp(code, consumers[i].opcode, 3u) != 0)
            return 0;
        memcpy(&displacement, code + 3u, sizeof(displacement));
        next = game + consumers[i].rva + 7u;
        if ((displacement < 0 && (uint64_t)(-(int64_t)displacement) > next) ||
            (displacement >= 0 && (uintptr_t)displacement > UINTPTR_MAX - next))
            return 0;
        target = displacement < 0 ? next - (uintptr_t)(-(int64_t)displacement)
                                  : next + (uintptr_t)displacement;
        if (!hf_registry_address(target, sizeof(uintptr_t), 8u) ||
            (i != 0u && target != resolved))
            return 0;
        resolved = target;
    }
    if (resolved == game + stock_rva)
    {
        *root = resolved;
        return 1;
    }
    /* A redirect into the game image is never a supported SDK allocation. */
    if (resolved >= game && resolved < game + image_size)
        return 0;
    if (api == NULL || api->size < offsetof(HfApi, damage_row) + sizeof(api->damage_row))
        return 0;
    witness_id = kind == HF_REGISTRY_DAMAGE ? HF_REGISTRY_DAMAGE_WITNESS_ID
                                           : HF_REGISTRY_PROJECTILE_WITNESS_ID;
    if (kind == HF_REGISTRY_DAMAGE)
    {
        if (witness_id < 650u || api->damage_row == NULL)
            return 0;
        registered = api->damage_row(witness_id);
    }
    else
    {
        if (witness_id < 351u || api->projectile_row == NULL)
            return 0;
        registered = api->projectile_row(witness_id);
    }
    if (witness_id > 4096u || !hf_registry_address(registered, 4u, 4u) ||
        !hf_registry_read(api, resolved + (uintptr_t)witness_id * 8u, &witness, sizeof(witness)) ||
        witness != registered || !hf_registry_read(api, witness, &row_id, sizeof(row_id)) ||
        row_id != witness_id)
        return 0;
    *root = resolved;
    return 1;
}

static int hf_registry_slot(const HfApi* api, uintptr_t game, size_t image_size,
                            uint32_t source_rva, uint32_t identity, uint32_t row_size,
                            uintptr_t* slot)
{
    int kind = row_size == 76u ? HF_REGISTRY_DAMAGE :
               row_size == 272u ? HF_REGISTRY_PROJECTILE : 0;
    uint32_t stock_rva = kind == HF_REGISTRY_DAMAGE ? 0x037C60C0u : 0x037C7670u;
    uintptr_t root;
    if (slot == NULL || kind == 0 || identity > 4096u ||
        source_rva != stock_rva + identity * 8u ||
        source_rva > image_size || 8u > image_size - source_rva ||
        !hf_registry_root(api, game, image_size, kind, &root) ||
        root > UINTPTR_MAX - (uintptr_t)identity * 8u)
        return 0;
    *slot = root + (uintptr_t)identity * 8u;
    return hf_registry_address(*slot, 8u, 8u);
}

static int hf_registry_pin_matches(const HfApi* api, uintptr_t game, size_t image_size,
                                   uint32_t rva, const uint8_t* expected,
                                   const uint8_t* actual, size_t size)
{
    uintptr_t root;
    /* Both accepted damage consumers are relocated by SDK 11. */
    if ((rva == 0x012A23FDu || rva == 0x011F8EAEu) && size == 7u)
    {
        static const uint8_t sources[2][7] = {
            {0x48u,0x8Du,0x0Du,0xBCu,0x3Cu,0x52u,0x02u},
            {0x48u,0x8Du,0x0Du,0x0Bu,0xD2u,0x5Cu,0x02u}
        };
        const uint8_t* source = sources[rva == 0x012A23FDu ? 0u : 1u];
        int32_t displacement;
        uint8_t current[7];
        memcpy(&displacement, actual + 3u, sizeof(displacement));
        return memcmp(expected, source, 7u) == 0 && memcmp(actual, source, 3u) == 0 &&
               hf_registry_root(api, game, image_size, HF_REGISTRY_DAMAGE, &root) &&
               (int64_t)(game + rva + 7u) + displacement == (int64_t)root &&
               hf_registry_read(api, game + rva, current, sizeof(current)) &&
               memcmp(actual, current, sizeof(current)) == 0;
    }
    return memcmp(expected, actual, size) == 0;
}
#endif
