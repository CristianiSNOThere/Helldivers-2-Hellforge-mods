/* Two-round Railgun with an owned damage/projectile profile. Original64 is
 * shared by other weapons; leave its complete row and native pointer intact. */
#include "railgun_guard.generated.h"
#define HF_RG_ENTITY UINT64_C(0x2E9D0BDC48B09E60)
#define HF_RG_MOD "superearth.arsenal-native-port"
typedef struct HfRailgunState {uint32_t damage,projectile,attempted;int applied,poisoned;uintptr_t addresses[8];} HfRailgunState;
typedef struct HfRailgunEdit {HfRef target;HfField field;HfValue before,after;} HfRailgunEdit;
static HfRailgunState HF_RAILGUN;
static uint32_t hf_rg_u32(const uint8_t* p) {uint32_t x;memcpy(&x,p,4);return x;}
static void hf_rg_put(uint8_t* p,uint32_t x) {memcpy(p,&x,4);}
static int hf_rg_equal(HfValue a,HfValue b) {return a.type==b.type && a.number==b.number;}
static int hf_rg_hash(const void* data,size_t size,const char* expected) {
#ifdef HF_RAILGUN_TEST
    extern int hf_rg_test_hash(const void*,size_t,const char*);return hf_rg_test_hash(data,size,expected);
#else
    static HfSickleCrypto crypto;char hash[65];return sickle_hash_bytes(&crypto,data,size,hash) && !strcmp(hash,expected);
#endif
}
static void hf_rg_edits(const HfRailgunState* state,HfRailgunEdit out[8]) {
    const HfRailgunEdit edits[8]={
        {state->damage,HF_FIELD(DamageInfo,durable_damage),HF_I32(225),HF_I32(320)},
        {state->damage,HF_FIELD_AT(DamageInfo,armor_penetration,0),HF_U32(5),HF_U32(10)},
        {state->damage,HF_FIELD_AT(DamageInfo,armor_penetration,1),HF_U32(5),HF_U32(10)},
        {state->damage,HF_FIELD_AT(DamageInfo,armor_penetration,2),HF_U32(5),HF_U32(10)},
        {state->damage,HF_FIELD_AT(DamageInfo,armor_penetration,3),HF_U32(5),HF_U32(10)},
        {HF_RG_ENTITY,HF_FIELD(WeaponMagazine,capacity),HF_U32(1),HF_U32(2)},
        {HF_RG_ENTITY,HF_FIELD(WeaponMagazine,starting_magazines),HF_U32(10),HF_U32(20)},
        {HF_RG_ENTITY,HF_FIELD(ProjectileWeapon,projectile),HF_U32(186),HF_U32(state->projectile)}
    };memcpy(out,edits,sizeof(edits));
}
static int hf_rg_register(const HfApi* api,HfMod* mod,HfRailgunState* state) {
    HfDamage damage={0};HfProjectile projectile={0};
    if(!api || !state || state->damage || api->size<offsetof(HfApi,damage_row)+sizeof(api->damage_row) ||
       !api->add_damage || !api->add_projectile)return 0;
    damage.size=sizeof(damage);damage.base=64u;damage.scale=1.0f;damage.name="superearth.arsenal-native-port/rs-422-railgun/impact";
    state->damage=api->add_damage(mod,&damage);
    if(state->damage<650u || state->damage>=714u)return 0;
    projectile.size=sizeof(projectile);projectile.base=186u;projectile.damage=state->damage;
    projectile.name="superearth.arsenal-native-port/rs-422-railgun/round";state->projectile=api->add_projectile(mod,&projectile);
    return state->projectile>=351u && state->projectile<415u;
}
static int hf_rg_component(const HfApi* api,uintptr_t game,const HfRailgunShape* s,uintptr_t* row,uint8_t* data) {
    uintptr_t root,table,again;uint8_t head[32],entries[542u*16u];unsigned hits=0,aliases=0;
    uint32_t stride=0;
    if(api->read(game+0x346bf98u,&root,8) || root<65536u ||
       api->read(root+0xf12478u+s->component*8u,&table,8) || table<65536u ||
       api->read(table-32u,head,32) || hf_rg_u32(head)!=s->component || memcmp(head+4,s->header,28) ||
       s->slots>542u || api->read(table,entries,s->slots*16u))return 0;
    for(unsigned i=0;i<s->slots;i++) {
        uint64_t id;memcpy(&id,entries+i*16u,8);uint32_t index=hf_rg_u32(entries+i*16u+8u);
        if(id==HF_RG_ENTITY) {if(index!=s->record || hf_rg_u32(entries+i*16u+12u))return 0;hits++;}
        if(id && index==s->record)aliases++;
    }
    *row=table+s->slots*16u+s->record*s->stride;
    if(hits!=1 || aliases!=1 || api->component_record(s->name,HF_RG_ENTITY,&stride)!=*row || stride!=s->stride ||
       api->read(*row,data,s->stride) || api->read(root+0xf12478u+s->component*8u,&again,8) || again!=table)return 0;
    return 1;
}
static int hf_rg_live(const HfApi* api,HfMod* mod,const HfRailgunState* s) {
    HfRailgunEdit edits[8];hf_rg_edits(s,edits);
    for(unsigned i=0;i<8;i++) {
        HfLiveValue v={0};if(!api->get_live(mod,edits[i].target,&edits[i].field,&v))return 0;
        if(s->applied) {
            if(!v.mod || strcmp(v.mod,HF_RG_MOD) || !hf_rg_equal(v.now,edits[i].after) || !hf_rg_equal(v.base,edits[i].after))return 0;
        } else if(!hf_rg_equal(v.now,edits[i].before) || !hf_rg_equal(v.base,edits[i].before) ||
                  (v.mod && v.mod[0] && (i>=5 || strcmp(v.mod,HF_RG_MOD))))return 0;
    }
    return 1;
}
static int hf_rg_preflight(const HfApi* api,HfMod* mod,HfRailgunState* state,char* why,size_t cap) {
    uintptr_t game,dt,pt,slot,stock,custom,row;size_t size=0;uint8_t bytes[616];uintptr_t addresses[8]={0};
    if(!api || !state || state->poisoned || state->damage<650u || state->damage>=714u ||
       state->projectile<351u || state->projectile>=415u || !api->read || !api->write || !api->get_live || !api->set_live ||
       !api->module_base || !api->component_record || !api->damage_row || !api->projectile_row)goto refuse;
    game=api->module_base("game.dll",&size);
    if(!game || !hf_registry_root(api,game,size,HF_REGISTRY_DAMAGE,&dt) ||
       !hf_registry_root(api,game,size,HF_REGISTRY_PROJECTILE,&pt))goto refuse;
    if(api->read(dt+64u*8u,&slot,8) || api->read(game+0x37c60c0u+64u*8u,&stock,8) || slot!=stock ||
       api->read(stock,bytes,76) || !hf_rg_hash(bytes,76,HF_RG_DAMAGE_SHA256))goto refuse;
    if(api->read(pt+186u*8u,&slot,8) || api->read(game+0x37c7670u+186u*8u,&stock,8) || slot!=stock ||
       api->read(stock,bytes,272) || !hf_rg_hash(bytes,272,HF_RG_PROJECTILE_SHA256))goto refuse;
    custom=api->damage_row(state->damage);
    if(custom<65536u || api->read(dt+state->damage*8u,&slot,8) || slot!=custom || api->read(custom,bytes,76) || hf_rg_u32(bytes)!=state->damage)goto refuse;
    hf_rg_put(bytes,64u);
    if(state->applied) {
        if(hf_rg_u32(bytes+8)!=320u)goto refuse;hf_rg_put(bytes+8,225u);
        for(unsigned i=0;i<4;i++) {if(hf_rg_u32(bytes+12u+i*4u)!=10u)goto refuse;hf_rg_put(bytes+12u+i*4u,5u);}
    }
    if(!hf_rg_hash(bytes,76,HF_RG_DAMAGE_SHA256))goto refuse;
    addresses[0]=custom+8u;for(unsigned i=1;i<5;i++)addresses[i]=custom+8u+i*4u;
    custom=api->projectile_row(state->projectile);
    if(custom<65536u || api->read(pt+state->projectile*8u,&slot,8) || slot!=custom || api->read(custom,bytes,272) ||
       hf_rg_u32(bytes)!=state->projectile || hf_rg_u32(bytes+60)!=state->damage)goto refuse;
    hf_rg_put(bytes,186u);hf_rg_put(bytes+60,64u);if(!hf_rg_hash(bytes,272,HF_RG_PROJECTILE_SHA256))goto refuse;
    for(unsigned i=0;i<4;i++) {
        const HfRailgunShape* s=&HF_RG_SHAPES[i];
        if(!hf_rg_component(api,game,s,&row,bytes))goto refuse;
        if(i==0) {
            addresses[5]=row+136u;addresses[6]=row+140u;
            if(state->applied) {
                if(hf_rg_u32(bytes+136)!=2u || hf_rg_u32(bytes+140)!=20u)goto refuse;
                hf_rg_put(bytes+136,1u);hf_rg_put(bytes+140,10u);
            }
        }
        if(i==3) {
            addresses[7]=row;
            if(state->applied) {if(hf_rg_u32(bytes)!=state->projectile)goto refuse;hf_rg_put(bytes,186u);}
        }
        if(!hf_rg_hash(bytes,s->stride,s->sha))goto refuse;
    }
    if(!hf_rg_live(api,mod,state))goto refuse;
    if(state->applied && memcmp(addresses,state->addresses,sizeof(addresses)))goto refuse;
    if(!state->applied)memcpy(state->addresses,addresses,sizeof(addresses));
    return state->applied?2:1;
refuse:(void)snprintf(why,cap,"Railgun registration, component ownership, charge/reload profile or native row guard differs");return state && state->poisoned?-1:0;
}
static int hf_rg_reset(const HfApi* api,HfMod* mod,HfRailgunState* state,char* why,size_t cap) {
    HfRailgunEdit edits[8];hf_rg_edits(state,edits);int failed=0;
    while(state->attempted) {
        unsigned i=--state->attempted;HfRailgunEdit* e=&edits[i];HfLiveValue v={0};uint32_t stride=0;uintptr_t row=0;
        if(i<5) {row=api->damage_row(state->damage);if(row)row+=e->field.offset;}
        else {row=api->component_record(e->field.component,HF_RG_ENTITY,&stride);if(row)row+=e->field.offset;}
        if(!row || row!=state->addresses[i] || !api->get_live(mod,e->target,&e->field,&v)){failed=1;continue;}
        if(hf_rg_equal(v.now,e->before) && hf_rg_equal(v.base,e->before) && (!v.mod || !v.mod[0] || !strcmp(v.mod,HF_RG_MOD))) {
            /* A package-waiting SDK edit is invisible to get_live. Remove our
             * queued request even when its bytes have never changed. NULL only
             * removes this mod's edit; never restores another mod's baseline. */
            if(!api->set_live(mod,e->target,&e->field,NULL,0u))failed=1;
            if(!api->get_live(mod,e->target,&e->field,&v) || !hf_rg_equal(v.now,e->before) ||
               !hf_rg_equal(v.base,e->before) || (v.mod && v.mod[0] && strcmp(v.mod,HF_RG_MOD)))failed=1;
            continue;
        }
        if(!v.mod || strcmp(v.mod,HF_RG_MOD) || !hf_rg_equal(v.base,e->after) ||
           (!hf_rg_equal(v.now,e->after) && !hf_rg_equal(v.now,e->before))){failed=1;continue;}
        if(!api->set_live(mod,e->target,&e->field,NULL,0u))failed=1;
        if(!api->get_live(mod,e->target,&e->field,&v) || !hf_rg_equal(v.now,e->before) || !hf_rg_equal(v.base,e->before) ||
           (v.mod && v.mod[0] && strcmp(v.mod,HF_RG_MOD)))failed=1;
    }
    state->applied=0;if(failed){state->poisoned=1;(void)snprintf(why,cap,"Railgun rollback refused a foreign or unrestorable field");}
    return failed?-1:1;
}
static int hf_rg_apply(const HfApi* api,HfMod* mod,HfRailgunState* state,char* why,size_t cap) {
    int result=hf_rg_preflight(api,mod,state,why,cap);HfRailgunEdit edits[8];if(result!=1)return result;
    hf_rg_edits(state,edits);state->attempted=0;
    for(unsigned i=0;i<8;i++) {
        HfRailgunEdit* e=&edits[i];HfLiveValue v={0};state->attempted=i+1;
        if(!api->set_live(mod,e->target,&e->field,&e->after,0u) || !api->get_live(mod,e->target,&e->field,&v) ||
           !v.mod || strcmp(v.mod,HF_RG_MOD) || !hf_rg_equal(v.now,e->after) || !hf_rg_equal(v.base,e->after))goto rollback;
    }
    state->applied=1;if(hf_rg_preflight(api,mod,state,why,cap)==2)return 1;
rollback:if(hf_rg_reset(api,mod,state,why,cap)<0)return -1;
    (void)snprintf(why,cap,"Railgun write/readback failed; owned changes restored");return 0;
}
