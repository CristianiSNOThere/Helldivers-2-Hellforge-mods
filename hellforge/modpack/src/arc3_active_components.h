#ifndef HF_ARC3_ACTIVE_COMPONENTS_H
#define HF_ARC3_ACTIVE_COMPONENTS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define HF_ARC_ENTITY UINT64_C(0x96DE9CD50F7306E6)
typedef struct HfArcActiveIo {
 void* context;
 int (*read)(void*,uintptr_t,void*,size_t);
 uintptr_t (*record)(void*,const char*,uint64_t,uint32_t*);
} HfArcActiveIo;
typedef struct HfArcActiveJoin {uintptr_t owner,weapon,charge,weapon_table,charge_table;} HfArcActiveJoin;
typedef struct HfArcActiveSpec {const char* name;uint32_t component,slots,stride,index;uint8_t header[28];} HfArcActiveSpec;
static const HfArcActiveSpec HF_ARC_ACTIVE_COMPONENTS[2]={
 {"WeaponDataComponentData",236,730,1232,327,{0xb1,0xdb,0xe4,0x88,0x4c,0x44,0x4c,0x44,1,0,0,0,0xb1,0xdb,0xe4,0x88,0,0x0f,7,0,1,0,0,0,0,0,0,0}},
 {"WeaponChargeComponentData",204,20,216,6,{0xa1,0x35,0xc3,0xea,0x4c,0x44,0x4c,0x44,1,0,0,0,0xa1,0x35,0xc3,0xea,0x88,0x0a,0,0,1,0,0,0,0,0,0,0}}
};
static int hf_arc_active_read(const HfArcActiveIo* io,uintptr_t at,void* out,size_t n) {
 return io && io->read && at>=65536 && at<UINT64_C(0x800000000000) && n<=UINT64_C(0x800000000000)-at && !io->read(io->context,at,out,n);
}
static uintptr_t hf_arc_active_offset(uintptr_t base,size_t offset) {
 return base>=65536 && base<UINT64_C(0x800000000000) && offset<UINT64_C(0x800000000000)-base ? base+offset : 0;
}
static int hf_arc_active_join(const HfArcActiveIo* io,uintptr_t game,HfArcActiveJoin* out) {
 HfArcActiveJoin joined={0};uintptr_t again;uint8_t entries[2][730*16],snapshot[730*16];
 if(!io || !io->record || !out || !hf_arc_active_read(io,hf_arc_active_offset(game,0x346bf98),&joined.owner,8) || !joined.owner)return 0;
 for(unsigned c=0;c<2;c++) {
  const HfArcActiveSpec* s=&HF_ARC_ACTIVE_COMPONENTS[c];uint8_t head[32];uintptr_t table,row;uint32_t size=0,type=0;unsigned hits=0,aliases=0;
  if(!hf_arc_active_read(io,hf_arc_active_offset(joined.owner,0xf12478+s->component*8),&table,8) || table<65568 ||
     !hf_arc_active_read(io,table-32,head,32))return 0;
  memcpy(&type,head,4);
  if(type!=s->component || memcmp(head+4,s->header,28) || !hf_arc_active_read(io,table,entries[c],s->slots*16))return 0;
  for(unsigned i=0;i<s->slots;i++) {
   uint64_t id;uint32_t index,flags;memcpy(&id,entries[c]+i*16,8);memcpy(&index,entries[c]+i*16+8,4);memcpy(&flags,entries[c]+i*16+12,4);
   if(id==HF_ARC_ENTITY){if(index!=s->index || flags)return 0;hits++;}
   if(id && index==s->index)aliases++;
  }
  row=hf_arc_active_offset(table,s->slots*16+s->index*s->stride);
  if(hits!=1 || aliases!=1 || io->record(io->context,s->name,HF_ARC_ENTITY,&size)!=row || size!=s->stride ||
     !hf_arc_active_read(io,hf_arc_active_offset(joined.owner,0xf12478+s->component*8),&again,8) || again!=table)return 0;
  if(c==0){joined.weapon=row;joined.weapon_table=table;}else{joined.charge=row;joined.charge_table=table;}
 }
 for(unsigned c=0;c<2;c++) {
  const HfArcActiveSpec* s=&HF_ARC_ACTIVE_COMPONENTS[c];uintptr_t table=c?joined.charge_table:joined.weapon_table;
  if(!hf_arc_active_read(io,hf_arc_active_offset(joined.owner,0xf12478+s->component*8),&again,8) || again!=table ||
     !hf_arc_active_read(io,table,snapshot,s->slots*16) || memcmp(entries[c],snapshot,s->slots*16))return 0;
 }
 if(!hf_arc_active_read(io,hf_arc_active_offset(game,0x346bf98),&again,8) || again!=joined.owner)return 0;
 *out=joined;return 1;
}
#endif
