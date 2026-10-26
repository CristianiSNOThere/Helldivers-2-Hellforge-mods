/* Damage reaches its existing maximum at 80% of the unchanged material gauge.
 * The chained getter remains authoritative for other weapons and foreign edits. */
#include "entity_reader.h"
#include "railgun_curve_pins.generated.h"
typedef float (*HfRgGetter)(uintptr_t,uint32_t);
typedef struct HfRgCurve {
    const HfApi* api;HfMod* mod;uintptr_t game,exe,source;HfRgGetter original;
    uint8_t charge[216];int scheduled,pending,ready,active,failed,package_wait_logged;float waited;
    uint32_t observations,last_entity;float last_seconds,last_before,last_after;
} HfRgCurve;
static HfRgCurve HF_RG_CURVE;
static int hf_rg_curve_read(void* ctx,uintptr_t at,void* out,size_t size) {
    HfRgCurve* c=ctx;return c->api->read(at,out,size);
}
static int hf_rg_finite(float value) {uint32_t bits;memcpy(&bits,&value,4);return (bits&0x7f800000u)!=0x7f800000u;}
static float hf_rg_curve_value(float original) {
    if(!hf_rg_finite(original) || original<=1.0f || original>2.5f)return original;
    float result=1.0f+3.0f*(original-1.0f);return result>2.5f?2.5f:result;
}
/* Unlike the generic lookup, distinguish an absent override from unreadable data. */
static int hf_rg_curve_override(HfRgCurve* c,uintptr_t header,uint32_t entity,uint32_t* out) {
    uint8_t raw[20];uintptr_t buckets;uint32_t capacity,empty,multiplier;
    if(!hf_entity_read(hf_rg_curve_read,c,header,raw,20))return -1;
    memcpy(&buckets,raw,8);memcpy(&capacity,raw+8,4);memcpy(&empty,raw+12,4);memcpy(&multiplier,raw+16,4);
    if(!capacity)return 0;
    if(buckets<65536u || (capacity&(capacity-1)) || capacity>1048576u || entity==empty)return -1;
    uint32_t start=entity*multiplier,limit=capacity<4096u?capacity:4096u;
    for(uint32_t i=0;i<limit;i++) {
        uint32_t pair[2];if(!hf_entity_read(hf_rg_curve_read,c,buckets+(size_t)((start+i)%capacity)*8u,pair,8))return -1;
        if(pair[0]==entity) {if(pair[1]==UINT32_MAX)return 0;*out=pair[1];return 1;}
        if(pair[0]==empty)return 0;
    }
    return -1;
}
static int hf_rg_curve_identity(HfRgCurve* c,uint32_t entity,float* released) {
    uintptr_t root,again,manager,refs,ref,states,row;uint32_t index,instance,resolved,check;
    uint8_t desc[24],desc2[24],reference[16],actual[216];uint64_t resource;uint32_t named;
    HfEntityRead rd=hf_rg_curve_read;
    if(!hf_entity_alive(rd,c,c->exe,entity) || !hf_entity_ptr(rd,c,c->game+0x346bf98u,&root) ||
       !hf_entity_hash(rd,c,root+0xf1aeb0u,entity,&index) || index>=0x100000u ||
       !hf_entity_read(rd,c,root+0xf32f18u+(size_t)index*24u,desc,24))return 0;
    memcpy(&resource,desc,8);memcpy(&named,desc+8,4);
    if(resource!=HF_RG_ENTITY || named!=entity ||
       !hf_entity_ptr(rd,c,c->game+0x3326c20u,&manager) ||
       !hf_entity_hash(rd,c,manager+0x20u,entity,&instance) || instance>=0x100000u ||
       !hf_entity_ptr(rd,c,manager+0x38u,&refs) || !hf_entity_ptr(rd,c,refs+(size_t)instance*8u,&ref) ||
       !hf_entity_read(rd,c,ref,reference,16))return 0;
    memcpy(&resource,reference,8);memcpy(&named,reference+8,4);
    if(resource!=HF_RG_ENTITY || named!=entity)return 0;
    uint32_t stride=0;
    if(c->api->component_record("WeaponChargeComponentData",HF_RG_ENTITY,&stride)!=c->source || stride!=216u)return 0;
    row=c->source;int override=hf_rg_curve_override(c,manager+0x50u,entity,&resolved);
    if(override<0)return 0;
    if(override>0) {
        if(resolved>=0x100000u || !hf_entity_ptr(rd,c,manager+0x90u,&row))return 0;
        row+=(size_t)resolved*216u;
    }
    if(!hf_entity_read(rd,c,row,actual,216) || memcmp(actual,c->charge,216) ||
       !hf_entity_read(rd,c,c->source,actual,216) || memcmp(actual,c->charge,216) ||
       !hf_entity_ptr(rd,c,manager+0x40u,&states) ||
       !hf_entity_read(rd,c,states+(size_t)instance*40u+8u,released,4) || !hf_rg_finite(*released) ||
       !hf_entity_ptr(rd,c,c->game+0x346bf98u,&again) || again!=root ||
       !hf_entity_hash(rd,c,root+0xf1aeb0u,entity,&check) || check!=index ||
       !hf_entity_read(rd,c,root+0xf32f18u+(size_t)index*24u,desc2,24) || memcmp(desc,desc2,24) ||
       !hf_entity_ptr(rd,c,c->game+0x3326c20u,&again) || again!=manager ||
       !hf_entity_hash(rd,c,manager+0x20u,entity,&check) || check!=instance ||
       !hf_entity_alive(rd,c,c->exe,entity))return 0;
    return 1;
}
static float hf_rg_curve_detour(uintptr_t unused,uint32_t entity) {
    HfRgCurve* c=&HF_RG_CURVE;float original=c->original(unused,entity),seconds;
    if(!c->active || !hf_rg_finite(original) || original<=1.0f || original>2.5f ||
       !hf_rg_curve_identity(c,entity,&seconds) || seconds<=0.5f)return original;
    float fraction=(seconds-0.5f)/2.5f;if(fraction>1.0f)fraction=1.0f;
    float expected=1.0f+1.5f*fraction,difference=expected-original;
    if(difference>0.00001f || difference< -0.00001f)return original;
    float result=hf_rg_curve_value(original);
    c->last_entity=entity;c->last_seconds=seconds;c->last_before=original;c->last_after=result;
    if(c->observations++==0)c->api->log(c->mod,"Railgun charge observed: entity%u seconds%.6f multiplier%.6f -> %.6f; 80%% damage plateau active",entity,(double)seconds,(double)original,(double)result);
    return result;
}
static void hf_rg_curve_tick(HfMod* mod,float dt,void* user) {
    HfRgCurve* c=user;const HfApi* api=c->api;char why[256];uintptr_t source;uint8_t data[616];size_t size=0;
    (void)mod;if(!c->pending || c->failed || c->active)return;
    if(hf_rg_finite(dt) && dt>0.0f)c->waited+=dt;
    int package=api->load_package(c->mod,UINT64_C(0x7F37DB9B767844C2));
    if(!package) {c->failed=1;c->pending=0;api->log(c->mod,"Refused Railgun: weapon package unavailable; source fields unchanged");return;}
    if(package!=HF_PACKAGE_RESIDENT) {
        if(!c->package_wait_logged++)api->log(c->mod,"Railgun waiting for held weapon package 7f37db9b767844c2");
        goto waiting;
    }
    c->game=api->module_base("game.dll",&size);c->exe=api->module_base("helldivers2.exe",&size);
    if(!c->game || !c->exe || hf_rg_preflight(api,c->mod,&HF_RAILGUN,why,sizeof(why))!=1 ||
       !hf_rg_component(api,c->game,&HF_RG_SHAPES[2],&source,data))goto waiting;
    if(!c->ready) {
        for(unsigned i=0;i<sizeof(HF_RG_PINS)/sizeof(HF_RG_PINS[0]);i++)
            if(!api->code_matches(c->game+HF_RG_PINS[i].rva,HF_RG_PINS[i].bytes))goto waiting;
        if(api->hook(c->mod,c->game+0x73e480u,(void*)hf_rg_curve_detour,(void**)&c->original) || !c->original) {
            c->failed=1;api->log(c->mod,"Refused Railgun: damage getter hook unavailable; source fields unchanged");return;
        }
        c->ready=1;api->log(c->mod,"Railgun charge getter hook ready; exact charge/resolver/failure code pins matched");
    }
    c->source=source;memcpy(c->charge,data,216);
    int result=hf_rg_apply(api,c->mod,&HF_RAILGUN,why,sizeof(why));c->pending=0;
    if(result==1 || result==2) {
        c->active=1;api->log(c->mod,"Applied Railgun: two rounds per magazine,20 spare magazines, AP10; private projectile%u/damage%u; maximum damage at80%% gauge (1.333333s), overcharge unchanged at3s",HF_RAILGUN.projectile,HF_RAILGUN.damage);
    } else {c->failed=1;api->log(c->mod,"%s Railgun: %s",result<0?"FATAL:":"Refused",why);}
    return;
waiting:
    if(c->waited>=90.0f) {c->failed=1;c->pending=0;api->log(c->mod,"Refused Railgun: charge code/component readiness timed out; source fields unchanged");}
}
static void hf_rg_curve_begin(const HfApi* api,HfMod* mod) {
    (void)hf_entity_held; /* Shared reader also supports heatsinks when selected. */
    HfRgCurve* c=&HF_RG_CURVE;c->api=api;c->mod=mod;
    if(api->size<offsetof(HfApi,code_matches)+sizeof(api->code_matches) || !api->code_matches || !api->hook ||
       !api->load_package || !api->on_game_thread || !api->on_game_thread(mod,hf_rg_curve_tick,c)) {
        c->failed=1;api->log(mod,"Refused Railgun: SDK13 charge hook/scheduling unavailable");return;
    }
    c->scheduled=1;
}
static void hf_rg_curve_request(const HfApi* api,HfMod* mod) {
    HfRgCurve* c=&HF_RG_CURVE;
    if(c->active || c->pending || c->failed)return;
    if(!c->scheduled) {api->log(mod,"Refused Railgun: charge scheduler unavailable");return;}
    c->pending=1;c->waited=0;api->log(mod,"Queued Railgun private profile and80%% damage plateau on game thread");
}
