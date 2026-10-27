#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if defined(_WIN32) && !defined(ENEMY_MOVEMENT_TEST)
#include <windows.h>
#endif
#include "hellforge_mod.h"
#include "movement_data.h"

#define M_SIZE 732u
#define M_COUNT (sizeof movement_edits/sizeof movement_edits[0])
static const HfApi *m_api;
static HfMod *m_self;
static uintptr_t m_owned;
static int m_result;
static uint8_t m_snapshot[M_SIZE],m_expected[M_SIZE],m_current[M_SIZE];

static int m_region(uintptr_t at,size_t size)
{
    if(at<0x10000 || at>=UINT64_C(0x800000000000) || !size || size>UINT64_C(0x800000000000)-at) return 0;
#if defined(_WIN32) && !defined(ENEMY_MOVEMENT_TEST)
    uintptr_t end=at+size;
    while(at<end) {
        MEMORY_BASIC_INFORMATION v;
        if(!VirtualQuery((void*)at,&v,sizeof v) || v.State!=MEM_COMMIT || v.Type!=MEM_PRIVATE ||
           (v.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
           !(v.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY))) return 0;
        uintptr_t next=(uintptr_t)v.BaseAddress+v.RegionSize;
        if(next<=at) return 0;
        at=next;
    }
#endif
    return 1;
}
#include "movement_lookup.h"
static int m_read(uintptr_t row,uint8_t *out)
{
    return m_resolve()==row && !m_api->read(row,out,M_SIZE) && m_resolve()==row;
}
static void m_after(uint8_t *row)
{
    for(unsigned i=0;i<M_COUNT;++i)memcpy(row+movement_edits[i].offset,&movement_edits[i].after,4);
}
static void m_refuse(const char *reason)
{
    m_result=-1;
    m_api->log(m_self,"MOVEMENT REFUSED: %s; Alpha Warrior edits stopped",reason);
}
static void m_rollback(uintptr_t row,unsigned attempted)
{
    int restored=1;
    while(attempted) {
        const MovementEdit *e=&movement_edits[--attempted];
        uint8_t now[4],before[4],after[4];
        memcpy(before,&e->before,4);memcpy(after,&e->after,4);
        if(m_resolve()!=row || m_api->read(row+e->offset,now,4) || m_resolve()!=row) {restored=0;continue;}
        int ours=1;
        for(unsigned k=0;k<4;++k)if(now[k]!=before[k] && now[k]!=after[k])ours=0;
        if(!ours) {restored=0;continue;}
        if(!memcmp(now,before,4))continue;
        if(m_api->write(row+e->offset,before,4) || m_resolve()!=row ||
           m_api->read(row+e->offset,now,4) || m_resolve()!=row || memcmp(now,before,4))restored=0;
    }
    m_api->log(m_self,"MOVEMENT rollback: %s",restored?"verified":"INCOMPLETE; foreign or retired records preserved");
}
static void m_ready(HfMod *mod,HfEvent event,void *user)
{
    (void)mod;(void)user;
    if(event!=HF_EVENT_ENTITIES_READY)return;
    m_result=0;
    uintptr_t row=m_resolve();
    if(!row || !m_read(row,m_snapshot)) {m_refuse(m_lookup_reason?m_lookup_reason:"AI row read failed");return;}
    memcpy(m_expected,movement_baseline,M_SIZE);m_after(m_expected);
    if(m_owned==row && !memcmp(m_snapshot,m_expected,M_SIZE)) {
        m_result=2;m_api->log(m_self,"MOVEMENT ALREADY_APPLIED: owned Alpha Warrior record verified");return;
    }
    if(memcmp(m_snapshot,movement_baseline,M_SIZE)) {m_refuse("Alpha Warrior baseline changed or another mod owns these values");return;}
    memcpy(m_expected,m_snapshot,M_SIZE);
    for(unsigned i=0;i<M_COUNT;++i) {
        const MovementEdit *e=&movement_edits[i];
        if(!m_read(row,m_current) || memcmp(m_current,m_expected,M_SIZE)) {
            m_rollback(row,i);m_refuse("record changed before commit");return;
        }
        int result=m_api->write(row+e->offset,&e->after,4);
        memcpy(m_expected+e->offset,&e->after,4);
        if(result || !m_read(row,m_current) || memcmp(m_current,m_expected,M_SIZE)) {
            m_rollback(row,i+1);m_refuse("write or readback failed");return;
        }
    }
    m_owned=row;m_result=1;
    m_api->log(m_self,"MOVEMENT APPLIED: Alpha Warrior movement -20%%; guard/patrol/investigate 2.5 -> 2.0; combat/strafe 6.0 -> 4.8; 5 edits verified");
}
int enemy_movement_init(const HfApi *hf,HfMod *mod)
{
    if(!hf || hf->version<11 || hf->size<offsetof(HfApi,component_record)+sizeof hf->component_record ||
       !hf->log || !hf->build_id || !hf->on || !hf->read || !hf->write || !hf->module_base)return 1;
    m_api=hf;m_self=mod;
    const char *build=hf->build_id();
    if(!build || strcmp(build,"25480438")) {m_refuse("supported game build is 25480438");return 1;}
    hf->log(mod,"INIT: Enemy Rebalance 0.7.0 Alpha Warrior movement; waiting for entity data");
    return hf->on(mod,HF_EVENT_ENTITIES_READY,m_ready,NULL);
}
