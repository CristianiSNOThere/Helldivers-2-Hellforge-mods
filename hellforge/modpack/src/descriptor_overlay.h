/* Append authored choices to the game's allocated per-weapon list. The global
 * descriptor count and arrays are never enlarged. Ownership is supplied only
 * after a same-generation catalogue/parent check by the runtime adapter. */
#ifndef HF_SICKLE_DESCRIPTOR_OVERLAY_H
#define HF_SICKLE_DESCRIPTOR_OVERLAY_H
#include "catalog_children.h"
#include "heatsink_overlay.h"
typedef struct HfHeatsinkDescriptors {
    uint8_t rows[4][88];
    uint32_t magazine_slot;
    uint64_t generation;
} HfHeatsinkDescriptors;
static void hf_desc_put64(uint8_t* p,uint64_t value) { memcpy(p,&value,8); }
/* Donor scalar fields are compared to source-pinned expected rows, with the
 * three runtime pointer fields normalized. Slot data must be read separately
 * and proven to be exactly one magazine slot before calling this builder. */
static int hf_desc_build(HfHeatsinkDescriptors* out,const uint8_t donors[4][88],
    const uint8_t expected[4][88],const char* const names[4],const uint32_t text[4][3],uint64_t generation) {
    if (!out || !donors || !expected || !names || !text) return 0;
    for (unsigned i=0;i<4;i++) {
        uint8_t actual[88],trusted[88];memcpy(actual,donors[i],88);memcpy(trusted,expected[i],88);
        memset(actual,0,8);memset(trusted,0,8);memset(actual+48,0,8);memset(trusted+48,0,8);
        memset(actual+64,0,8);memset(trusted+64,0,8);
        if (!names[i] || memcmp(actual,trusted,88) || hf_cat_u32(actual+8)!=HF_PRIVATE_CHILDREN[i].donor_id ||
            hf_cat_u32(actual+56)!=1 || hf_cat_u32(actual+72)!=0) return 0;
    }
    memset(out,0,sizeof(*out));out->magazine_slot=5;out->generation=generation;
    for (unsigned i=0;i<4;i++) {
        uint8_t* r=out->rows[i];memcpy(r,donors[i],88);
        hf_desc_put64(r,(uintptr_t)names[i]);hf_cat_put32(r+8,HF_PRIVATE_CHILDREN[i].id);
        for (unsigned j=0;j<3;j++) hf_cat_put32(r+12+j*4,text[i][j]);
        hf_cat_put32(r+24,0);hf_desc_put64(r+32,HF_HEATSINK_KEYS[i]);
        hf_desc_put64(r+48,(uintptr_t)&out->magazine_slot);hf_desc_put64(r+56,1);
        hf_desc_put64(r+64,0);hf_desc_put64(r+72,0);
    }
    return 1;
}
/* 0 passthrough; 2 appended; negative refusal. The caller retains capacity from
 * before the original overwrites its in/out count, and validates the buffer. */
static int hf_desc_append(const HfHeatsinkDescriptors* owned,uint64_t generation,uint32_t weapon,
    uint32_t mask,uint8_t include_locked,uint8_t parent_owned,uint8_t* list,uint32_t capacity,uint32_t* count) {
    unsigned family=weapon==0x6AF4EC48u?0:weapon==0x126DB14Fu?1:2;
    if (family==2 || !(mask&(1u<<5)) || (!include_locked && !parent_owned)) return 0;
    if (!owned || owned->generation!=generation || !list || !count || *count>capacity ||
        capacity>4096 || capacity-*count<2) return -1;
    for (uint32_t i=0;i<*count;i++) for (unsigned j=0;j<4;j++) {
        const uint8_t* r=list+(size_t)i*88;
        uint64_t key;memcpy(&key,r+32,8);
        if (hf_cat_u32(r+8)==HF_PRIVATE_CHILDREN[j].id || key==HF_HEATSINK_KEYS[j]) return -2;
    }
    for (unsigned i=0;i<2;i++) memcpy(list+(size_t)(*count+i)*88,owned->rows[family*2+i],88);
    *count+=2;return 2;
}
#endif
