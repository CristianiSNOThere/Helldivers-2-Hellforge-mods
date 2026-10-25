/* Build a temporary parsed catalogue DTO array for the native apply function.
 * Borrowed nested arrays remain owned by the game's original decoder/caller.
 * No stock DTO is changed and no profile or backend file is read or written. */
#ifndef HF_SICKLE_CATALOG_CHILDREN_H
#define HF_SICKLE_CATALOG_CHILDREN_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define HF_CATALOG_DTO_BYTES 112u
typedef struct HfPrivateChild { uint32_t id,weapon,donor_id; } HfPrivateChild;
static const HfPrivateChild HF_PRIVATE_CHILDREN[4]={
    {3949405240u,0x6AF4EC48u,968619308u},{3607579644u,0x6AF4EC48u,1681023692u},
    {2802346321u,0x126DB14Fu,968619308u},{3443815810u,0x126DB14Fu,2276900410u}
};
static uint32_t hf_cat_u32(const uint8_t* p) { uint32_t v;memcpy(&v,p,4);return v; }
static void hf_cat_put32(uint8_t* p,uint32_t v) { memcpy(p,&v,4); }
static uint32_t hf_cat_child(uint32_t id,uint32_t parent) {
    uint32_t k=id*0x5BD1E995u;k^=k>>24;return k*0x5BD1E995u ^ parent*0x5BD1E995u;
}
static uint8_t* hf_cat_build(const uint8_t* source,uint32_t count,uint32_t* output_count,const char** reason)
{
    uint32_t root[2]={UINT32_MAX,UINT32_MAX},end[2]={0},donor[4]={UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
    if (output_count) *output_count=0;
    if (reason) *reason="invalid catalogue arguments or capacity";
    if (!source || !output_count || !reason || !count || count>4092u) return NULL;
    for (uint32_t i=0;i<count;i++) {
        const uint8_t* row=source+(size_t)i*HF_CATALOG_DTO_BYTES;
        uint32_t id=hf_cat_u32(row),parent=hf_cat_u32(row+8),mix=hf_cat_u32(row+24);
        for (unsigned j=0;j<4;j++) {
            const HfPrivateChild* e=&HF_PRIVATE_CHILDREN[j];
            if (id==e->id || mix==hf_cat_child(e->id,e->weapon)) { *reason="private child identity already owned";return NULL; }
            if (id==e->donor_id && parent==0xF47A207Au) {
                if (donor[j]!=UINT32_MAX || mix!=hf_cat_child(id,parent) || hf_cat_u32(row+36)!=2u) {
                    *reason="donor is ambiguous or not an attachment";return NULL;
                }
                donor[j]=i;
            }
        }
        for (unsigned j=0;j<2;j++) if (mix==HF_PRIVATE_CHILDREN[j*2].weapon && !parent) {
            if (root[j]!=UINT32_MAX || id!=mix) { *reason="weapon root is ambiguous";return NULL; }
            root[j]=i;
        }
    }
    for (unsigned j=0;j<4;j++) if (donor[j]==UINT32_MAX) { *reason="SAI donor attachment unavailable";return NULL; }
    for (unsigned j=0;j<2;j++) {
        if (root[j]==UINT32_MAX) { *reason="Sickle weapon root unavailable";return NULL; }
        end[j]=root[j];
        while (end[j]+1<count && hf_cat_u32(source+(size_t)(end[j]+1)*112+8)==HF_PRIVATE_CHILDREN[j*2].weapon) end[j]++;
        for (uint32_t i=0;i<count;i++) if (hf_cat_u32(source+(size_t)i*112+8)==HF_PRIVATE_CHILDREN[j*2].weapon &&
            (i<=root[j] || i>end[j])) { *reason="weapon children are outside the native parent run";return NULL; }
    }
    uint8_t* output=malloc((size_t)(count+4)*112);
    if (!output) { *reason="catalogue allocation failed";return NULL; }
    uint32_t next=0;
    for (uint32_t i=0;i<count;i++) {
        memcpy(output+(size_t)next++*112,source+(size_t)i*112,112);
        for (unsigned j=0;j<2;j++) if (i==end[j]) for (unsigned k=0;k<2;k++) {
            unsigned n=j*2+k;
            const HfPrivateChild* e=&HF_PRIVATE_CHILDREN[n];
            uint8_t* row=output+(size_t)next++*112;
            memcpy(row,source+(size_t)donor[n]*112,112);
            hf_cat_put32(row,e->id);hf_cat_put32(row+8,e->weapon);hf_cat_put32(row+24,hf_cat_child(e->id,e->weapon));
            row[32]=0;row[34]=0;row[35]=0; /* non-consumable, no account-level requirement */
            /* Counts are u32; sellCount+0x60 is immediately followed by the
             * optional maxCap value+0x64. Do not clear a fictitious u64 count. */
            memset(row+56,0,12);memset(row+72,0,12);memset(row+88,0,12);
        }
    }
    if (next!=count+4) { free(output);*reason="catalogue construction mismatch";return NULL; }
    *output_count=next;*reason="ready";return output;
}
#endif
