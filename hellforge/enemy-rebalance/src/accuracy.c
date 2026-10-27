#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if defined(_WIN32) && !defined(ENEMY_ACCURACY_TEST)
#include <windows.h>
#endif
#include "hellforge_mod.h"
#include "accuracy_data.h"

static const HfApi *a_api;
static HfMod *a_self;
static uintptr_t a_owned[A_PROFILE_COUNT];
static uint8_t a_expected[A_PROFILE_COUNT][A_ROW_SIZE];
static uint8_t a_current[A_ROW_SIZE];
static int a_result;

static int a_region(uintptr_t at)
{
    if(at<UINT64_C(0x10000) || at>=UINT64_C(0x800000000000) ||
       A_ROW_SIZE>UINT64_C(0x800000000000)-at) return 0;
#if defined(_WIN32) && !defined(ENEMY_ACCURACY_TEST)
    uintptr_t end=at+A_ROW_SIZE;
    while(at<end) {
        MEMORY_BASIC_INFORMATION info;
        if(!VirtualQuery((const void*)at,&info,sizeof info) || info.State!=MEM_COMMIT ||
           info.Type!=MEM_PRIVATE || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
           !(info.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY))) return 0;
        uintptr_t next=(uintptr_t)info.BaseAddress+info.RegionSize;
        if(next<=at) return 0;
        at=next;
    }
#endif
    return 1;
}

/* The SDK refuses missing, duplicate, or shared record owners. Resolve again
 * around every read/store so a replaced table never leaves a stale write target. */
static uintptr_t a_resolve(unsigned index)
{
    uint32_t stride=0;
    uintptr_t row=a_api->component_record("WeaponDataComponentData",accuracy_profiles[index].resource,&stride);
    return row && stride==A_ROW_SIZE && a_region(row)?row:0;
}
static int a_read(unsigned index,uintptr_t row,uint8_t *out)
{
    return a_resolve(index)==row && !a_api->read(row,out,A_ROW_SIZE) && a_resolve(index)==row;
}
static void a_after(unsigned index,uint8_t *out)
{
    memcpy(out,accuracy_profiles[index].before,A_ROW_SIZE);
    memcpy(out+84u,accuracy_profiles[index].after,8u);
}
static void a_refused(const char *reason,unsigned index)
{
    a_result=-1;
    a_api->log(a_self,"ACCURACY REFUSED: %s (%s); bot spread transaction stopped",reason,accuracy_profiles[index].label);
}
static int a_partial_is_ours(unsigned index,const uint8_t *row)
{
    const uint8_t *before=accuracy_profiles[index].before;
    const uint8_t *after=(const uint8_t*)accuracy_profiles[index].after;
    if(memcmp(row,before,84u) || memcmp(row+92u,before+92u,A_ROW_SIZE-92u)) return 0;
    for(unsigned byte=0;byte<8u;++byte)
        if(row[84u+byte]!=before[84u+byte] && row[84u+byte]!=after[byte]) return 0;
    return 1;
}
static int a_rollback(const uintptr_t *rows,unsigned attempted)
{
    int restored=1;
    while(attempted) {
        unsigned edit=--attempted,index=edit/2u,axis=edit%2u,offset=84u+4u*axis;
        uint32_t before=0,current=0,after=accuracy_profiles[index].after[axis];
        memcpy(&before,accuracy_profiles[index].before+offset,4u);
        if(!a_read(index,rows[index],a_current)) {restored=0;continue;}
        memcpy(&current,a_current+offset,4u);
        if(current==before) continue;
        if(current!=after && !a_partial_is_ours(index,a_current)) {restored=0;continue;}
        if(a_resolve(index)!=rows[index] || a_api->write(rows[index]+offset,&before,4u) ||
           !a_read(index,rows[index],a_current) || memcmp(a_current+offset,&before,4u)) restored=0;
    }
    a_api->log(a_self,"ACCURACY rollback: %s",restored?"verified":"INCOMPLETE; foreign or retired records preserved");
    return restored;
}
static void a_ready(HfMod *mod,HfEvent event,void *user)
{
    (void)mod;(void)user;
    if(event!=HF_EVENT_ENTITIES_READY) return;
    uintptr_t rows[A_PROFILE_COUNT];
    unsigned owned_count=0;
    a_result=0;
    for(unsigned i=0;i<A_PROFILE_COUNT;++i) {
        rows[i]=a_resolve(i);
        if(!rows[i] || !a_read(i,rows[i],a_current)) {a_refused("weapon record owner, stride or memory bounds changed",i);return;}
        for(unsigned j=0;j<i;++j)
            if(rows[i]<rows[j]+A_ROW_SIZE && rows[j]<rows[i]+A_ROW_SIZE) {
                a_refused("weapon records overlap",i);return;
            }
        a_after(i,a_expected[i]);
        if(a_owned[i]==rows[i] && !memcmp(a_current,a_expected[i],A_ROW_SIZE)) ++owned_count;
        else if(memcmp(a_current,accuracy_profiles[i].before,A_ROW_SIZE)) {
            a_refused("complete weapon baseline changed or another mod owns these values",i);return;
        }
    }
    if(owned_count==A_PROFILE_COUNT) {
        a_result=2;
        a_api->log(a_self,"ACCURACY ALREADY_APPLIED: all %u owned bot weapon profiles verified",A_PROFILE_COUNT);
        return;
    }
    if(owned_count) {a_refused("mixed ownership state",0);return;}
    for(unsigned i=0;i<A_PROFILE_COUNT;++i) memcpy(a_expected[i],accuracy_profiles[i].before,A_ROW_SIZE);
    unsigned attempted=0;
    for(unsigned i=0;i<A_PROFILE_COUNT;++i) {
        for(unsigned axis=0;axis<2u;++axis) {
            unsigned offset=84u+4u*axis;
            if(!a_read(i,rows[i],a_current) || memcmp(a_current,a_expected[i],A_ROW_SIZE)) {
                (void)a_rollback(rows,attempted);a_refused("weapon changed before commit",i);return;
            }
            ++attempted;
            int failed=a_api->write(rows[i]+offset,&accuracy_profiles[i].after[axis],4u);
            memcpy(a_expected[i]+offset,&accuracy_profiles[i].after[axis],4u);
            if(failed || !a_read(i,rows[i],a_current) || memcmp(a_current,a_expected[i],A_ROW_SIZE)) {
                (void)a_rollback(rows,attempted);a_refused("write or readback failed",i);return;
            }
        }
    }
    for(unsigned i=0;i<A_PROFILE_COUNT;++i)
        if(!a_read(i,rows[i],a_current) || memcmp(a_current,a_expected[i],A_ROW_SIZE)) {
            (void)a_rollback(rows,attempted);a_refused("weapon changed during final verification",i);return;
        }
    for(unsigned i=0;i<A_PROFILE_COUNT;++i) a_owned[i]=rows[i];
    a_result=1;
    a_api->log(a_self,"ACCURACY APPLIED: horizontal and vertical projectile spread +25%% on %u bot weapon profiles (%u fields); Tank, Factory Strider and Cyborg weapon profiles not edited",A_PROFILE_COUNT,A_PROFILE_COUNT*2u);
}
int enemy_accuracy_init(const HfApi *api,HfMod *mod)
{
    if(!api || api->version<13 || api->size<offsetof(HfApi,component_record)+sizeof api->component_record ||
       !api->component_record || !api->build_id || !api->on || !api->read || !api->write || !api->log) return 1;
    a_api=api;a_self=mod;
    const char *build=api->build_id();
    if(!build || strcmp(build,"25480438")) {a_refused("supported game build is 25480438",0);return 1;}
    api->log(mod,"INIT: Enemy Rebalance 0.7.0 bot spread +25%%; waiting for entity data");
    return api->on(mod,HF_EVENT_ENTITIES_READY,a_ready,NULL);
}
