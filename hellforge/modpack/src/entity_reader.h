/* Safe, bounded reads of the source-pinned entity/wielder layouts. Mirrors the
 * installed SDK's generation and entity-resource conventions, with explicit
 * root revalidation around the join. No inventory selection or heat value is
 * used as a proxy for an attachment. */
#ifndef HF_SICKLE_ENTITY_READER_H
#define HF_SICKLE_ENTITY_READER_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int (*HfEntityRead)(void*,uintptr_t,void*,size_t); /* 0 succeeds */
typedef struct HfHeldIdentity { uint32_t avatar,entity;uint64_t resource;uintptr_t descriptor; } HfHeldIdentity;
static int hf_entity_read(HfEntityRead read,void* ctx,uintptr_t at,void* out,size_t size) {
    return read && at>=65536 && at<UINT64_C(0x800000000000) && size<UINT64_C(0x800000000000)-at && !read(ctx,at,out,size);
}
static int hf_entity_ptr(HfEntityRead read,void* ctx,uintptr_t at,uintptr_t* out) {
    uint64_t p;if (!hf_entity_read(read,ctx,at,&p,8) || p<65536 || p>=UINT64_C(0x800000000000)) return 0;
    *out=(uintptr_t)p;return 1;
}
static int hf_entity_hash(HfEntityRead read,void* ctx,uintptr_t header,uint32_t key,uint32_t* out) {
    uint8_t raw[20];uint64_t buckets;uint32_t capacity,empty,multiplier;
    if (!key || !hf_entity_read(read,ctx,header,raw,sizeof(raw))) return 0;
    memcpy(&buckets,raw,8);memcpy(&capacity,raw+8,4);memcpy(&empty,raw+12,4);memcpy(&multiplier,raw+16,4);
    if (buckets<65536 || buckets>=UINT64_C(0x800000000000) || !capacity || (capacity&(capacity-1)) || capacity>1048576 || key==empty) return 0;
    uint32_t start=key*multiplier,limit=capacity<4096?capacity:4096;
    for (uint32_t i=0;i<limit;i++) {
        uint32_t pair[2];uintptr_t at=(uintptr_t)buckets+(size_t)((start+i)%capacity)*8;
        if (!hf_entity_read(read,ctx,at,pair,sizeof(pair))) return 0;
        if (pair[0]==key) { if (pair[1]==UINT32_MAX) return 0;*out=pair[1];return 1; }
        if (pair[0]==empty) return 0;
    }
    return 0;
}
static int hf_entity_alive(HfEntityRead read,void* ctx,uintptr_t exe,uint32_t entity) {
    uintptr_t manager,gens;uint32_t count,index=entity&0x3fffffu;uint8_t generation;
    return entity && hf_entity_ptr(read,ctx,exe+0x1b135b8,&manager) &&
        hf_entity_read(read,ctx,manager+0x80,&count,4) && index<count && count<=0x400000 &&
        hf_entity_ptr(read,ctx,manager+0x88,&gens) && hf_entity_read(read,ctx,gens+index,&generation,1) &&
        generation==((entity>>22)&255);
}
static int hf_entity_held(HfEntityRead read,void* ctx,uintptr_t game,uintptr_t exe,uint32_t avatar,HfHeldIdentity* out) {
    uintptr_t wielder,slots,em;uint32_t instance,entity,index;uint8_t descriptor[24];
    if (!out) return 0;
    memset(out,0,sizeof(*out));
    if (!avatar || !hf_entity_alive(read,ctx,exe,avatar) ||
        !hf_entity_ptr(read,ctx,game+0x3326420,&wielder) || !hf_entity_hash(read,ctx,wielder+0x30,avatar,&instance) ||
        instance>=65536 || !hf_entity_ptr(read,ctx,wielder+0x60,&slots) ||
        !hf_entity_read(read,ctx,slots+(size_t)instance*0x1d0,&entity,4) || entity==avatar ||
        !hf_entity_alive(read,ctx,exe,entity) || !hf_entity_ptr(read,ctx,game+0x346bf98,&em) ||
        !hf_entity_hash(read,ctx,em+0xf1aeb0,entity,&index) || index>=0x100000 ||
        !hf_entity_read(read,ctx,em+0xf32f18+(size_t)index*24,descriptor,24)) return 0;
    uint32_t named;uint64_t resource;memcpy(&named,descriptor+8,4);memcpy(&resource,descriptor,8);
    if (named!=entity || !resource) return 0;
    uintptr_t current_wielder,current_em,current_slots;uint32_t current_entity,current_instance,current_index;uint8_t current_descriptor[24];
    if (!hf_entity_ptr(read,ctx,game+0x3326420,&current_wielder) || current_wielder!=wielder ||
        !hf_entity_ptr(read,ctx,game+0x346bf98,&current_em) || current_em!=em ||
        !hf_entity_ptr(read,ctx,wielder+0x60,&current_slots) || current_slots!=slots ||
        !hf_entity_read(read,ctx,slots+(size_t)instance*0x1d0,&current_entity,4) || current_entity!=entity ||
        !hf_entity_hash(read,ctx,wielder+0x30,avatar,&current_instance) || current_instance!=instance ||
        !hf_entity_hash(read,ctx,em+0xf1aeb0,entity,&current_index) || current_index!=index ||
        !hf_entity_read(read,ctx,em+0xf32f18+(size_t)index*24,current_descriptor,24) || memcmp(descriptor,current_descriptor,24) ||
        !hf_entity_alive(read,ctx,exe,avatar) || !hf_entity_alive(read,ctx,exe,entity)) return 0;
    out->avatar=avatar;out->entity=entity;out->resource=resource;out->descriptor=em+0xf32f18+(size_t)index*24;return 1;
}
#endif
