static int hf_punisher_unit_owned(const HfApi* api, const HfPunisherContent* content,
    uintptr_t root, char* reason, size_t capacity)
{
    const HfPunisherChildShape* s = &HF_PUNISHER_CHILD_UNIT_SHAPE;
    uintptr_t table, again, own, stock;
    uint8_t mapping[3382u*16u], recheck[3382u*16u], header[32], source[136], model[136];
    uint32_t index = UINT32_MAX, stride = 0u;
    unsigned mine = 0u, original = 0u, aliases = 0u, source_aliases = 0u;
    if (api->read(root + 0xF12478u + 8u*s->component, &table, 8u) ||
        table < 65536u || table >= UINT64_C(0x800000000000)-0x100000u ||
        api->read(table-32u, header, 32u) || memcmp(header, s->header, 32u) ||
        api->read(table, mapping, sizeof(mapping))) goto refused;
    for (uint32_t slot = 0u; slot < s->slots; slot++)
    {
        uint64_t key; memcpy(&key, mapping + slot*16u, 8u);
        if (key == content->child_entity)
        { mine++; index = hf_punisher_u32(mapping, slot*16u+8u); if (hf_punisher_u32(mapping,slot*16u+12u)) goto refused; }
        if (key == HF_PUNISHER_CHILD_RESOURCE)
        { original++; if (hf_punisher_u32(mapping,slot*16u+8u) != s->stock_record || hf_punisher_u32(mapping,slot*16u+12u)) goto refused; }
    }
    if (mine != 1u || original != 1u || index < s->first || index >= s->first+64u) goto refused;
    for (uint32_t slot = 0u; slot < s->slots; slot++)
    {
        uint64_t key; memcpy(&key, mapping + slot*16u, 8u); if (!key) continue;
        uint32_t record = hf_punisher_u32(mapping,slot*16u+8u);
        aliases += record == index; source_aliases += record == s->stock_record;
    }
    if (aliases != 1u || source_aliases != 1u) goto refused;
    for (unsigned target = 0u; target < 2u; target++)
    {
        uint64_t key = target ? HF_PUNISHER_CHILD_RESOURCE : content->child_entity;
        uint32_t slot = (uint32_t)(key%s->slots);unsigned found=0u;
        for (uint32_t probe=0u;probe<s->slots;probe++)
        {
            uint64_t identity;memcpy(&identity,mapping+slot*16u,8u);
            if (!identity) break;
            if (identity==key) {found=1u;break;}
            slot=(slot+1u)%s->slots;
        }
        if (!found) goto refused;
    }
    own = table + (size_t)s->slots*16u + (size_t)index*136u;
    stock = table + (size_t)s->slots*16u + (size_t)s->stock_record*136u;
    if (api->component_record("UnitComponentData", content->child_entity, &stride) != own || stride != 136u ||
        api->component_record("UnitComponentData", HF_PUNISHER_CHILD_RESOURCE, &stride) != stock || stride != 136u ||
        api->read(stock, source, 136u) || !hf_punisher_hash_matches(source, 136u, HF_PUNISHER_CHILD_UNIT_SHA256) ||
        api->read(own, model, 136u) || memcmp(model, &content->child_entity, 8u)) goto refused;
    memcpy(model, &((uint64_t){HF_PUNISHER_CHILD_RESOURCE}), 8u);
    if (memcmp(model, source, 136u) || api->read(root+0xF12478u+8u*s->component,&again,8u) || again != table ||
        api->read(table-32u,model,32u) || memcmp(model,header,32u) ||
        api->read(table,recheck,sizeof(recheck)) || memcmp(recheck,mapping,sizeof(mapping))) goto refused;
    return 1;
refused:
    (void)snprintf(reason,capacity,"private child model record is missing, shared or no longer names its private unit");
    return 0;
}
