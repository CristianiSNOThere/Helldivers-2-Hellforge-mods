/* Keep the Scorcher's accepted impact profile on its own registered DamageInfo.
 * Stock damage56 also serves Loyalist171 and projectile285; never write it.
 * Only the reviewed Scorcher projectile profiles142/143 receive owned links. */
#include "scorcher_guard.generated.h"
#define HF_SC_MOD "superearth.arsenal-native-port"
typedef struct HfScorcherPrivate {
    uint32_t damage,attempted;
    int applied,poisoned;
    uintptr_t records[3];
} HfScorcherPrivate;
static HfScorcherPrivate HF_SCORCHER;
typedef struct HfScorcherEdit {uint64_t target;HfField field;HfValue before,after;uint32_t flags;} HfScorcherEdit;

static uint32_t hf_sc_word(const uint8_t* p) {uint32_t n;memcpy(&n,p,4);return n;}
static void hf_sc_put(uint8_t* p,uint32_t n) {memcpy(p,&n,4);}
static int hf_sc_equal(HfValue a,HfValue b) {return a.type==b.type && a.number==b.number;}
static void hf_sc_edits(const HfScorcherPrivate* state,HfScorcherEdit out[4]) {
    const HfScorcherEdit edits[4]={
        {state->damage,HF_FIELD(DamageInfo,standard_damage),HF_I32(100),HF_I32(75),0u},
        {state->damage,HF_FIELD(DamageInfo,durable_damage),HF_I32(50),HF_I32(38),0u},
        {142u,HF_FIELD(ProjectileSettings,damage),HF_U32(56),HF_U32(state->damage),HF_LIVE_SHARED},
        {143u,HF_FIELD(ProjectileSettings,damage),HF_U32(56),HF_U32(state->damage),HF_LIVE_SHARED}
    };memcpy(out,edits,sizeof(edits));
}
static int hf_sc_register(const HfApi* api,HfMod* mod,HfScorcherPrivate* state) {
    HfDamage d={0};
    if(!api || !state || state->damage || api->size<offsetof(HfApi,damage_row)+sizeof(api->damage_row) ||
       !api->add_damage || !api->damage_row)return 0;
    d.size=sizeof(d);d.base=56u;d.scale=1.0f;d.name="superearth.arsenal-native-port/plas-1-scorcher-ar/impact";
    state->damage=api->add_damage(mod,&d);
    return state->damage>=650u && state->damage<714u;
}
static int hf_sc_hash(const void* data,size_t size,const char* expected) {
#ifdef HF_SCORCHER_TEST
    extern int hf_sc_test_hash(const void*,size_t,const char*);
    return hf_sc_test_hash(data,size,expected);
#else
    static HfSickleCrypto crypto;char actual[65];
    return sickle_crypto_init(&crypto) && sickle_hash_bytes(&crypto,data,size,actual) && !strcmp(actual,expected);
#endif
}

/* Prove the real native lookup registry and original slot agree. SDK-owned
 * expansion is joined through the existing mod witness in registry_bridge.inc.
 * A byte-identical foreign pointer is still not our stock record. */
static int hf_sc_stock(const HfApi* api,uintptr_t game,size_t image_size,int kind,
                       uint32_t id,uint32_t size,uint8_t* bytes,uintptr_t* address) {
    uintptr_t table,active,original;
    uint32_t stock=kind==HF_REGISTRY_DAMAGE?0x37C60C0u:0x37C7670u;
    if(!hf_registry_root(api,game,image_size,kind,&table) ||
       api->read(table+(uintptr_t)id*8u,&active,8) ||
       api->read(game+stock+(uintptr_t)id*8u,&original,8) || active!=original || active<65536u ||
       (active&3u) || api->read(active,bytes,size) || hf_sc_word(bytes)!=id)return 0;
    *address=active;return 1;
}
static int hf_sc_fields(const HfApi* api,HfMod* mod,const HfScorcherPrivate* state,int applied) {
    HfScorcherEdit edits[4];hf_sc_edits(state,edits);
    for(unsigned i=0;i<4;i++) {
        HfLiveValue v={0};HfScorcherEdit* e=&edits[i];
        if(!api->get_live(mod,e->target,&e->field,&v))return 0;
        if(v.mod && v.mod[0] && strcmp(v.mod,HF_SC_MOD))return 0;
        if(applied) {
            if(!v.mod || strcmp(v.mod,HF_SC_MOD) || !hf_sc_equal(v.base,e->after) || !hf_sc_equal(v.now,e->after))return 0;
        } else if(!hf_sc_equal(v.now,e->before) || !hf_sc_equal(v.base,e->before) ||
                  (i>=2 && v.mod && v.mod[0]))return 0;
    }
    return 1;
}
static int hf_sc_preflight(const HfApi* api,HfMod* mod,HfScorcherPrivate* state,char* why,size_t cap) {
    static const uint32_t ids[4]={142u,143u,171u,285u};
    static const char* const hashes[4]={HF_SC_PROJECTILE_142_SHA256,HF_SC_PROJECTILE_143_SHA256,
        HF_SC_PROJECTILE_171_SHA256,HF_SC_PROJECTILE_285_SHA256};
    uintptr_t game,table,address,reported,records[3]={0};size_t image_size=0;uint8_t bytes[272];
    if(!api || !state || state->poisoned || state->damage<650u || state->damage>=714u ||
       !api->read || !api->write || !api->get_live || !api->set_live || !api->module_base ||
       !api->damage_row || !api->projectile_row)goto refuse;
    game=api->module_base("game.dll",&image_size);
    if(!game || !hf_sc_stock(api,game,image_size,HF_REGISTRY_DAMAGE,56u,76u,bytes,&address) ||
       !hf_sc_hash(bytes,76u,HF_SC_DAMAGE_56_SHA256))goto refuse;
    for(unsigned i=0;i<4;i++) {
        if(!hf_sc_stock(api,game,image_size,HF_REGISTRY_PROJECTILE,ids[i],272u,bytes,&address))goto refuse;
        if(i<2)records[i+1]=address;
        if(i<2 && state->applied) {
            if(hf_sc_word(bytes+60)!=state->damage)goto refuse;
            hf_sc_put(bytes+60,56u);
        }
        if(!hf_sc_hash(bytes,272u,hashes[i]))goto refuse;
    }
    reported=api->damage_row(state->damage);
    records[0]=reported;
    if(!hf_registry_root(api,game,image_size,HF_REGISTRY_DAMAGE,&table) || reported<65536u ||
       api->read(table+(uintptr_t)state->damage*8u,&address,8) || address!=reported ||
       api->read(reported,bytes,76u) || hf_sc_word(bytes)!=state->damage)goto refuse;
    hf_sc_put(bytes,56u);
    if(state->applied) {
        if(hf_sc_word(bytes+4)!=75u || hf_sc_word(bytes+8)!=38u)goto refuse;
        hf_sc_put(bytes+4,100u);hf_sc_put(bytes+8,50u);
    }
    if(!hf_sc_hash(bytes,76u,HF_SC_DAMAGE_56_SHA256) || !hf_sc_fields(api,mod,state,state->applied))goto refuse;
    if(state->applied && memcmp(records,state->records,sizeof(records)))goto refuse;
    if(!state->applied)memcpy(state->records,records,sizeof(records));
    return state->applied?2:1;
refuse:
    (void)snprintf(why,cap,"Scorcher private damage, original Loyalist record, projectile registry or field ownership differs");
    return state && state->poisoned?-1:0;
}
static int hf_sc_reset(const HfApi* api,HfMod* mod,HfScorcherPrivate* state,char* why,size_t cap) {
    HfScorcherEdit edits[4];int failed=0;hf_sc_edits(state,edits);
    while(state->attempted) {
        HfScorcherEdit* e=&edits[--state->attempted];HfLiveValue v={0};
        unsigned index=state->attempted<2?0:state->attempted-1;
        uintptr_t actual=0;
        if(state->attempted<2)actual=api->damage_row(state->damage);
        else {
            uint8_t row[272];size_t image_size=0;uintptr_t game=api->module_base("game.dll",&image_size);
            if(!game || !hf_sc_stock(api,game,image_size,HF_REGISTRY_PROJECTILE,(uint32_t)e->target,272u,row,&actual))actual=0;
        }
        if(!actual || actual!=state->records[index]){failed=1;continue;}
        if(!api->get_live(mod,e->target,&e->field,&v)){failed=1;continue;}
        if(hf_sc_equal(v.now,e->before) && hf_sc_equal(v.base,e->before) &&
           (!v.mod || !v.mod[0] || !strcmp(v.mod,HF_SC_MOD)))continue;
        if(!v.mod || strcmp(v.mod,HF_SC_MOD) || !hf_sc_equal(v.base,e->after) ||
           (!hf_sc_equal(v.now,e->after) && !hf_sc_equal(v.now,e->before))) {failed=1;continue;}
        (void)api->set_live(mod,e->target,&e->field,NULL,e->flags);
        if(!api->get_live(mod,e->target,&e->field,&v) || !hf_sc_equal(v.now,e->before) ||
           !hf_sc_equal(v.base,e->before) || (v.mod && v.mod[0] && strcmp(v.mod,HF_SC_MOD)))failed=1;
    }
    state->applied=0;
    if(failed){state->poisoned=1;(void)snprintf(why,cap,"Scorcher private edits could not roll back without overwriting foreign values");}
    return failed?-1:1;
}
static int hf_sc_apply(const HfApi* api,HfMod* mod,HfScorcherPrivate* state,char* why,size_t cap) {
    HfScorcherEdit edits[4];int ready=hf_sc_preflight(api,mod,state,why,cap);
    if(ready!=1)return ready;
    hf_sc_edits(state,edits);state->attempted=0;
    for(unsigned i=0;i<4;i++) {
        HfScorcherEdit* e=&edits[i];HfLiveValue v={0};state->attempted=i+1u;
        if(!api->set_live(mod,e->target,&e->field,&e->after,e->flags) ||
           !api->get_live(mod,e->target,&e->field,&v) || !v.mod || strcmp(v.mod,HF_SC_MOD) ||
           !hf_sc_equal(v.now,e->after) || !hf_sc_equal(v.base,e->after))goto rollback;
    }
    state->applied=1;
    if(hf_sc_preflight(api,mod,state,why,cap)==2)return 1;
rollback:
    if(hf_sc_reset(api,mod,state,why,cap)<0)return -1;
    (void)snprintf(why,cap,"Scorcher private setup failed; its owned impact edits were restored");return 0;
}
