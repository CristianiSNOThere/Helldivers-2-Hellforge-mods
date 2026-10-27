/* Enemy Rebalance health changes, build 25480438. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if defined(_WIN32) && !defined(ENEMY_BALANCE_TEST)
#include <windows.h>
#endif
#include "hellforge_mod.h"
#include "balance_data.h"

#define ROW_SIZE 22096u
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static const uint64_t resources[2] = {UINT64_C(0x9E2E17F2CCCCAFDD),UINT64_C(0xDCF8E74212FBEE3B)};
static const HfApi *api;
static HfMod *self;
static uintptr_t owned[2];
static int last_result;
static uint8_t snapshot[2][ROW_SIZE], expected[2][ROW_SIZE], current[2][ROW_SIZE];

static int data_region(uintptr_t at, size_t size)
{
    if (at < 0x10000 || at >= UINT64_C(0x800000000000) || !size || size > UINT64_C(0x800000000000)-at) return 0;
#if defined(_WIN32) && !defined(ENEMY_BALANCE_TEST)
    uintptr_t end = at + size;
    while (at < end) {
        MEMORY_BASIC_INFORMATION m;
        if (!VirtualQuery((const void*)at,&m,sizeof m) || m.State != MEM_COMMIT || m.Type != MEM_PRIVATE ||
            (m.Protect & (PAGE_GUARD|PAGE_NOACCESS)) ||
            !(m.Protect & (PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY))) return 0;
        uintptr_t next = (uintptr_t)m.BaseAddress + m.RegionSize;
        if (next <= at) return 0;
        at = next;
    }
#endif
    return 1;
}

/* SDK lookup refuses missing, duplicate and shared records. Rejoin it before
 * every store; no table allocation or record address is assumed to survive reload. */
static int resolve(uintptr_t rows[2])
{
    for (unsigned i=0;i<2;++i) {
        uint32_t stride=0;
        rows[i]=api->component_record("HealthComponentData",resources[i],&stride);
        if (!rows[i] || stride!=ROW_SIZE || !data_region(rows[i],ROW_SIZE)) return 0;
    }
    return rows[0]+ROW_SIZE <= rows[1] || rows[1]+ROW_SIZE <= rows[0];
}
static int same_owner(const uintptr_t rows[2])
{
    uintptr_t found[2];
    return resolve(found) && found[0]==rows[0] && found[1]==rows[1];
}
static int read_rows(const uintptr_t rows[2], uint8_t out[2][ROW_SIZE])
{
    return api->read(rows[0],out[0],ROW_SIZE)==0 && api->read(rows[1],out[1],ROW_SIZE)==0;
}
static int guards_equal(const uint8_t a[2][ROW_SIZE],const uint8_t b[2][ROW_SIZE])
{
    for (unsigned t=0;t<2;++t) for (unsigned g=0;g<COUNT(guard_ranges);++g)
        if (memcmp(a[t]+guard_ranges[g][0],b[t]+guard_ranges[g][0],guard_ranges[g][1])) return 0;
    return 1;
}
static uint32_t word(const uint8_t *p) {uint32_t v;memcpy(&v,p,4);return v;}
static void set_after(uint8_t rows[2][ROW_SIZE])
{
    for(unsigned i=0;i<COUNT(edits);++i) memcpy(rows[edits[i].target]+edits[i].offset,&edits[i].after,4);
}
static void refused(const char *reason)
{
    last_result=-1;
    api->log(self,"REFUSED: %s; no further edits attempted",reason);
}

/* Roll back only bytes this attempt could have written, and only while the
 * exact record owner still exists. Foreign words are preserved and reported. */
static int rollback(const uintptr_t rows[2],unsigned attempted)
{
    int restored=1;
    while(attempted) {
        const BalanceEdit *e=&edits[--attempted];
        uint8_t now[4],before[4],after[4];
        memcpy(before,&e->before,4);memcpy(after,&e->after,4);
        if(!same_owner(rows) || api->read(rows[e->target]+e->offset,now,4)) {restored=0;continue;}
        int ours=1;
        for(unsigned k=0;k<4;++k) if(now[k]!=before[k] && now[k]!=after[k]) ours=0;
        if(!ours) {restored=0;api->log(self,"Rollback preserved foreign value: %s",e->label);continue;}
        if(!memcmp(now,before,4)) continue;
        if(api->write(rows[e->target]+e->offset,before,4) ||
           api->read(rows[e->target]+e->offset,now,4) || memcmp(now,before,4)) restored=0;
    }
    api->log(self,"Transaction rollback: %s",restored?"verified":"INCOMPLETE; foreign or retired records preserved");
    return restored;
}

static void ready(HfMod *mod,HfEvent event,void *user)
{
    (void)mod;(void)user;
    if(event!=HF_EVENT_ENTITIES_READY) return;
    uintptr_t rows[2];
    last_result=0;
    if(!resolve(rows) || !read_rows(rows,snapshot) || !same_owner(rows)) {refused("enemy record ownership, stride or memory bounds");return;}
    memcpy(expected,baseline,sizeof expected);set_after(expected);
    if(owned[0]==rows[0] && owned[1]==rows[1] && guards_equal(snapshot,expected)) {
        last_result=2;api->log(self,"ALREADY_APPLIED: owned enemy records verified");return;
    }
    if(!guards_equal(snapshot,baseline)) {
        for(unsigned i=0;i<COUNT(edits);++i) {
            const BalanceEdit *e=&edits[i];
            if(word(snapshot[e->target]+e->offset)!=e->before) api->log(self,"Unexpected value: %s",e->label);
        }
        refused("enemy baseline changed or another mod owns these values");return;
    }
    memcpy(expected,snapshot,sizeof expected);
    for(unsigned i=0;i<COUNT(edits);++i) {
        const BalanceEdit *e=&edits[i];
        if(!same_owner(rows) || !read_rows(rows,current) || !guards_equal(current,expected)) {
            rollback(rows,i);refused("record changed before commit");return;
        }
        int result=api->write(rows[e->target]+e->offset,&e->after,4);
        memcpy(expected[e->target]+e->offset,&e->after,4);
        if(result || !same_owner(rows) || !read_rows(rows,current) || !guards_equal(current,expected)) {
            api->log(self,"Failed edit: %s",e->label);
            rollback(rows,i+1);refused("write or readback failed");return;
        }
    }
    owned[0]=rows[0];owned[1]=rows[1];last_result=1;
    api->log(self,"APPLIED: Titan explosion multiplier 0.5 -> 1.0 (26 fields); Impaler head 1250 -> 900 HP; four fatal leg-flesh zones 1000 -> 500 HP; 31 edits verified");
}

int enemy_health_init(const HfApi *hf,HfMod *mod)
{
    if(!hf || hf->version<11 || hf->size < offsetof(HfApi,component_record)+sizeof hf->component_record ||
       !hf->log || !hf->build_id || !hf->on || !hf->read || !hf->write || !hf->component_record) return 1;
    api=hf;self=mod;
    const char *build=api->build_id();
    if(!build || strcmp(build,"25480438")) {refused("supported game build is 25480438");return 1;}
    api->log(self,"INIT: Enemy Rebalance 0.7.0 health; waiting for entity data");
    return api->on(mod,HF_EVENT_ENTITIES_READY,ready,NULL);
}
