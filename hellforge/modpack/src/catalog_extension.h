#ifndef HF_HEATSINK_CATALOG_EXTENSION_H
#define HF_HEATSINK_CATALOG_EXTENSION_H
#include "catalog_ownership.h"
/* Scope of Authority 2.14.15 appends G-11 after native catalogue finalization.
 * Accept only its exact G-6 clone, preserving the existing catalogue root and
 * every original item index. This function never writes catalogue data. */
static int hf_catalog_extension(HfRowRead read,void* context,uintptr_t root,uint32_t before,uint32_t after) {
    uint8_t appended[184],expected[184],map[24],expected_map[24];uint32_t frag=UINT32_MAX;
    if (!read || !root || !before || before>=4096u || after!=before+1u ||
        read(context,root+0x1ce4u+(size_t)before*184u,appended,184u) ||
        hf_cat_u32(appended)!=before || hf_cat_u32(appended+4)!=0xe2e872c1u ||
        hf_cat_u32(appended+8)!=0xe2e872c1u || hf_cat_u32(appended+12)!=1u ||
        (hf_cat_u32(appended+20)!=2u && hf_cat_u32(appended+20)!=4u)) return 0;
    for (uint32_t i=0;i<before;i++) {
        uint8_t row[16];
        if (read(context,root+0x1ce4u+(size_t)i*184u,row,16u) || hf_cat_u32(row)!=i) return 0;
        uint32_t id=hf_cat_u32(row+8);
        if (id==0xe2e872c1u) return 0;
        if (id==0xc4736993u) {
            if (frag!=UINT32_MAX || hf_cat_u32(row+4)!=id || hf_cat_u32(row+12)!=1u) return 0;
            frag=i;
        }
    }
    if (frag==UINT32_MAX || read(context,root+0x1ce4u+(size_t)frag*184u,expected,184u) ||
        read(context,root+0xb9ce4u+(size_t)frag*24u,expected_map,24u) ||
        read(context,root+0xb9ce4u+(size_t)before*24u,map,24u) ||
        hf_cat_u32(expected_map)!=frag || hf_cat_u32(expected_map+4)!=0xc4736993u ||
        hf_cat_u32(expected_map+8)!=0xc4736993u) return 0;
    hf_cat_put32(expected,before);hf_cat_put32(expected+4,0xe2e872c1u);hf_cat_put32(expected+8,0xe2e872c1u);
    hf_cat_put32(expected+20,hf_cat_u32(appended+20));
    hf_cat_put32(expected_map,before);hf_cat_put32(expected_map+4,0xe2e872c1u);hf_cat_put32(expected_map+8,0xe2e872c1u);
    return !memcmp(expected,appended,184u) && !memcmp(expected_map,map,24u);
}
#endif
