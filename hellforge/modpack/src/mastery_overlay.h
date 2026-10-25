/* Mod-owned mastery views. Original package identity and all old level data
 * survive unchanged; borrowed game pointers are valid only in their generation.
 * Publish on the game thread, invalidate before a progression reload, and free
 * only after readers quiesce. This file does not hook or mutate game memory. */
#ifndef HF_SICKLE_MASTERY_OVERLAY_H
#define HF_SICKLE_MASTERY_OVERLAY_H
#include "catalog_children.h"
#define HF_MASTERY_LEVEL_BYTES 0x848u
#define HF_MASTERY_MAX_LEVELS 100u
#define HF_MASTERY_MAX_UNLOCKS 256u
typedef int (*HfMasteryRead)(void*,uintptr_t,void*,size_t); /* 0 = success */
typedef struct HfMasteryView {
    uint8_t package[112], source_package[112];
    uint8_t *levels,*source_levels,*unlocks[HF_MASTERY_MAX_LEVELS];
    uintptr_t source;
    uint64_t generation;
    uint32_t count;
} HfMasteryView;
static uint64_t hf_mastery_u64(const uint8_t* p) { uint64_t v;memcpy(&v,p,8);return v; }
static void hf_mastery_put64(uint8_t* p,uint64_t v) { memcpy(p,&v,8); }
static void hf_mastery_free(HfMasteryView* v) {
    if (!v) return;
    for (unsigned i=0;i<HF_MASTERY_MAX_LEVELS;i++) free(v->unlocks[i]);
    free(v->levels);free(v->source_levels);free(v);
}
static int hf_mastery_read(HfMasteryRead read,void* context,uintptr_t address,void* out,size_t size) {
    return read && address>=65536u && address<UINT64_C(0x800000000000) &&
        size<UINT64_C(0x800000000000)-address && read(context,address,out,size)==0;
}
/* Revalidation includes every original unlock row; a pointer match alone is
 * insufficient when a progression response replaces data in-place. */
static int hf_mastery_valid(const HfMasteryView* v,uint64_t generation,HfMasteryRead read,void* context) {
    uint8_t package[112],level[HF_MASTERY_LEVEL_BYTES],rows[HF_MASTERY_MAX_UNLOCKS*48];
    if (!v || generation!=v->generation || !hf_mastery_read(read,context,v->source,package,112) ||
        memcmp(package,v->source_package,112)) return 0;
    for (uint32_t i=0;i<v->count;i++) {
        if (!hf_mastery_read(read,context,(uintptr_t)hf_mastery_u64(package+96)+(size_t)i*HF_MASTERY_LEVEL_BYTES,
            level,sizeof(level)) || memcmp(level,v->source_levels+(size_t)i*HF_MASTERY_LEVEL_BYTES,sizeof(level))) return 0;
        const uint8_t* l=level;
        uint32_t n=hf_cat_u32(l+0x840);
        if (n && (!hf_mastery_read(read,context,(uintptr_t)hf_mastery_u64(l+0x838),rows,(size_t)n*48) ||
            memcmp(rows,v->unlocks[i],(size_t)n*48))) return 0;
    }
    return 1;
}
static HfMasteryView* hf_mastery_build(uint32_t weapon,uintptr_t source,uint64_t generation,
    HfMasteryRead read,void* context,const char** reason) {
    unsigned family=weapon==0x6AF4EC48u?0:weapon==0x126DB14Fu?1:2;
    uint32_t expected_id[2]={3479006272u,1414744964u},target=UINT32_MAX,max_sort=0;
    HfMasteryView* v=NULL;
    if (!reason) return NULL;
    *reason="unsupported mastery weapon";
    if (family==2) return NULL;
    v=calloc(1,sizeof(*v));
    if (!v) { *reason="mastery allocation failed";return NULL; }
    v->source=source;v->generation=generation;
    *reason="mastery package unavailable or identity changed";
    if (!hf_mastery_read(read,context,source,v->source_package,112) ||
        hf_cat_u32(v->source_package)!=expected_id[family]) goto fail;
    v->count=hf_cat_u32(v->source_package+104);
    if (!v->count || v->count>HF_MASTERY_MAX_LEVELS) goto fail;
    size_t bytes=(size_t)v->count*HF_MASTERY_LEVEL_BYTES;
    v->source_levels=malloc(bytes);v->levels=malloc(bytes);
    *reason="mastery level allocation or read failed";
    if (!v->source_levels || !v->levels || !hf_mastery_read(read,context,
        (uintptr_t)hf_mastery_u64(v->source_package+96),v->source_levels,bytes)) goto fail;
    memcpy(v->package,v->source_package,112);memcpy(v->levels,v->source_levels,bytes);
    for (uint32_t i=0;i<v->count;i++) {
        uint8_t* l=v->levels+(size_t)i*HF_MASTERY_LEVEL_BYTES;
        if (hf_cat_u32(l)==1u) {
            *reason="ambiguous mastery level one";
            if (target!=UINT32_MAX) goto fail;
            target=i;
        }
        uint32_t n=hf_cat_u32(l+0x840);
        *reason="mastery unlock array is unavailable or too large";
        if (n>HF_MASTERY_MAX_UNLOCKS-2) goto fail;
        v->unlocks[i]=calloc((size_t)n+2,48);
        if (!v->unlocks[i] || (n && !hf_mastery_read(read,context,
            (uintptr_t)hf_mastery_u64(l+0x838),v->unlocks[i],(size_t)n*48))) goto fail;
        for (uint32_t j=0;j<n;j++) {
            uint8_t* u=v->unlocks[i]+(size_t)j*48;
            for (unsigned k=0;k<4;k++) if (hf_cat_u32(u)==hf_cat_child(HF_PRIVATE_CHILDREN[k].id,HF_PRIVATE_CHILDREN[k].weapon)) {
                *reason="private mastery identity already owned";goto fail;
            }
            uint32_t sort=hf_cat_u32(u+8);if (sort>max_sort) max_sort=sort;
        }
        hf_mastery_put64(l+0x838,(uintptr_t)v->unlocks[i]);
    }
    *reason="missing mastery level one or sort order overflow";
    if (target==UINT32_MAX || max_sort>UINT32_MAX-2u) goto fail;
    uint8_t* l=v->levels+(size_t)target*HF_MASTERY_LEVEL_BYTES;
    uint32_t n=hf_cat_u32(l+0x840);
    for (unsigned k=0;k<2;k++) {
        const HfPrivateChild* e=&HF_PRIVATE_CHILDREN[family*2+k];
        uint8_t* u=v->unlocks[target]+(size_t)(n+k)*48;
        hf_cat_put32(u,hf_cat_child(e->id,e->weapon));hf_cat_put32(u+4,1);hf_cat_put32(u+8,max_sort+k+1);
    }
    hf_cat_put32(l+0x840,n+2);hf_mastery_put64(v->package+96,(uintptr_t)v->levels);
    *reason="mastery source changed during cloning";
    if (!hf_mastery_valid(v,generation,read,context)) goto fail;
    *reason="ready";return v;
fail:
    hf_mastery_free(v);return NULL;
}
#endif
