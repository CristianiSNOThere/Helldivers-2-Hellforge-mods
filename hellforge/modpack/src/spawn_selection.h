/* Native 0x878700 scans global descriptors inline, so its ID getter detours do
 * not see private choices. Supply only its explicit magazine override argument
 * from the same eligible local player's native saved selection. No table edits. */
#ifndef HF_SICKLE_SPAWN_SELECTION_H
#define HF_SICKLE_SPAWN_SELECTION_H
#include "entity_reader.h"
#include "catalog_children.h"
#include "heatsink_overlay.h"
static int hf_spawn_selection(HfEntityRead read,void* ctx,uintptr_t game,uintptr_t exe,
    uint32_t avatar,uint64_t resource,uintptr_t overrides,uint32_t mask,
    uint64_t output[10],uint32_t* output_mask) {
    unsigned family=resource==HF_SICKLE_ENTITY?0:resource==HF_DOUBLE_EDGE_ENTITY?1:2;
    if (family==2 || (overrides && (mask&(1u<<5)))) return 0;
    uintptr_t players,catalog,player;uint32_t index,invalid,sentinel;
    if (!hf_entity_read(read,ctx,game+0x3483c34,&invalid,4) ||
        !hf_entity_read(read,ctx,game+0x3483c4c,&sentinel,4)) return -1;
    /* The native sentinel resolves the current local player first. Mirror it
     * with bounded, repeat-validated reads instead of guessing a player ID. */
    if (avatar==sentinel) {
        uintptr_t local,rows;uint32_t n;
        if (!hf_entity_ptr(read,ctx,game+0x3326468,&local) ||
            !hf_entity_read(read,ctx,local+0x88,&n,4)) return -1;
        if (!n) return 0;
        if (!hf_entity_ptr(read,ctx,local+0xe8,&rows) || !hf_entity_read(read,ctx,rows+8,&avatar,4)) return -1;
    }
    if (avatar==invalid || !hf_entity_alive(read,ctx,exe,avatar)) return 0;
    if (!hf_entity_ptr(read,ctx,game+0x33264f8,&players) ||
        !hf_entity_hash(read,ctx,players+0x930,avatar,&index) || index>=65536 ||
        !hf_entity_ptr(read,ctx,players+0x948+(size_t)index*8,&player)) return -1;
    uint8_t flag;
    if (!hf_entity_read(read,ctx,player+0x14,&flag,1)) return -1;
    if (!(flag&1)) return 0; /* Native remote/custom-override route owns this call. */
    if (!hf_entity_ptr(read,ctx,game+0x347cef8,&catalog)) return -1;
    uint32_t weapon=HF_PRIVATE_CHILDREN[family*2].weapon;uintptr_t address=0;uint8_t row[148];
    for (unsigned i=0;i<512;i++) {
        uint32_t id;uintptr_t at=catalog+0x178a4ec+(size_t)i*148;
        if (!hf_entity_read(read,ctx,at,&id,4)) return -1;
        if (id==weapon) { if (address) return -1;address=at; }
    }
    if (!address || !hf_entity_read(read,ctx,address,row,148)) return -1;
    int32_t active;memcpy(&active,row+8,4);
    if (active<0 || active>=3) return 0;
    /* Native cache includes slot0 before optic1; JSON profile arrays start at
     * optic1. Magazine is cache index5, not profile array index4. */
    uint32_t selected=hf_cat_u32(row+12+(size_t)active*40+5*4);int choice=0;
    for (unsigned i=0;i<4;i++) if (selected==HF_PRIVATE_CHILDREN[i].id) choice=(int)i+1;
    if (!choice) return 0;
    if ((unsigned)(choice-1)/2!=family) return -1;
    uint64_t original[10]={0};
    if (overrides && !hf_entity_read(read,ctx,overrides,original,sizeof(original))) return -1;
    /* An explicit incoming private key in another slot is a conflicting owner. */
    for (unsigned i=0;i<10;i++) if (overrides && (mask&(1u<<i)))
        for (unsigned j=0;j<4;j++) if (original[i]==HF_HEATSINK_KEYS[j]) return -1;
    uintptr_t now_players,now_catalog,now_player;uint32_t now_index;uint8_t now_flag,again[148];
    if (!hf_entity_ptr(read,ctx,game+0x33264f8,&now_players) || now_players!=players ||
        !hf_entity_ptr(read,ctx,game+0x347cef8,&now_catalog) || now_catalog!=catalog ||
        !hf_entity_hash(read,ctx,players+0x930,avatar,&now_index) || now_index!=index ||
        !hf_entity_ptr(read,ctx,players+0x948+(size_t)index*8,&now_player) || now_player!=player ||
        !hf_entity_read(read,ctx,player+0x14,&now_flag,1) || now_flag!=flag ||
        !hf_entity_read(read,ctx,address,again,148) || memcmp(again,row,148) ||
        !hf_entity_alive(read,ctx,exe,avatar)) return -1;
    memcpy(output,original,sizeof(original));output[5]=HF_HEATSINK_KEYS[choice-1];*output_mask=mask|(1u<<5);
    return choice;
}
#endif
