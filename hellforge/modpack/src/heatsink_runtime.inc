/* Installed-SDK11 adapter, included after the accepted Sickle port. Private
 * integration work: do not enable source slots until every hook/data gate passes. */
#include "descriptor_overlay.h"
#include "mastery_overlay.h"
#include "catalog_ownership.h"
#include "entity_reader.h"
#include "heat_instance.h"
#include "spawn_selection.h"
#include "data_transaction.h"
#include "heatsink_guards.generated.h"

typedef uintptr_t (*HsIdFn)(uint32_t);
typedef uintptr_t (*HsHashFn)(uint64_t);
typedef uint64_t (*HsCatalogFn)(const uint8_t*,uint32_t,uintptr_t);
typedef uint8_t (*HsListFn)(uint64_t,uint8_t**,uint32_t*,uint32_t,uint32_t,uint32_t,uint8_t);
typedef uint64_t (*HsDispatchFn)(uintptr_t,uintptr_t,uint64_t,const uint8_t*);
typedef uint64_t (*HsHitFn)(uintptr_t,uint8_t*,uint32_t,uint32_t,const uint32_t*,const uint8_t*,const uint8_t*);
typedef uint8_t (*HsSingleFn)(uint64_t,uint64_t,uint8_t*);
typedef uint8_t (*HsMultiFn)(const uint8_t*,uint64_t,uint8_t*);
typedef void (*HsCreateHeatFn)(uintptr_t,uintptr_t,const uint32_t*);
typedef const char* (*HsTextFn)(uint32_t,void*,void*,void*);
typedef uintptr_t (*HsSpawnFn)(uint64_t,uint32_t,uint64_t,const uint64_t*,uint32_t);
typedef struct HsRuntime {
    const HfApi* api;HfMod* mod;uintptr_t game,exe;size_t game_size;
    volatile LONG hooks_ready,active,failed,logged_refusal,pending_cleanup;
    SRWLOCK lock;
    uint64_t epoch,catalog_generation;
    uintptr_t entity_root,catalog_root,custom_address[2],heat_source[2];
    uintptr_t damage_rows[3];
    uint32_t catalog_count,children[4],parents[2];
    int catalog_ready,descriptors_ready,text_ready,slots_live,granted[4];
    uint8_t heat[2][592],custom_before[2][4872],custom_after[2][4872];
    HfHeatsinkDescriptors descriptors;HfMasteryView* mastery[2];
    HfHeatBinding bindings[256];
    HfSickleCrypto crypto;
    HsCatalogFn catalog;HsIdFn by_id,catalog_id,mastery_fn;HsHashFn by_delta,delta;
    HsListFn list;HsDispatchFn dispatch;HsHitFn hit;HsSingleFn single;HsMultiFn multi;
    HsHashFn base_heat;HsHashFn instance_heat;HsCreateHeatFn create_heat;
    HsTextFn text;uintptr_t text_slot;
    HsSpawnFn spawn;
    uint64_t start_ms,next_tick;const char* waiting;
} HsRuntime;
static HsRuntime HS;
static void hs_disable_locked(const char* reason);
static int hs_descriptors(void);
static const uint64_t HS_ENTITIES[2]={HF_SICKLE_ENTITY,HF_DOUBLE_EDGE_ENTITY};
static const uint32_t HS_EMPTY_DELTA[2]={0,0},HS_EMPTY_HEAT_COMPONENT[3]={266,0,0};
static int hs_read(void* context,uintptr_t address,void* output,size_t size) {
    HsRuntime* s=context;return s->api->read(address,output,size);
}
static int hs_copy(uintptr_t address,void* output,size_t size) {
    return hf_entity_read(hs_read,&HS,address,output,size);
}
static uintptr_t hs_pointer(uintptr_t address) {
    uintptr_t value=0;return hf_entity_ptr(hs_read,&HS,address,&value)?value:0;
}
static uint32_t hs_word(uintptr_t address,uint32_t fallback) {
    uint32_t value;return hs_copy(address,&value,4)?value:fallback;
}
static int hs_family(uint64_t resource) { return resource==HS_ENTITIES[0]?0:resource==HS_ENTITIES[1]?1:-1; }
static int hs_choice(uint64_t hash) { for (unsigned i=0;i<4;i++) if (hash==HF_HEATSINK_KEYS[i]) return (int)i;return -1; }
static void hs_refusal(const char* reason) {
    if (!InterlockedCompareExchange(&HS.logged_refusal,1,0)) HS.api->log(HS.mod,"Sickle heatsinks refused: %s",reason);
}
static int hs_equal(uintptr_t address,const void* expected,size_t size) {
    uint8_t bytes[4872];return size<=sizeof(bytes) && hs_copy(address,bytes,size) && !memcmp(bytes,expected,size);
}
/* Game-thread writes only, bounded to previously validated data rows. A partial
 * API write is repaired only when every byte is still ours or its old value. */
static int hs_write(void* context,uintptr_t address,const void* bytes,size_t size) {
    HsRuntime* s=context;return s->api->write(address,bytes,size);
}
static int hs_write_if(uintptr_t address,const uint8_t* before,const uint8_t* after,size_t size) {
    int result=hf_row_write_if(hs_read,hs_write,&HS,address,before,after,size);
    if (result<0) { InterlockedExchange(&HS.active,0);HS.failed=1;HS.pending_cleanup=1;hs_refusal("owned data write could not be completely repaired"); }
    return result==1;
}
static void hs_undo(uintptr_t address,const uint8_t* before,const uint8_t* after,size_t size) {
    if (!hf_row_undo_owned(hs_read,hs_write,&HS,address,before,after,size)) {
        HS.failed=1;InterlockedExchange(&HS.active,0);hs_refusal("owned data rollback could not be completely repaired");
    }
}
static int hs_hash(const void* bytes,size_t size,const char* expected) {
    char actual[65];return sickle_hash_bytes(&HS.crypto,bytes,size,actual) && !strcmp(actual,expected);
}
static HfHeatBinding* hs_binding(uint32_t entity,int create) {
    HfHeatBinding* empty=NULL;
    for (unsigned i=0;i<256;i++) {
        HfHeatBinding* b=&HS.bindings[i];
        if (b->entity==entity && b->epoch==HS.epoch) return b;
        if (!b->choice || b->epoch!=HS.epoch) empty=b;
    }
    return create?empty:NULL;
}
/* The reverse map legitimately uses key0; the generic entity helper rejects it.
 * Validate that map with the same bounded probing while allowing zero. */
static int hs_override_row(uintptr_t descriptor,uintptr_t* row,uintptr_t* manager) {
    uint8_t d[24],header[20];uint32_t index,inverse=UINT32_MAX;
    uintptr_t m=hs_pointer(HS.game+0x3326d48),rows;
    if (!m || !hs_copy(descriptor,d,24) || !hf_entity_hash(hs_read,&HS,m+0x68,hf_cat_u32(d+8),&index) ||
        index>=hs_word(m+0xa0,0) || index>=65536 || !hs_copy(m+0x88,header,20) || !(rows=hs_pointer(m+0xa8))) return 0;
    uint64_t buckets=hf_mastery_u64(header);uint32_t cap=hf_cat_u32(header+8),empty=hf_cat_u32(header+12),mult=hf_cat_u32(header+16);
    if (!cap || (cap&(cap-1)) || cap>1048576 || index==empty) return 0;
    for (uint32_t i=0;i<cap && i<4096;i++) {
        uint32_t pair[2];if (!hs_copy((uintptr_t)buckets+(size_t)((index*mult+i)%cap)*8,pair,8)) return 0;
        if (pair[0]==index) { inverse=pair[1];break; } if (pair[0]==empty) break;
    }
    if (inverse!=hf_cat_u32(d+8) || hs_pointer(HS.game+0x3326d48)!=m) return 0;
    *row=rows+(size_t)index*592;*manager=m;return 1;
}
static void hs_disable_locked(const char* reason) {
    InterlockedExchange(&HS.active,0);
    uintptr_t root=hs_pointer(HS.game+0x346bf98);
    if (root && root==HS.entity_root) {
        for (unsigned i=0;i<256;i++) {
            HfHeatBinding* binding=&HS.bindings[i];uint32_t index;uintptr_t row,manager;uint8_t descriptor[24];
            int family=hs_family(binding->resource);
            if (!binding->choice || binding->epoch!=HS.epoch || family<0 ||
                !hf_entity_hash(hs_read,&HS,root+0xf1aeb0,binding->entity,&index) || index>=0x100000) continue;
            uintptr_t address=root+0xf32f18+(size_t)index*24;
            if (hs_copy(address,descriptor,24) && hf_cat_u32(descriptor+8)==binding->entity &&
                hf_mastery_u64(descriptor)==binding->resource && hs_override_row(address,&row,&manager) &&
                manager==binding->manager && hs_equal(row,binding->profile,592)) hs_undo(row,HS.heat[family],binding->profile,592);
        }
        if (HS.slots_live) for (unsigned i=0;i<2;i++) {
            uint32_t stride=0;uintptr_t address=HS.api->component_record("WeaponCustomizationComponentData",HS_ENTITIES[i],&stride);
            if (stride==4872 && address==HS.custom_address[i]) hs_undo(address,HS.custom_before[i],HS.custom_after[i],4872);
        }
    }
    HS.slots_live=0;HS.epoch++;memset(HS.bindings,0,sizeof(HS.bindings));
    if (reason) hs_refusal(reason);
}
static int hs_parent_owned(unsigned family) {
    uint8_t row[184];uint32_t index=HS.parents[family],id=HF_PRIVATE_CHILDREN[family*2].weapon;
    if (!HS.catalog_ready || index>=HS.catalog_count || hs_pointer(HS.game+0x347cef8)!=HS.catalog_root ||
        hs_word(HS.catalog_root+0x1ce0,0)!=HS.catalog_count ||
        !hs_copy(HS.catalog_root+0x1ce4+(size_t)index*184,row,184) || hf_cat_u32(row)!=index ||
        hf_cat_u32(row+4)!=id || hf_cat_u32(row+8)!=id || hf_cat_u32(row+12)!=1) return 0;
    uint32_t state=hf_cat_u32(row+20);return state==2 || state==4;
}
/* Called while holding the exclusive adapter lock, before native armory reads. */
static int hs_ownership(void) {
    uint8_t before[4][184],after[4][184];uintptr_t addresses[4];int changed[4]={0};
    if (!HS.catalog_ready || hs_pointer(HS.game+0x347cef8)!=HS.catalog_root ||
        hs_word(HS.catalog_root+0x1ce0,0)!=HS.catalog_count) return 0;
    for (unsigned i=0;i<4;i++) {
        if (HS.children[i]>=HS.catalog_count) return 0;
        addresses[i]=HS.catalog_root+0x1ce4+(size_t)HS.children[i]*184;
        if (!hs_copy(addresses[i],before[i],184)) return 0;
        changed[i]=hf_child_ownership(i,HS.children[i],before[i],hs_parent_owned(i/2),HS.granted[i],after[i]);
        if (changed[i]<0) return 0;
    }
    for (unsigned i=0;i<4;i++) if (changed[i]) {
        if (!hs_write_if(addresses[i],before[i],after[i],184)) {
            for (unsigned j=0;j<i;j++) if (changed[j]) hs_undo(addresses[j],before[j],after[j],184);
            return 0;
        }
    }
    for (unsigned i=0;i<4;i++) HS.granted[i]=hf_cat_u32(before[i]+20)<=1 && hf_cat_u32(after[i]+20)==2 ? (int)hf_cat_u32(before[i]+20)+1 :
        (hf_cat_u32(after[i]+20)==2 && HS.granted[i]);
    return 1;
}
static uint64_t hs_catalog(const uint8_t* dtos,uint32_t count,uintptr_t context) {
    const char* reason=NULL;uint32_t n=0;uint8_t* source=NULL;uint8_t* augmented=NULL;
    if (count && count<=4092) {
        source=malloc((size_t)count*112);
        if (source && hs_copy((uintptr_t)dtos,source,(size_t)count*112)) augmented=hf_cat_build(source,count,&n,&reason);
    }
    InterlockedExchange(&HS.active,0);
    AcquireSRWLockExclusive(&HS.lock);hs_disable_locked(NULL);HS.catalog_ready=0;HS.catalog_generation++;HS.epoch++;
    memset(HS.bindings,0,sizeof(HS.bindings));memset(HS.granted,0,sizeof(HS.granted));ReleaseSRWLockExclusive(&HS.lock);
    /* The original catalogue finalizer calls a second ID getter. Descriptors
     * must exist before that call or it changes private category 2 into 0. */
    if (augmented && !hs_descriptors()) { free(augmented);augmented=NULL;reason="catalogue descriptors were not ready before native finalization"; }
    if (!augmented) { free(source);hs_refusal(reason?reason:"catalogue DTO copy unavailable");return HS.catalog(dtos,count,context); }
    uint64_t result=HS.catalog(augmented,n,context);
    AcquireSRWLockExclusive(&HS.lock);
    HS.catalog_root=hs_pointer(HS.game+0x347cef8);HS.catalog_count=n;
    for (unsigned i=0;i<4;i++) HS.children[i]=UINT32_MAX;
    for (unsigned i=0;i<2;i++) HS.parents[i]=UINT32_MAX;
    for (uint32_t i=0;i<n;i++) {
        uint8_t* r=augmented+(size_t)i*112;uint32_t id=hf_cat_u32(r),parent=hf_cat_u32(r+8);
        for (unsigned j=0;j<4;j++) if (id==HF_PRIVATE_CHILDREN[j].id && parent==HF_PRIVATE_CHILDREN[j].weapon) HS.children[j]=i;
        for (unsigned j=0;j<2;j++) if (id==HF_PRIVATE_CHILDREN[j*2].weapon && !parent) HS.parents[j]=i;
    }
    HS.catalog_ready=HS.catalog_root && hs_word(HS.catalog_root+0x1ce0,0)==n;
    for (unsigned i=0;i<4;i++) HS.catalog_ready &= HS.children[i]<n;
    for (unsigned i=0;i<2;i++) HS.catalog_ready &= HS.parents[i]<n;
    HS.descriptors.generation=HS.catalog_generation;
    ReleaseSRWLockExclusive(&HS.lock);free(augmented);free(source);
    HS.api->log(HS.mod,"Sickle heatsinks: native catalogue received four private children (%u total)",n);
    return result;
}
static uintptr_t hs_by_id(uint32_t id) {
    uintptr_t original=HS.by_id(id);
    if (HS.descriptors_ready && HS.hooks_ready) for (unsigned i=0;i<4;i++) if (id==HF_PRIVATE_CHILDREN[i].id) {
        if (original) { InterlockedExchange(&HS.active,0);HS.pending_cleanup=1;HS.failed=1;hs_refusal("private descriptor ID acquired by another provider");return original; }
        return (uintptr_t)HS.descriptors.rows[i];
    }
    return original;
}
static uintptr_t hs_catalog_id(uint32_t id) {
    uintptr_t original=HS.catalog_id(id);
    if (HS.descriptors_ready && HS.hooks_ready) for (unsigned i=0;i<4;i++) if (id==HF_PRIVATE_CHILDREN[i].id) {
        if (original) { InterlockedExchange(&HS.active,0);HS.pending_cleanup=1;HS.failed=1;hs_refusal("private catalog descriptor ID acquired by another provider");return original; }
        return (uintptr_t)HS.descriptors.rows[i];
    }
    return original;
}
static uintptr_t hs_by_delta(uint64_t hash) {
    uintptr_t original=HS.by_delta(hash);int choice=hs_choice(hash);
    if (choice>=0 && HS.descriptors_ready && HS.hooks_ready) {
        if (original) { InterlockedExchange(&HS.active,0);HS.pending_cleanup=1;HS.failed=1;hs_refusal("private descriptor delta acquired by another provider");return original; }
        return (uintptr_t)HS.descriptors.rows[choice];
    }
    return original;
}
static uintptr_t hs_delta(uint64_t hash) {
    uintptr_t original=HS.delta(hash);
    if (hs_choice(hash)>=0 && HS.hooks_ready) {
        if (original) { InterlockedExchange(&HS.active,0);HS.pending_cleanup=1;HS.failed=1;hs_refusal("private entity delta acquired by another provider");return original; }
        return (uintptr_t)HS_EMPTY_DELTA;
    }
    return original;
}
static uint8_t hs_list(uint64_t context,uint8_t** output,uint32_t* count,uint32_t weapon,uint32_t type,uint32_t mask,uint8_t locked) {
    uint32_t capacity=0;uintptr_t original_buffer=0;
    if (count) (void)hs_copy((uintptr_t)count,&capacity,4);
    if (output) (void)hs_copy((uintptr_t)output,&original_buffer,8);
    if (!original_buffer) {
        capacity=0;uint32_t groups=hs_word(HS.game+0x348da98,UINT32_MAX);
        if (groups<=64) for (uint32_t i=0;i<groups;i++) {
            uintptr_t group=hs_pointer(HS.game+0x37c5b50+(size_t)i*8);
            uint32_t n=group?hs_word(group+8,UINT32_MAX):UINT32_MAX;
            if (n>4096 || capacity>4096-n) { capacity=0;break; } capacity+=n;
        }
    }
    if (HS.active) { AcquireSRWLockExclusive(&HS.lock);if (!hs_ownership()) hs_disable_locked("catalogue ownership guard changed");ReleaseSRWLockExclusive(&HS.lock); }
    uint8_t result=HS.list(context,output,count,weapon,type,mask,locked);
    AcquireSRWLockShared(&HS.lock);
    if (HS.active && output && count && capacity<=4096 && *output && *count<=capacity) {
        int rc=hf_desc_append(&HS.descriptors,HS.catalog_generation,weapon,mask,locked,
            hs_parent_owned(weapon==HF_PRIVATE_CHILDREN[0].weapon?0:1),*output,capacity,count);
        if (rc>0) result=1;else if (rc<0) hs_refusal("attachment list capacity or identity changed");
    }
    ReleaseSRWLockShared(&HS.lock);return result;
}
static uintptr_t hs_mastery(uint32_t weapon) {
    uintptr_t source=HS.mastery_fn(weapon);unsigned family=weapon==HF_PRIVATE_CHILDREN[0].weapon?0:weapon==HF_PRIVATE_CHILDREN[2].weapon?1:2;
    if (family==2 || !HS.active || !source) return source;
    AcquireSRWLockExclusive(&HS.lock);
    HfMasteryView* view=HS.mastery[family];
    if (!view || view->source!=source || !hf_mastery_valid(view,HS.catalog_generation,hs_read,&HS)) {
        const char* reason=NULL;view=hf_mastery_build(weapon,source,HS.catalog_generation,hs_read,&HS,&reason);
        if (view) HS.mastery[family]=view;else hs_disable_locked(reason?reason:"mastery clone unavailable");
        /* Retain replaced views for session lifetime; native UI may still hold one. */
    }
    uintptr_t result=view?(uintptr_t)view->package:source;
    ReleaseSRWLockExclusive(&HS.lock);return result;
}
static int hs_deltas(uintptr_t list,uint64_t keys[10],uint32_t* count) {
    uint32_t n;if (!hs_copy(list,&n,4) || n>10 || (n && !hs_copy(list+8,keys,(size_t)n*8))) return 0;
    *count=n;return 1;
}
static uintptr_t hs_spawn(uint64_t context,uint32_t avatar,uint64_t resource,const uint64_t* overrides,uint32_t mask) {
    uint64_t selected[10];uint32_t selected_mask=0;int choice=0;
    if (HS.active && TryAcquireSRWLockShared(&HS.lock)) {
        if (HS.active && HS.catalog_ready && hs_pointer(HS.game+0x347cef8)==HS.catalog_root &&
            hs_parent_owned(resource==HF_SICKLE_ENTITY?0:1)) {
            choice=hf_spawn_selection(hs_read,&HS,HS.game,HS.exe,avatar,resource,(uintptr_t)overrides,mask,selected,&selected_mask);
            if (choice>0) {
                uint8_t row[184],desired[184];unsigned i=(unsigned)choice-1;
                if (!hs_copy(HS.catalog_root+0x1ce4+(size_t)HS.children[i]*184,row,184) ||
                    hf_child_ownership(i,HS.children[i],row,1,HS.granted[i],desired)!=0) choice=-1;
            }
        }
        ReleaseSRWLockShared(&HS.lock);
    }
    /* Stack argument five and the ten-key array live through the synchronous
     * original call; the native builder copies chosen keys into its own result. */
    if (choice>0) {
        uintptr_t result=HS.spawn(context,avatar,resource,selected,selected_mask);
        HS.api->log(HS.mod,"Sickle heatsink spawn: avatar %08x choice %d retained through native builder",avatar,choice);
        return result;
    }
    return HS.spawn(context,avatar,resource,overrides,mask);
}
static uint64_t hs_dispatch(uintptr_t context,uintptr_t descriptor,uint64_t arg3,const uint8_t* deltas) {
    uint64_t result=HS.dispatch(context,descriptor,arg3,deltas);
    if (!HS.active || !TryAcquireSRWLockExclusive(&HS.lock)) return result;
    uint8_t d[24],actual[592],desired[592];uint64_t keys[10];uint32_t n;uintptr_t row=0,manager=0;
    if (!HS.active || context!=HS.entity_root+0x40 || hs_pointer(HS.game+0x346bfc8)!=context || hs_pointer(HS.game+0x346bf98)!=HS.entity_root ||
        hs_pointer(HS.game+0x3326d48)!=context+0xeb6570 || !hs_copy(descriptor,d,24) ||
        !hs_deltas((uintptr_t)deltas,keys,&n)) goto done;
    uint64_t resource=hf_mastery_u64(d);uint32_t entity=hf_cat_u32(d+8);int family=hs_family(resource);
    if (family<0 || !entity || !hs_equal(HS.heat_source[family],HS.heat[family],592)) goto done;
    HfHeatBinding* previous=hs_binding(entity,0);HfHeatBinding next;
    if (!hs_override_row(descriptor,&row,&manager)) { row=HS.heat_source[family];manager=context+0xeb6570; }
    if (!hs_copy(row,actual,592)) goto done;
    int rc=hf_heat_prepare(previous,HS.epoch,manager,entity,resource,keys,n,HS.heat[family],actual,desired,&next);
    if (!rc) goto done;
    if (rc<0) { hs_refusal("instance heat or attachment provenance changed");goto done; }
    HfHeatBinding* destination=previous?previous:hs_binding(entity,1);
    if (!destination) { hs_refusal("private heat binding capacity reached");goto done; }
    /* Native maps must already be allocated; never call the game's unchecked
     * insertion with a missing/zero-capacity map. */
    if (!hs_word(manager+0x70,0) || !hs_word(manager+0x90,0)) goto done;
    HS.create_heat(manager,descriptor,HS_EMPTY_HEAT_COMPONENT);
    uintptr_t resolved_manager;
    if (!hs_override_row(descriptor,&row,&resolved_manager) || resolved_manager!=manager ||
        row==HS.heat_source[family] || !hs_equal(row,actual,592) || !hs_write_if(row,actual,desired,592)) {
        hs_refusal("private native heat copy could not be committed");goto done;
    }
    if (hs_pointer(HS.game+0x3326d48)!=manager || hs_pointer(HS.game+0x346bf98)!=HS.entity_root || hs_pointer(HS.game+0x346bfc8)!=context) {
        (void)hs_write_if(row,desired,actual,592);goto done;
    }
    int changed=!previous || previous->choice!=next.choice;
    *destination=next;
    if (changed) HS.api->log(HS.mod,"Sickle heatsink instance: entity %08x choice %u",entity,next.choice);
done:
    ReleaseSRWLockExclusive(&HS.lock);return result;
}
static int hs_damage_registry(int capture) {
    uintptr_t root=0;
    if (!hf_registry_root(HS.api,HS.game,HS.game_size,HF_REGISTRY_DAMAGE,&root)) return 0;
    for (unsigned i=0;i<HF_SICKLE_REGISTRY_COUNT;i++) {
        const HfSickleRegistrySpec* spec=&HF_SICKLE_REGISTRIES[i];
        if (spec->id<542 || spec->id>544 || spec->size!=76) continue;
        uintptr_t row=hs_pointer(root+(size_t)spec->id*8);uint8_t expected[272];
        memcpy(expected,spec->original,spec->size);
        memcpy(expected+spec->offset,spec->patch,spec->patch_size);
        if (!row || !hs_equal(row,expected,spec->size) ||
            (!capture && row!=HS.damage_rows[spec->id-542])) return 0;
        if (capture) HS.damage_rows[spec->id-542]=row;
    }
    return 1;
}
static uint64_t hs_hit(uintptr_t context,uint8_t* output,uint32_t kind,uint32_t element,const uint32_t* target,
    const uint8_t* source,const uint8_t* vector) {
    uint32_t profile=hs_word((uintptr_t)source+0x24,UINT32_MAX),victim=hs_word((uintptr_t)target,0);
    uint64_t result=HS.hit(context,output,kind,element,target,source,vector);
    if (!HS.active || profile<542 || profile>544 || !victim || !TryAcquireSRWLockShared(&HS.lock)) return result;
    HfHeldIdentity held;uintptr_t row,manager;uint8_t heat[592],hit[0x9c];
    if (HS.active && hs_damage_registry(0) && hf_entity_held(hs_read,&HS,HS.game,HS.exe,victim,&held) &&
        held.resource==HF_DOUBLE_EDGE_ENTITY && hs_override_row(held.descriptor,&row,&manager) &&
        hs_copy(row,heat,592) && hs_copy((uintptr_t)output,hit,sizeof(hit)) && hf_cat_u32(hit+0x80)==victim &&
        hs_equal(HS.heat_source[1],HS.heat[1],592)) {
        HfHeatBinding* binding=hs_binding(held.entity,0);uint8_t old_damage[4];memcpy(old_damage,hit+0x98,4);
        if (hf_heat_damage(binding,HS.epoch,manager,victim,held.entity,held.resource,1,heat,profile,hit,sizeof(hit)))
            (void)hs_write_if((uintptr_t)output+0x98,old_damage,hit+0x98,4);
    }
    ReleaseSRWLockShared(&HS.lock);return result;
}
static void hs_preview(uint64_t resource,const uint64_t* keys,uint32_t n,uint8_t* output) {
    int family=hs_family(resource);uint8_t before[592],after[592];
    if (family<0 || !HS.active || !TryAcquireSRWLockShared(&HS.lock)) return;
    if (HS.active && hs_equal(HS.heat_source[family],HS.heat[family],592) && hs_copy((uintptr_t)output,before,592)) {
        memcpy(after,before,592);
        if (hf_heat_overlay(resource,keys,n,HS.heat[family],after)>0) (void)hs_write_if((uintptr_t)output,before,after,592);
    }
    ReleaseSRWLockShared(&HS.lock);
}
static uint8_t hs_single(uint64_t resource,uint64_t key,uint8_t* output) {
    uint8_t result=HS.single(resource,key,output);if (result) hs_preview(resource,&key,1,output);return result;
}
static uint8_t hs_multi(const uint8_t* deltas,uint64_t resource,uint8_t* output) {
    uint8_t result=HS.multi(deltas,resource,output);uint64_t keys[10];uint32_t n;
    if (result && hs_deltas((uintptr_t)deltas,keys,&n)) hs_preview(resource,keys,n,output);return result;
}
static const char* hs_text(uint32_t id,void* a,void* b,void* c) {
    static const char* const upper[4]={"STANDARD HEATSINK","RAPID COOLING HEATSINK","STANDARD HEATSINK","RAPID HEAT HEATSINK"};
    static const char* const title[4]={"Standard Heatsink","Rapid Cooling Heatsink","Standard Heatsink","Rapid Heat Heatsink"};
    static const char* const description[4]={"Standard heat capacity and cooling.",
        "Cools at 12 heat units per second.","Standard heat capacity and heat damage.",
        "150 heat capacity. Heat stages are reached 25% sooner. Direct heat damage is 50% of the original weapon."};
    for (unsigned i=0;i<4;i++) for (unsigned j=0;j<3;j++) if (id==HF_HEATSINK_TEXT_IDS[i][j])
        return j==0?upper[i]:j==1?title[i]:description[i];
    return HS.text?HS.text(id,a,b,c):"";
}
static int hs_localization(void) {
    uintptr_t api=hs_pointer(HS.game+0x3326308),table=api?hs_pointer(api+0x10):0,current=table?hs_pointer(table+1000):0;
    if (!current) return 0;
    if (HS.text_ready) return HS.text_slot==table+1000 && current==(uintptr_t)hs_text;
    size_t loader_size=0,exe_size=0;uintptr_t loader=HS.api->module_base("hellforge_modloader.dll",&loader_size);
    uintptr_t exe=HS.api->module_base("helldivers2.exe",&exe_size);
    /* Wait until HellForge owns the text slot when it has registered weapon
     * text; installing ahead of its one-time hook would make it refuse theirs. */
    int loader_owned=loader && current>=loader && current<loader+loader_size;
    int engine_owned=exe && current>=exe && current<exe+exe_size;
    if (!loader_owned && !(engine_owned && GetTickCount64()-HS.start_ms>=30000)) return 0;
    HsTextFn previous=(HsTextFn)current;
    for (unsigned i=0;i<4;i++) for (unsigned j=0;j<3;j++) {
        const char* existing=previous(HF_HEATSINK_TEXT_IDS[i][j],NULL,NULL,NULL);char first=0;
        if (existing && (!hs_copy((uintptr_t)existing,&first,1) || first)) {
            hs_refusal("a private localization key has an existing or unrecognized value");return 0;
        }
    }
    DWORD protection,ignored;
    if (!VirtualProtect((void*)(table+1000),8,PAGE_READWRITE,&protection)) return 0;
    HS.text=previous;
    void* actual=InterlockedCompareExchangePointer((void* volatile*)(table+1000),(void*)hs_text,(void*)current);
    (void)VirtualProtect((void*)(table+1000),8,protection,&ignored);
    if ((uintptr_t)actual!=current) { HS.text=NULL;return 0; }
    HS.text_slot=table+1000;HS.text_ready=1;return 1;
}
static int hs_descriptors(void) {
    if (HS.descriptors_ready) return 1;
    uint8_t donors[4][88];uint32_t slot;
    for (unsigned i=0;i<4;i++) {
        if (HS.by_id(HF_PRIVATE_CHILDREN[i].id) || HS.catalog_id(HF_PRIVATE_CHILDREN[i].id) || HS.by_delta(HF_HEATSINK_KEYS[i]) || HS.delta(HF_HEATSINK_KEYS[i])) {
            hs_refusal("a private attachment identity is already registered");return 0;
        }
        uintptr_t donor=HS.by_id(HF_PRIVATE_CHILDREN[i].donor_id);
        if (!donor || !hs_copy(donor,donors[i],88) || !hs_copy((uintptr_t)hf_mastery_u64(donors[i]+48),&slot,4) || slot!=5) return 0;
    }
    if (!hf_desc_build(&HS.descriptors,donors,HF_HEATSINK_DONOR_GUARDS,HF_HEATSINK_NAMES,HF_HEATSINK_TEXT_IDS,HS.catalog_generation)) {
        hs_refusal("SAI donor descriptor scalars changed");return 0;
    }
    HS.descriptors_ready=1;return 1;
}
static int hs_sources(void) {
    uintptr_t root=hs_pointer(HS.game+0x346bf98);if (!root) return 0;
    uintptr_t context=hs_pointer(HS.game+0x346bfc8);
    if (context!=root+0x40 || hs_pointer(HS.game+0x3326d48)!=context+0xeb6570) return 0;
    for (unsigned i=0;i<sizeof(HF_HEATSINK_LAYOUT_PINS)/sizeof(HF_HEATSINK_LAYOUT_PINS[0]);i++) {
        const HfHeatsinkLayoutPin* p=&HF_HEATSINK_LAYOUT_PINS[i];uint8_t bytes[12];
        if (!hs_copy((p->exe?HS.exe:HS.game)+p->rva,bytes,p->size) || memcmp(bytes,p->bytes,p->size)) return 0;
    }
    if (HS.slots_live && root!=HS.entity_root) return 0;
    if (HS.slots_live && root==HS.entity_root) {
        for (unsigned i=0;i<2;i++) if (!hs_equal(HS.custom_address[i],HS.custom_after[i],4872) ||
            !hs_equal(HS.heat_source[i],HS.heat[i],592)) return 0;
        return hs_damage_registry(0);
    }
    for (unsigned i=0;i<2;i++) {
        uint32_t stride=0;
        HS.heat_source[i]=HS.api->component_record("WeaponHeatComponentData",HS_ENTITIES[i],&stride);
        if (!HS.heat_source[i] || stride!=592 || !hs_copy(HS.heat_source[i],HS.heat[i],592) ||
            !hs_hash(HS.heat[i],592,HF_HEATSINK_HEAT_SHA[i]) || HS.base_heat(HS_ENTITIES[i])!=HS.heat_source[i]) return 0;
        HS.custom_address[i]=HS.api->component_record("WeaponCustomizationComponentData",HS_ENTITIES[i],&stride);
        if (!HS.custom_address[i] || stride!=4872 || !hs_copy(HS.custom_address[i],HS.custom_before[i],4872) ||
            !hs_hash(HS.custom_before[i],4872,HF_HEATSINK_CUSTOM_SHA[i])) return 0;
        memcpy(HS.custom_after[i],HS.custom_before[i],4872);
        /* Exactly one spare default and slot word are used. Full-row hashes
         * establish their original zero values and all existing defaults. */
        if (hf_mastery_u64(HS.custom_before[i]+24) || hf_cat_u32(HS.custom_before[i]+92)) return 0;
        hf_cat_put32(HS.custom_after[i]+24,5);hf_cat_put32(HS.custom_after[i]+28,HF_PRIVATE_CHILDREN[i*2].id);
        hf_cat_put32(HS.custom_after[i]+92,5);
    }
    if (hs_pointer(HS.game+0x346bf98)!=root || !hs_damage_registry(1)) return 0;
    HS.entity_root=root;return 1;
}
static void hs_tick(HfMod* mod,float dt,void* user) {
    (void)mod;(void)dt;(void)user;
    uint64_t now=GetTickCount64();
    if (InterlockedExchange(&HS.pending_cleanup,0)) { AcquireSRWLockExclusive(&HS.lock);hs_disable_locked(NULL);ReleaseSRWLockExclusive(&HS.lock); }
    if (HS.failed || !HS.hooks_ready || now<HS.next_tick) return;
    HS.next_tick=now+500;
    if (!HS.active && now-HS.start_ms>120000) {
        HS.failed=1;hs_refusal(HS.waiting?HS.waiting:"initial catalogue response was not observed after hook installation");return;
    }
    AcquireSRWLockExclusive(&HS.lock);
    if (HS.active) {
        if (!hs_sources() || !hs_localization() || !hs_ownership()) {
            hs_disable_locked("an active source, catalogue, text or ownership guard changed");
        }
        ReleaseSRWLockExclusive(&HS.lock);return;
    }
    if (!HS.catalog_ready) { HS.waiting="catalogue registration not ready";goto done; }
    if (!HF_SICKLE_RUNNER.done) { HS.waiting="accepted Sickle base profile not ready";goto done; }
    if (!hs_descriptors()) { HS.waiting="private attachment descriptor guard";goto done; }
    if (!hs_sources()) { HS.waiting="heat/source/context guard";goto done; }
    if (!hs_ownership()) { HS.waiting="private catalogue ownership guard";goto done; }
    if (!hs_localization()) { HS.waiting="private localization guard";goto done; }
    HS.waiting="mastery or source activation guard";
    for (unsigned i=0;i<2;i++) {
        uintptr_t source=HS.mastery_fn(HF_PRIVATE_CHILDREN[i*2].weapon);const char* reason=NULL;
        HfMasteryView* view=source?hf_mastery_build(HF_PRIVATE_CHILDREN[i*2].weapon,source,HS.catalog_generation,hs_read,&HS,&reason):NULL;
        if (!view) { hs_refusal(reason?reason:"weapon mastery source unavailable");goto done; }
        HS.mastery[i]=view;
    }
    if (!HS.slots_live) {
        if (!hs_equal(HS.custom_address[0],HS.custom_before[0],4872) || !hs_equal(HS.custom_address[1],HS.custom_before[1],4872)) goto done;
        if (!hs_write_if(HS.custom_address[0],HS.custom_before[0],HS.custom_after[0],4872)) goto done;
        if (!hs_write_if(HS.custom_address[1],HS.custom_before[1],HS.custom_after[1],4872)) {
            hs_undo(HS.custom_address[0],HS.custom_before[0],HS.custom_after[0],4872);goto done;
        }
        HS.slots_live=1;
    }
    HS.epoch++;memset(HS.bindings,0,sizeof(HS.bindings));HS.descriptors.generation=HS.catalog_generation;
    InterlockedExchange(&HS.active,1);
    HS.api->log(HS.mod,"Applied Sickle heatsinks: armory choices, Sickle cooling12, Double-Edge capacity150 with direct heat damage10/20/50");
done:
    ReleaseSRWLockExclusive(&HS.lock);
}
static DWORD WINAPI hs_install_worker(void* user) {
    (void)user;uint8_t bytes[32];
    while (GetTickCount64()-HS.start_ms<60000) {
        if (!HS.game) HS.game=HS.api->module_base("game.dll",&HS.game_size);
        if (!HS.game) { Sleep(25);continue; }
        int ready=1;
        for (unsigned i=0;i<12;i++) {
            const HfHeatsinkCodePin* p=&HF_HEATSINK_HOOK_PINS[i];
            if (!hs_copy(HS.game+p->rva,bytes,32) || memcmp(bytes,p->bytes,32)) { ready=0;break; }
        }
        for (unsigned i=0;ready && i<3;i++) {
            const HfHeatsinkCodePin* p=&HF_HEATSINK_CALL_PINS[i];
            if (!hs_copy(HS.game+p->rva,bytes,32) || memcmp(bytes,p->bytes,32)) ready=0;
        }
        if (!ready) { Sleep(25);continue; }
        HS.base_heat=(HsHashFn)(HS.game+0x50dd30);HS.instance_heat=(HsHashFn)(HS.game+0x50e1f0);HS.create_heat=(HsCreateHeatFn)(HS.game+0x767b10);
        void* detours[12]={(void*)hs_catalog,(void*)hs_by_id,(void*)hs_by_delta,(void*)hs_list,(void*)hs_mastery,
            (void*)hs_delta,(void*)hs_dispatch,(void*)hs_hit,(void*)hs_single,(void*)hs_multi,(void*)hs_catalog_id,(void*)hs_spawn};
        void** originals[12]={(void**)&HS.catalog,(void**)&HS.by_id,(void**)&HS.by_delta,(void**)&HS.list,(void**)&HS.mastery_fn,
            (void**)&HS.delta,(void**)&HS.dispatch,(void**)&HS.hit,(void**)&HS.single,(void**)&HS.multi,(void**)&HS.catalog_id,(void**)&HS.spawn};
        for (unsigned i=0;i<12;i++) if (HS.api->hook(HS.mod,HS.game+HF_HEATSINK_HOOK_PINS[i].rva,detours[i],originals[i])) {
            HS.failed=1;hs_refusal(HF_HEATSINK_HOOK_PINS[i].name);return 0;
        }
        InterlockedExchange(&HS.hooks_ready,1);HS.api->log(HS.mod,"Sickle heatsinks: 12 source-pinned hooks installed; awaiting catalogue and accepted base profile");return 0;
    }
    HS.failed=1;hs_refusal("native hook code did not match within 60 seconds");return 0;
}
static void sickle_heatsinks_begin(const HfApi* api,HfMod* mod) {
    if (HS.api) return;
    HS.api=api;HS.mod=mod;HS.epoch=1;HS.start_ms=GetTickCount64();InitializeSRWLock(&HS.lock);
    if (!api->module_base || !api->read || !api->write) { HS.failed=1;hs_refusal("native memory API is unavailable");return; }
    HS.game=api->module_base("game.dll",&HS.game_size);HS.exe=api->module_base("helldivers2.exe",NULL);
    if (!HS.exe || !api->hook || !api->component_record || !api->on_game_thread ||
        !api->on_game_thread(mod,hs_tick,NULL)) { HS.failed=1;hs_refusal("native adapter scheduling is unavailable");return; }
    HANDLE thread=CreateThread(NULL,0,hs_install_worker,NULL,0,NULL);
    if (!thread) { HS.failed=1;hs_refusal("native adapter worker could not start");return; }
    CloseHandle(thread);
}
