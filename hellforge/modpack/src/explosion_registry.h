#ifndef HFX_EXPLOSION_REGISTRY_H
#define HFX_EXPLOSION_REGISTRY_H
/* Private, uninstalled prototype. All mutation goes through caller-owned IO.
 * Install only on the game thread, before either private projectile is linked.
 * No SDK catalogue extension or stock-row reservation is implied. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#define HFX_STOCK_SLOTS 423u
#define HFX_SLOTS 425u
#define HFX_TABLE_RVA 0x37CC920u
#define HFX_ROW_SIZE 152u

typedef struct HfxPin {
    uint32_t rva;
    uint8_t size,operand,kind;
    uint32_t value;
    uint8_t bytes[16];
} HfxPin;
#include "explosion_pins.generated.h"
typedef struct HfxIo {
    void* context;
    int (*read)(void*,uintptr_t,void*,size_t);
    int (*write)(void*,uintptr_t,const void*,size_t);
    int (*hash)(void*,const void*,size_t,const char*);
} HfxIo;
typedef struct HfxBlock {
    uint64_t slots[HFX_SLOTS];
    uint8_t rows[2][HFX_ROW_SIZE];
    uint64_t effects[2];
} HfxBlock;
typedef struct HfxPlan {
    uintptr_t game,allocation;
    size_t image_size;
    HfxBlock desired;
    uint8_t sources[2][HFX_ROW_SIZE];
    uint8_t after[HFX_PATCH_COUNT][16];
    uint32_t attempted;
    int state; /* 0 unused, 1 prepared, 2 installed, -1 quarantined */
} HfxPlan;

static int hfx_address(uintptr_t at,size_t size) {
    return at>=65536u && at<UINT64_C(0x800000000000) &&
        size<=UINT64_C(0x800000000000)-at;
}
static int hfx_read(const HfxIo* io,uintptr_t at,void* out,size_t size) {
    return io && io->read && out && size && hfx_address(at,size) &&
        io->read(io->context,at,out,size)==0;
}
static inline uint32_t hfx_u32(const void* in) { uint32_t x;memcpy(&x,in,4);return x; }
static void hfx_put32(void* out,uint32_t x) { memcpy(out,&x,4); }
static void hfx_putf(void* out,float x) { memcpy(out,&x,4); }

static int hfx_pin_read(const HfxIo* io,const HfxPlan* p,const HfxPin* pin,uint8_t out[16]) {
    return pin->rva<p->image_size && pin->size<=p->image_size-pin->rva &&
        hfx_read(io,p->game+pin->rva,out,pin->size);
}
static int hfx_codes_match(const HfxIo* io,const HfxPlan* p,int applied) {
    uint32_t i;uint8_t bytes[16];
    for(i=0;i<HFX_PATCH_COUNT;i++)
        if(!hfx_pin_read(io,p,&HFX_PINS[i],bytes) ||
           memcmp(bytes,applied?p->after[i]:HFX_PINS[i].bytes,HFX_PINS[i].size))return 0;
    for(i=0;i<HFX_CLEAR_COUNT;i++)
        if(!hfx_pin_read(io,p,&HFX_CLEAR_PINS[i],bytes) ||
           memcmp(bytes,HFX_CLEAR_PINS[i].bytes,HFX_CLEAR_PINS[i].size))return 0;
    return 1;
}

static int hfx_stock_slots(const HfxIo* io,uintptr_t table,uint64_t slots[HFX_STOCK_SLOTS]) {
    uint32_t i,id;
    if(!hfx_read(io,table,slots,HFX_STOCK_SLOTS*8u) || slots[0])return 0;
    for(i=1;i<HFX_STOCK_SLOTS;i++)
        if((slots[i]&3u) || !hfx_read(io,(uintptr_t)slots[i],&id,4) || id!=i)return 0;
    return 1;
}

static int hfx_prepare(const HfxIo* io,HfxPlan* p,uintptr_t game,size_t image_size,
                       uintptr_t allocation,uint32_t normal_damage,uint32_t pellet_damage) {
    static const uint32_t donors[2]={290u,355u};
    static const char* hashes[2]={
        "49165C22C8391678A903603E6DFC7AD8541B9157D681524B8C316B38E6DCE821",
        "515E555EA9EA45C57B21BB09AEC068C68E9EC6D89542A3D5BC4EAAD350A0E6E6"};
    uint8_t zero[sizeof(HfxBlock)]={0},actual[sizeof(HfxBlock)];
    uint32_t i;uint64_t array,count;int64_t operand;int32_t disp;
    if(!p || p->state || !io || !io->hash || !io->write || sizeof(uintptr_t)!=8u ||
       !hfx_address(game,image_size) || image_size<HFX_TABLE_RVA+HFX_STOCK_SLOTS*8u ||
       !hfx_address(allocation,sizeof(HfxBlock)) || (allocation&7u) ||
       !(allocation+sizeof(HfxBlock)<=game || allocation>=game+image_size) ||
       normal_damage<650u || normal_damage>713u || pellet_damage<650u ||
       pellet_damage>713u || normal_damage==pellet_damage)return 0;
    memset(p,0,sizeof(*p));p->game=game;p->allocation=allocation;p->image_size=image_size;
    if(!hfx_codes_match(io,p,0) || !hfx_read(io,allocation,actual,sizeof(actual)) ||
       memcmp(actual,zero,sizeof(zero)) ||
       !hfx_stock_slots(io,game+HFX_TABLE_RVA,p->desired.slots))return 0;
    for(i=0;i<2;i++) {
        uint8_t stable[HFX_ROW_SIZE];uint8_t* row=p->desired.rows[i];
        if(!hfx_read(io,(uintptr_t)p->desired.slots[donors[i]],row,HFX_ROW_SIZE))return 0;
        memcpy(p->sources[i],row,HFX_ROW_SIZE);
        memcpy(&array,row+40,8);memcpy(&count,row+48,8);
        /* These donors have one uint64 resource identity, not a pointer tree. */
        if(count!=1u || (array&3u) || !hfx_read(io,(uintptr_t)array,&p->desired.effects[i],8) ||
           !io->hash(io->context,&p->desired.effects[i],8,
             "0371C21FFDD677605BF449D0A3BC560F60492D36A2703BE3B629DAD2EE72B57D"))return 0;
        memcpy(stable,row,sizeof(stable));memset(stable+40,0,8);
        if(!io->hash(io->context,stable,sizeof(stable),hashes[i]))return 0;
        hfx_put32(row,HFX_STOCK_SLOTS+i);
        hfx_put32(row+4,i?pellet_damage:normal_damage);
        hfx_putf(row+16,i?0.5f:3.5f);hfx_putf(row+20,i?0.5f:6.5f);
        hfx_putf(row+24,i?0.5f:8.0f);
        array=allocation+offsetof(HfxBlock,effects)+i*8u;memcpy(row+40,&array,8);
        p->desired.slots[HFX_STOCK_SLOTS+i]=allocation+offsetof(HfxBlock,rows)+i*HFX_ROW_SIZE;
    }
    for(i=0;i<HFX_PATCH_COUNT;i++) {
        const HfxPin* pin=&HFX_PINS[i];memcpy(p->after[i],pin->bytes,pin->size);
        if(pin->kind==3u)operand=pin->value+2u;
        else if(pin->kind==1u)operand=(int64_t)allocation+pin->value-(int64_t)(game+pin->rva+pin->size);
        else operand=(int64_t)allocation+pin->value-(int64_t)game;
        if(operand<INT32_MIN || operand>INT32_MAX)return 0;
        disp=(int32_t)operand;memcpy(p->after[i]+pin->operand,&disp,4);
    }
    p->state=1;return 1;
}

/* Roll back comparison limits first, then table readers. A partial write is
 * recoverable only when every changed byte matches our before/after values.
 * Foreign bytes are never overwritten; the allocation remains quarantined. */
static int hfx_rollback(const HfxIo* io,HfxPlan* p) {
    int failed=0;uint32_t n=p->attempted;
    if(p->state==2) {
        uint64_t active[HFX_STOCK_SLOTS];
        /* A completed reload may have freed the old stock rows. Never redirect
         * readers back to their stale pre-installation pointer list. */
        if(!hfx_stock_slots(io,p->allocation,active) ||
           memcmp(active,p->desired.slots,sizeof(active))) {p->state=-1;return 0;}
    }
    while(n) {
        uint8_t actual[16];uint32_t j;const HfxPin* pin=&HFX_PINS[--n];
        if(!hfx_pin_read(io,p,pin,actual)){failed=1;continue;}
        for(j=0;j<pin->size;j++)
            if(actual[j]!=pin->bytes[j] && actual[j]!=p->after[n][j])break;
        if(j!=pin->size){failed=1;continue;}
        if(memcmp(actual,pin->bytes,pin->size)) {
            (void)io->write(io->context,p->game+pin->rva+pin->operand,pin->bytes+pin->operand,4);
            if(!hfx_pin_read(io,p,pin,actual) || memcmp(actual,pin->bytes,pin->size))failed=1;
        }
    }
    p->attempted=0;p->state=failed?-1:0;return !failed;
}

static int hfx_commit(const HfxIo* io,HfxPlan* p) {
    static const uint32_t donors[2]={290u,355u};
    uint64_t slots[HFX_STOCK_SLOTS];HfxBlock block;uint32_t i;uint8_t code[16];
    uint8_t zero[sizeof(HfxBlock)]={0},row[HFX_ROW_SIZE];uint64_t pointer,effect;
    if(!p || p->state!=1 || !hfx_codes_match(io,p,0) ||
       !hfx_stock_slots(io,p->game+HFX_TABLE_RVA,slots) ||
       memcmp(slots,p->desired.slots,sizeof(slots)))return 0;
    if(!hfx_read(io,p->allocation,&block,sizeof(block)) || memcmp(&block,zero,sizeof(block)))return 0;
    for(i=0;i<2;i++) {
        if(!hfx_read(io,(uintptr_t)slots[donors[i]],row,sizeof(row)) ||
           memcmp(row,p->sources[i],sizeof(row)))return 0;
        memcpy(&pointer,row+40,8);
        if(!hfx_read(io,(uintptr_t)pointer,&effect,8) || effect!=p->desired.effects[i])return 0;
    }
    /* Complete privately owned rows before exposing any redirected lookup. */
    if(io->write(io->context,p->allocation,&p->desired,sizeof(p->desired)) ||
       !hfx_read(io,p->allocation,&block,sizeof(block)) || memcmp(&block,&p->desired,sizeof(block)))return 0;
    for(i=0;i<HFX_PATCH_COUNT;i++) {
        const HfxPin* pin=&HFX_PINS[i];
        if(!hfx_pin_read(io,p,pin,code) || memcmp(code,pin->bytes,pin->size)) {
            (void)hfx_rollback(io,p);return 0;
        }
        p->attempted=i+1u;
        if(io->write(io->context,p->game+pin->rva+pin->operand,p->after[i]+pin->operand,4) ||
           !hfx_pin_read(io,p,pin,code) || memcmp(code,p->after[i],pin->size)) {
            (void)hfx_rollback(io,p);return 0;
        }
    }
    if(!hfx_codes_match(io,p,1)) {(void)hfx_rollback(io,p);return 0;}
    p->state=2;return 1;
}

static int hfx_owned(const HfxIo* io,const HfxPlan* p) {
    HfxBlock actual;uint64_t slots[HFX_STOCK_SLOTS];
    if(!p || p->state!=2 || !hfx_codes_match(io,p,1) ||
       !hfx_read(io,p->allocation,&actual,sizeof(actual)) ||
       !hfx_stock_slots(io,p->allocation,slots))return 0;
    /* Stock pointers can legitimately change on reload. Private objects cannot. */
    return !memcmp(&actual.slots[HFX_STOCK_SLOTS],&p->desired.slots[HFX_STOCK_SLOTS],
                   sizeof(actual)-HFX_STOCK_SLOTS*8u);
}
#endif
