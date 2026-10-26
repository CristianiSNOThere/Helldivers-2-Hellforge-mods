/* Reviewed build-25480438 component shapes. Active tables can be relocated independently. */
#ifndef HF_COMPONENT_LOOKUP_H
#define HF_COMPONENT_LOOKUP_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int (*HfComponentRead)(void*,uintptr_t,void*,size_t);
typedef struct HfComponentLayout { uint32_t group, component, slots, stride; } HfComponentLayout;
static const HfComponentLayout HF_COMPONENT_LAYOUTS[] = {
    {528664u,5u,540u,160u},
    {772928u,60u,12u,104u},
    {5737840u,113u,498u,80u},
    {7825176u,224u,1002u,22096u},
    {5766096u,117u,50u,136u},
    {6151256u,155u,140u,568u},
    {26151656u,236u,730u,1232u},
    {27493728u,266u,58u,592u},
    {46404840u,321u,542u,616u},
};
static const HfComponentLayout* hf_component_layout(uint32_t group) {
    for (size_t i=0;i<sizeof(HF_COMPONENT_LAYOUTS)/sizeof(HF_COMPONENT_LAYOUTS[0]);i++)
        if (HF_COMPONENT_LAYOUTS[i].group==group) return &HF_COMPONENT_LAYOUTS[i];
    return NULL;
}
static int hf_component_pointer(uintptr_t p,size_t size) {
    return p>=65536u && p<UINT64_C(0x800000000000) && size<UINT64_C(0x800000000000)-p;
}
static int hf_component_map(HfComponentRead read,void* context,uintptr_t root,uint32_t group,
                            const uint8_t expected[28],uintptr_t* output) {
    const HfComponentLayout* s=hf_component_layout(group);uintptr_t table=0,again=0;uint8_t header[32];
    if (!s || !read || !output || !hf_component_pointer(root,0xf12478u+8u*s->component+8u) ||
        read(context,root+0xf12478u+8u*s->component,&table,8) || table<32u ||
        !hf_component_pointer(table,(size_t)s->slots*16u) || read(context,table-32u,header,32) ||
        memcmp(header,&s->component,4) || memcmp(header+4,expected,28) ||
        read(context,root+0xf12478u+8u*s->component,&again,8) || again!=table) return 0;
    *output=table;return 1;
}
/* A reviewed stock row must keep its exact owner entry and remain exclusive.
 * Added entities may occupy other hash slots and own appended rows. */
static int hf_component_record(HfComponentRead read,void* context,uintptr_t root,uint32_t group,
    const uint8_t header[28],uint32_t owner_offset,const uint8_t owner[16],uint32_t stride,uintptr_t* output) {
    const HfComponentLayout* s=hf_component_layout(group);uintptr_t table,again;uint32_t index,slot,flags,payload;
    uint64_t identity;uint8_t entries[1002u*16u];unsigned hits=0,aliases=0;
    if (!s || s->stride!=stride || s->slots>1002u || owner_offset<group+28u ||
        (owner_offset-group-28u)%16u || (slot=(owner_offset-group-28u)/16u)>=s->slots ||
        !hf_component_map(read,context,root,group,header,&table) ||
        read(context,table,entries,(size_t)s->slots*16u) || memcmp(entries+slot*16u,owner,16)) return 0;
    memcpy(&identity,owner,8);memcpy(&index,owner+8,4);memcpy(&flags,owner+12,4);
    memcpy(&payload,header+16,4);
    if (!identity || flags || (uint64_t)s->slots*16u+((uint64_t)index+1u)*stride>payload) return 0;
    for (uint32_t i=0;i<s->slots;i++) {
        uint64_t id;uint32_t record;memcpy(&id,entries+i*16u,8);memcpy(&record,entries+i*16u+8u,4);
        if (id==identity) hits++;if (id && record==index) aliases++;
    }
    if (hits!=1u || aliases!=1u || !hf_component_map(read,context,root,group,header,&again) || again!=table) return 0;
    *output=table+(size_t)s->slots*16u+(size_t)index*stride;
    return hf_component_pointer(*output,stride);
}
#endif
