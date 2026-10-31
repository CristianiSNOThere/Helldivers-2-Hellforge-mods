#ifndef HF_ARC_PRIVATE_REGISTRY_H
#define HF_ARC_PRIVATE_REGISTRY_H
/* Mod-owned Arc ID 17 extends the actual native lookup. No stock Arc or
 * damage slot is reserved. All pointers and consumers are pinned. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#define HFA_TABLE_RVA 0x37C5F90u
#define HFA_STOCK_SLOTS 17u
#define HFA_PRIVATE_ID 17u
typedef struct HfaPin {uint32_t rva;uint8_t size,operand,kind;uint32_t value;uint8_t bytes[16];} HfaPin;
#include "arc_registry_pins.generated.h"
typedef struct HfaIo {void* context;int(*read)(void*,uintptr_t,void*,size_t);int(*write)(void*,uintptr_t,const void*,size_t);} HfaIo;
typedef struct HfaBlock {uint64_t slots[18];uint8_t row[104];} HfaBlock;
typedef struct HfaPlan {
 uintptr_t game,allocation;size_t image_size;HfaBlock desired;
 uint8_t source[104],after[HFA_PIN_COUNT][16];uint32_t attempted;int state;
} HfaPlan;
static int hfa_read(const HfaIo* io,uintptr_t at,void* out,size_t n) {
 return io && io->read && at>=65536 && at<UINT64_C(0x800000000000) && n<=UINT64_C(0x800000000000)-at && !io->read(io->context,at,out,n);
}
static int hfa_codes(const HfaIo* io,const HfaPlan* p,int after) {
 uint8_t bytes[16];
 for(unsigned i=0;i<HFA_PIN_COUNT;i++) {
  const HfaPin* pin=&HFA_PINS[i];
  if(pin->rva>p->image_size || pin->size>p->image_size-pin->rva ||
     !hfa_read(io,p->game+pin->rva,bytes,pin->size) || memcmp(bytes,after?p->after[i]:pin->bytes,pin->size))return 0;
 }return 1;
}
static int hfa_slots(const HfaIo* io,uintptr_t at,uint64_t* slots) {
 if(!hfa_read(io,at,slots,17*8) || slots[0])return 0;
 for(unsigned i=1;i<17;i++) {uint32_t id;if((slots[i]&3) || !hfa_read(io,slots[i],&id,4) || id!=i)return 0;}
 return 1;
}
static int hfa_prepare(const HfaIo* io,HfaPlan* p,uintptr_t game,size_t image_size,uintptr_t allocation,
                       uint32_t damage,const uint8_t source[104],const uint8_t target[104]) {
 HfaBlock zero={0},actual;uint32_t id=17;
 if(!io || !io->write || !p || p->state || !allocation || allocation&7 ||
    damage<650 || damage>=714 || image_size<HFA_TABLE_RVA+17*8 ||
    allocation<game+image_size || allocation-game>INT32_MAX)return 0;
 memset(p,0,sizeof(*p));p->game=game;p->image_size=image_size;p->allocation=allocation;
 if(!hfa_codes(io,p,0) || !hfa_read(io,allocation,&actual,sizeof(actual)) || memcmp(&actual,&zero,sizeof(actual)) ||
    !hfa_slots(io,game+HFA_TABLE_RVA,p->desired.slots) ||
    !hfa_read(io,p->desired.slots[11],p->source,104) || memcmp(p->source,source,104))return 0;
 memcpy(p->desired.row,target,104);memcpy(p->desired.row,&id,4);memcpy(p->desired.row+36,&damage,4);
 p->desired.slots[17]=allocation+offsetof(HfaBlock,row);
 for(unsigned i=0;i<HFA_PIN_COUNT;i++) {
  const HfaPin* pin=&HFA_PINS[i];int64_t d=pin->kind==1?(int64_t)(allocation+pin->value)-(int64_t)(game+pin->rva+pin->size):(int64_t)(allocation+pin->value)-(int64_t)game;
  if(d<INT32_MIN || d>INT32_MAX)return 0;int32_t disp=(int32_t)d;
  memcpy(p->after[i],pin->bytes,pin->size);memcpy(p->after[i]+pin->operand,&disp,4);
 }p->state=1;return 1;
}
static int hfa_rollback(const HfaIo* io,HfaPlan* p) {
 int failed=0;uint8_t actual[16];unsigned n=p->attempted;
 while(n) {
  unsigned i=--n;const HfaPin* pin=&HFA_PINS[i];
  if(!hfa_read(io,p->game+pin->rva,actual,pin->size)){failed=1;continue;}
  unsigned j;for(j=0;j<pin->size;j++)if(actual[j]!=pin->bytes[j] && actual[j]!=p->after[i][j])break;
  if(j!=pin->size){failed=1;continue;}
  if(memcmp(actual,pin->bytes,pin->size)) {
   (void)io->write(io->context,p->game+pin->rva+pin->operand,pin->bytes+pin->operand,4);
   if(!hfa_read(io,p->game+pin->rva,actual,pin->size) || memcmp(actual,pin->bytes,pin->size))failed=1;
  }
 }p->state=failed?-1:0;p->attempted=0;return !failed;
}
static int hfa_commit(const HfaIo* io,HfaPlan* p) {
 uint64_t slots[17];uint8_t row[104],code[16];HfaBlock actual,zero={0};
 if(p->state!=1 || !hfa_codes(io,p,0) || !hfa_slots(io,p->game+HFA_TABLE_RVA,slots) ||
    memcmp(slots,p->desired.slots,sizeof(slots)) || !hfa_read(io,slots[11],row,104) || memcmp(row,p->source,104) ||
    !hfa_read(io,p->allocation,&actual,sizeof(actual)) || memcmp(&actual,&zero,sizeof(actual)))return 0;
 if(io->write(io->context,p->allocation,&p->desired,sizeof(p->desired)) ||
    !hfa_read(io,p->allocation,&actual,sizeof(actual)) || memcmp(&actual,&p->desired,sizeof(actual)))return 0;
 for(unsigned i=0;i<HFA_PIN_COUNT;i++) {
  const HfaPin* pin=&HFA_PINS[i];
  if(!hfa_read(io,p->game+pin->rva,code,pin->size) || memcmp(code,pin->bytes,pin->size)){hfa_rollback(io,p);return 0;}
  p->attempted=i+1;
  if(io->write(io->context,p->game+pin->rva+pin->operand,p->after[i]+pin->operand,4) ||
     !hfa_read(io,p->game+pin->rva,code,pin->size) || memcmp(code,p->after[i],pin->size)){hfa_rollback(io,p);return 0;}
 }
 if(!hfa_codes(io,p,1)){hfa_rollback(io,p);return 0;}
 p->state=2;return 1;
}
static int hfa_owned(const HfaIo* io,const HfaPlan* p) {
 HfaBlock actual;
 return p->state==2 && hfa_codes(io,p,1) && hfa_read(io,p->allocation,&actual,sizeof(actual)) &&
   actual.slots[17]==p->desired.slots[17] && !memcmp(actual.row,p->desired.row,104);
}
#endif
