/* Prove that every native component lookup selects an exclusive SDK-owned
 * appended row, while the original One-Two still selects its pinned stock row. */
static int hf_punisher_child_owned(const HfApi* api, const HfPunisherContent* content,
    const HfPunisherRecord rows[HF_PUNISHER_ROW_COUNT], char* reason, size_t capacity)
{
    uintptr_t game, root, again;
    uint8_t entries[730u * 16u], recheck[730u * 16u], header[32], original[HF_PUNISHER_WEAPON_SIZE];
    if (!api || !api->module_base || !api->component_record || !api->read || !content ||
        !content->child_entity || content->child_entity == HF_PUNISHER_CHILD_RESOURCE)
        goto refused;
    game = api->module_base("game.dll", NULL);
    if (!game || api->read(game + 0x346BF98u, &root, 8u) || root < 65536u ||
        root >= UINT64_C(0x800000000000) - 0xF13000u)
        goto refused;
    for (unsigned family = 0u; family < 4u; family++)
    {
        const HfPunisherChildShape* shape = &HF_PUNISHER_CHILD_SHAPES[family];
        unsigned row = HF_PUNISHER_CHILD_WEAPON + family;
        uintptr_t table, own_address, stock_address;
        uint32_t own_stride = 0u, stock_stride = 0u, own_index = UINT32_MAX;
        unsigned own_hits = 0u, stock_hits = 0u, own_aliases = 0u, stock_aliases = 0u;
        if (api->read(root + 0xF12478u + 8u * shape->component, &table, 8u) || table < 65536u ||
            table >= UINT64_C(0x800000000000) - 0x100000u ||
            api->read(table - 32u, header, 32u) || memcmp(header, shape->header, 32u) ||
            api->read(table, entries, (size_t)shape->slots * 16u)) goto refused;
        for (uint32_t slot = 0u; slot < shape->slots; slot++)
        {
            uint64_t identity;
            uint32_t index = hf_punisher_u32(entries, slot * 16u + 8u);
            memcpy(&identity, entries + slot * 16u, 8u);
            if (identity == content->child_entity)
            {
                if (hf_punisher_u32(entries, slot * 16u + 12u)) goto refused;
                own_index = index; own_hits++;
            }
            if (identity == HF_PUNISHER_CHILD_RESOURCE)
            {
                if (index != shape->stock_record || hf_punisher_u32(entries, slot * 16u + 12u)) goto refused;
                stock_hits++;
            }
        }
        if (own_hits != 1u || stock_hits != 1u || own_index < shape->first ||
            own_index >= shape->first + 64u) goto refused;
        for (unsigned target = 0u; target < 2u; target++)
        {
            uint64_t key = target ? HF_PUNISHER_CHILD_RESOURCE : content->child_entity;
            uint32_t slot = (uint32_t)(key % shape->slots);
            unsigned found = 0u;
            for (uint32_t probe = 0u; probe < shape->slots; probe++)
            {
                uint64_t identity; memcpy(&identity, entries + slot * 16u, 8u);
                if (!identity) break;
                if (identity == key) { found = 1u; break; }
                slot = (slot + 1u) % shape->slots;
            }
            if (!found) goto refused;
        }
        for (uint32_t slot = 0u; slot < shape->slots; slot++)
        {
            uint64_t identity;
            memcpy(&identity, entries + slot * 16u, 8u);
            if (!identity) continue;
            uint32_t index = hf_punisher_u32(entries, slot * 16u + 8u);
            if (index == own_index) own_aliases++;
            if (index == shape->stock_record) stock_aliases++;
        }
        if (own_aliases != 1u || stock_aliases != 1u) goto refused;
        own_address = table + (size_t)shape->slots * 16u + (size_t)own_index * shape->stride;
        stock_address = table + (size_t)shape->slots * 16u + (size_t)shape->stock_record * shape->stride;
        if (own_address != rows[row].address || rows[row].stride != shape->stride ||
            hf_punisher_range_overlaps(own_address, shape->stride, stock_address, shape->stride) ||
            api->component_record(HF_PUNISHER_SOURCE[row].component, content->child_entity, &own_stride) != own_address ||
            api->component_record(HF_PUNISHER_SOURCE[row].component, HF_PUNISHER_CHILD_RESOURCE, &stock_stride) != stock_address ||
            own_stride != shape->stride || stock_stride != shape->stride ||
            api->read(stock_address, original, shape->stride) ||
            !hf_punisher_hash_matches(original, shape->stride, HF_PUNISHER_SOURCE[row].base_sha256) ||
            api->read(root + 0xF12478u + 8u * shape->component, &again, 8u) || again != table ||
            api->read(table, recheck, (size_t)shape->slots * 16u) ||
            memcmp(recheck, entries, (size_t)shape->slots * 16u)) goto refused;
    }
    if (api->read(game + 0x346BF98u, &again, 8u) || again != root) goto refused;
    return hf_punisher_unit_owned(api, content, root, reason, capacity);
refused:
    (void)snprintf(reason, capacity, "private child registry ownership or stock One-Two source changed");
    return 0;
}
