#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#if defined(_WIN32) && !defined(ENEMY_CHARGER_TEST)
#include <windows.h>
#endif
#include "hellforge_mod.h"
#include "charger_data.h"

typedef uint8_t (*ChargerStop)(void *,uint32_t);
typedef void (*ChargerEmit)(void *,uint32_t);
static const HfApi *c_api;
static HfMod *c_self;
static uintptr_t c_base;
static ChargerStop c_original;
static ChargerEmit c_emitter;
static _Atomic unsigned c_enabled;
static int c_installed;
static _Thread_local int c_inside;
static unsigned c_requests;

static int c_region(uintptr_t at,size_t size)
{
    if(at<0x10000 || at>=UINT64_C(0x800000000000) || !size || size>UINT64_C(0x800000000000)-at)return 0;
#if defined(_WIN32) && !defined(ENEMY_CHARGER_TEST)
    uintptr_t end=at+size;
    while(at<end) {
        MEMORY_BASIC_INFORMATION v;
        if(!VirtualQuery((void*)at,&v,sizeof v) || v.State!=MEM_COMMIT || v.Type!=MEM_PRIVATE ||
           (v.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
           !(v.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY)))return 0;
        uintptr_t next=(uintptr_t)v.BaseAddress+v.RegionSize;
        if(next<=at)return 0;
        at=next;
    }
#endif
    return 1;
}
static int c_match(uintptr_t at,const uint8_t *expected,size_t size)
{
    uint8_t scratch[512];
    while(size) {
        size_t count=size<sizeof scratch?size:sizeof scratch;
        if(c_api->read(at,scratch,count) || memcmp(scratch,expected,count))return 0;
        at+=count;expected+=count;size-=count;
    }
    return 1;
}
static int c_module(void)
{
    size_t size=0;
    return c_base>=65536 && c_api->module_base("game.dll",&size)==c_base && size==74727424u &&
        c_base<=UINT64_C(0x800000000000)-size;
}
/* Active tables may include appended foreign entities. A reviewed target
 * keeps its exact owner slot, unique row ownership and complete stock record. */
static int c_table(unsigned component,unsigned target,uintptr_t root)
{
    const ChargerTable *t=&c_tables[component];
    uintptr_t slot=root+0xF12478u+(uintptr_t)t->index*8u,table=0,again=0;
    uint8_t header[32],entries[512];unsigned owners=0,aliases=0,matched=0;
    if(c_api->read(slot,&table,8) || table<65568 ||
       !c_region(table-32u,t->size+32u) || c_api->read(table-32u,header,32) ||
       memcmp(header,&t->index,4) || memcmp(header+4,t->header+8,4) ||
       memcmp(header+8,t->header,24))return 0;
    for(unsigned offset=0;offset<t->map_size;offset+=sizeof entries) {
        unsigned count=t->map_size-offset;
        if(count>sizeof entries)count=sizeof entries;
        if(c_api->read(table+offset,entries,count))return 0;
        for(unsigned j=0;j<count;j+=16) {
            uint64_t identity=0;uint32_t row=0;
            memcpy(&identity,entries+j,8);memcpy(&row,entries+j+8,4);
            if(identity==c_resources[target]) {
                owners++;
                if(!memcmp(t->map+offset+j,entries+j,16))matched++;
            }
            if(identity && row==t->rows[target])aliases++;
        }
    }
    if(owners!=1 || aliases!=1 || matched!=1 ||
       (uint64_t)t->map_size+((uint64_t)t->rows[target]+1u)*t->stride>t->size ||
       !c_match(table+t->map_size+(uintptr_t)t->rows[target]*t->stride,t->baseline[target],t->stride) ||
       c_api->read(slot,&again,8) || again!=table ||
       !c_match(table-32u,header,32))return 0;
    return 1;
}
static int c_records(unsigned target)
{
    uintptr_t root=0,again=0;
    if(target>=4 || !c_module() || c_api->read(c_base+0x346BF98u,&root,8) ||
       root<65536 || root>UINT64_C(0x800000000000)-0xF13000u ||
       !c_match(c_base+0x511CC0u,c_getter_code,sizeof c_getter_code))return 0;
    for(unsigned i=0;i<3;++i)if(!c_table(i,target,root))return 0;
    return !c_api->read(c_base+0x346BF98u,&again,8) && again==root;
}
#include "charger_hold.h"
static uint8_t c_stop(void *entity,uint32_t phase)
{
    /* The original always owns the ability's actors, motion, timing and cleanup. */
    if(c_inside)return c_original(entity,phase);
    c_inside=1;
    int holding=0;uint32_t native_phase=c_hold_phase(entity,phase,&holding);
    uint8_t result=c_original(entity,native_phase);
    if(holding)result=0;
    unsigned enabled=atomic_load_explicit(&c_enabled,memory_order_acquire);
    if(phase==0 && enabled && c_emitter && c_module()) {
        uint8_t identity[24],again[24];
        if(!c_api->read((uintptr_t)entity,identity,sizeof identity)) {
            uint64_t resource=0;uint32_t unit=0;
            memcpy(&resource,identity,8);memcpy(&unit,identity+12,4);
            for(unsigned i=0;i<4;++i) {
                if(resource!=c_resources[i] || !(enabled&(1u<<i)) || !(identity[20]&1u) || !unit)continue;
                if(c_records(i) &&
                   c_match(c_base+0x10A87B0u+32u,c_stop_code+32,sizeof c_stop_code-32u) &&
                   c_match(c_base+0x119FEC0u,c_emit_code,sizeof c_emit_code) &&
                   !c_api->read((uintptr_t)entity,again,sizeof again) && !memcmp(identity,again,sizeof identity)) {
                    c_emitter(entity,UINT32_C(0xC38F50FB));
                    c_hold_arm((uintptr_t)entity,identity,i);
                    if(c_requests<4) {
                        ++c_requests;
                        c_api->log(c_self,"CHARGER BRAKE_REQUESTED: resource %016llX; live brake clip requested; raised-rear hold +0.5s; visual acceptance pending",(unsigned long long)resource);
                    }
                }
                break;
            }
        }
    }
    c_inside=0;
    return result;
}
static void c_loaded(HfMod *mod,HfEvent event,void *user)
{
    (void)mod;(void)user;
    if(event!=HF_EVENT_GAME_LOADED || c_installed)return;
    size_t size=0;c_base=c_api->module_base("game.dll",&size);
    if(!c_module() || !c_match(c_base+0x10A87B0u,c_stop_code,sizeof c_stop_code) ||
       !c_match(c_base+0x119FEC0u,c_emit_code,sizeof c_emit_code) ||
       !c_match(c_base+0x511CC0u,c_getter_code,sizeof c_getter_code)) {
        c_api->log(c_self,"CHARGER REFUSED: reviewed module, stop, animation helper or AI getter changed");return;
    }
    c_emitter=(ChargerEmit)(c_base+0x119FEC0u);
    if(c_api->hook(c_self,c_base+0x10A87B0u,(void*)c_stop,(void**)&c_original) || !c_original) {
        c_api->log(c_self,"CHARGER REFUSED: stopping hook unavailable; other balance features remain independent");return;
    }
    c_installed=1;
    c_api->log(c_self,"CHARGER HOOK_READY: existing stopping handler retained; waiting for reviewed entity records");
}
static void c_ready(HfMod *mod,HfEvent event,void *user)
{
    (void)mod;(void)user;
    if(event!=HF_EVENT_ENTITIES_READY)return;
    c_hold_reload();
    atomic_store_explicit(&c_enabled,0,memory_order_release);
    if(!c_installed)return;
    unsigned enabled=0;
    for(unsigned i=0;i<4;++i) {
        if(c_records(i))enabled|=1u<<i;
        else c_api->log(c_self,"CHARGER RESOURCE_REFUSED: %016llX; ownership or complete AI/behavior/unit baseline changed",(unsigned long long)c_resources[i]);
    }
    atomic_store_explicit(&c_enabled,enabled,memory_order_release);
    c_api->log(c_self,"CHARGER READY: verified variant mask %X; animation requests pending",enabled);
}
int enemy_charger_init(const HfApi *hf,HfMod *mod)
{
    if(!hf || hf->version<11 || hf->size<offsetof(HfApi,hook)+sizeof hf->hook ||
       !hf->log || !hf->build_id || !hf->on || !hf->read || !hf->module_base || !hf->hook)return 0;
    c_api=hf;c_self=mod;
    const char *build=hf->build_id();
    if(!build || strcmp(build,"25480438")) {
        hf->log(mod,"CHARGER REFUSED: supported game build is 25480438");return 0;
    }
    if(hf->on(mod,HF_EVENT_GAME_LOADED,c_loaded,NULL) || hf->on(mod,HF_EVENT_ENTITIES_READY,c_ready,NULL))
        hf->log(mod,"CHARGER REFUSED: lifecycle callback registration failed");
    return 0;
}
