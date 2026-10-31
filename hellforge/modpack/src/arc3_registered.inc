#include "arc_registry.h"
typedef struct HfArcRegistered {const HfApi* api;HfMod* mod;uint32_t damage;uintptr_t damage_row;HfaPlan registry;int poisoned;} HfArcRegistered;
static HfArcRegistered HF_ARC_REGISTERED;
static int hf_arc_private_read(void* context,uintptr_t at,void* out,size_t size){return ((const HfApi*)context)->read(at,out,size);}
static int hf_arc_private_write(void* context,uintptr_t at,const void* bytes,size_t size){return ((const HfApi*)context)->write(at,bytes,size);}
static int hf_arc_private_register(const HfApi* api,HfMod* mod) {
 HfDamage d={0};
 if(!api || !api->add_damage || !api->damage_row || api->size<offsetof(HfApi,content)+sizeof(api->content) || !api->content)return 0;
 HF_ARC_REGISTERED.api=api;HF_ARC_REGISTERED.mod=mod;
 d.size=sizeof(d);d.base=547;d.name="superearth.arsenal-native-port/arc-3/supercharge-impact";
 HF_ARC_REGISTERED.damage=api->add_damage(mod,&d);
 if(HF_ARC_REGISTERED.damage<650 || HF_ARC_REGISTERED.damage>=714)return 0;
 if(!HF_REGISTRY_DAMAGE_WITNESS_ID)HF_REGISTRY_DAMAGE_WITNESS_ID=HF_ARC_REGISTERED.damage;
 return 1;
}
static int hf_arc_private_damage_owned(uintptr_t* output) {
 HfArcRegistered* s=&HF_ARC_REGISTERED;const HfApi* api=s->api;size_t size=0;uintptr_t root=0,pointer=0,game=0,row=0;
 uint64_t slots[714];unsigned owners=0;int ended=0;
 if(!api || s->poisoned || s->damage<650 || s->damage>=714)return 0;
 game=api->module_base("game.dll",&size);row=api->damage_row(s->damage);
 if(!game || !row || !hf_registry_root(api,game,size,HF_REGISTRY_DAMAGE,&root) ||
    api->read(root,slots,sizeof(slots)) || slots[s->damage]!=row)return 0;
 for(unsigned i=0;i<714;i++)if(i!=s->damage && slots[i]==row)return 0;
 for(unsigned i=0;i<4096;i++) {
  HfContent c={0};c.size=sizeof(c);if(!api->content(s->mod,i,&c)){ended=1;break;}
  if(c.kind==HF_KIND_DAMAGE && c.id==s->damage) {
   if(!c.mod || strcmp(c.mod,"superearth.arsenal-native-port") || !c.name ||
      strcmp(c.name,"superearth.arsenal-native-port/arc-3/supercharge-impact") || c.base!=547)return 0;
   owners++;
  }
 }
 if(!ended || owners!=1)return 0;
 if(api->read(game+0x37c60c0u+547*8,&pointer,8) || slots[547]!=pointer)return 0;
 *output=row;return 1;
}
static uintptr_t hf_arc_private_allocate(uintptr_t game,size_t size) {
#ifdef HF_ARC_PRIVATE_TEST
 extern uintptr_t hf_arc_test_allocate(uintptr_t,size_t);
 return hf_arc_test_allocate(game,size);
#else
 uintptr_t at=(game+size+65535u)&~(uintptr_t)65535u,end=game+INT32_MAX-65536u;
 while(at<end) {
  MEMORY_BASIC_INFORMATION region;if(!VirtualQuery((void*)at,&region,sizeof(region)))return 0;
  if(region.State==MEM_FREE) {uintptr_t p=(uintptr_t)VirtualAlloc((void*)at,sizeof(HfaBlock),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(p)return p;}
  uintptr_t next=((uintptr_t)region.BaseAddress+region.RegionSize+65535u)&~(uintptr_t)65535u;if(next<=at)return 0;at=next;
 }return 0;
#endif
}
static int hf_arc_private_owned(void) {
 HfArcRegistered* s=&HF_ARC_REGISTERED;uintptr_t row=0;uint8_t bytes[76],expected[76];
 HfaIo io={(void*)s->api,hf_arc_private_read,hf_arc_private_write};
 if(!hf_arc_private_damage_owned(&row) || row!=s->damage_row || !hfa_owned(&io,&s->registry) || s->api->read(row,bytes,76))return 0;
 memcpy(expected,HF_ARC3_SUPER_DAMAGE547,76);memcpy(expected,&s->damage,4);
 return !memcmp(bytes,expected,76);
}
static int hf_arc_private_bind(uintptr_t stock_damage,uintptr_t stock_arc,uint32_t* arc_id) {
 HfArcRegistered* s=&HF_ARC_REGISTERED;const HfApi* api=s->api;uintptr_t row=0,game=0,pointer=0;size_t size=0;
 uint8_t before[76],after[76],actual[76],arc[104];HfaIo io={(void*)api,hf_arc_private_read,hf_arc_private_write};
 if(!arc_id || !hf_arc_private_damage_owned(&row))return 0;
 game=api->module_base("game.dll",&size);
 /* Validate actual donor lookup ownership, as well as serialized bytes. */
 if(api->read(game+0x37c60c0u+547*8,&pointer,8) || pointer!=stock_damage ||
    api->read(stock_damage,actual,76) || memcmp(actual,HF_ARC3_EXPECT_DAMAGE547,76) ||
    api->read(stock_arc,arc,104) || memcmp(arc,HF_ARC3_EXPECT_ARC11,104))return 0;
 if(s->registry.state==2) {if(!hf_arc_private_owned())return 0;*arc_id=17;return 1;}
 if(s->registry.state || api->read(game+HFA_TABLE_RVA+11*8,&pointer,8) || pointer!=stock_arc)return 0;
 memcpy(before,HF_ARC3_EXPECT_DAMAGE547,76);memcpy(before,&s->damage,4);
 memcpy(after,HF_ARC3_SUPER_DAMAGE547,76);memcpy(after,&s->damage,4);
 if(api->read(row,actual,76) || memcmp(actual,before,76))return 0;
 uintptr_t allocation=hf_arc_private_allocate(game,size);
 if(!allocation || !hfa_prepare(&io,&s->registry,game,size,allocation,s->damage,HF_ARC3_EXPECT_ARC11,HF_ARC3_SUPER_ARC11))return 0;
 s->damage_row=row;
 if(!hf_arc_private_damage_owned(&pointer) || pointer!=row || api->write(row,after,76) ||
    api->read(row,actual,76) || memcmp(actual,after,76) || !hfa_commit(&io,&s->registry)) {
  /* Never restore a row after registry ownership is lost or foreign bytes
   * appear. Partial writes belonging entirely to this attempt may be undone. */
  if(hf_arc_private_damage_owned(&pointer) && pointer==row && !api->read(row,actual,76)) {
   unsigned i;for(i=0;i<76;i++)if(actual[i]!=before[i] && actual[i]!=after[i])break;
   if(i==76 && !api->write(row,before,76) && !api->read(row,actual,76) && !memcmp(actual,before,76) && s->registry.state>=0)return 0;
  }s->poisoned=1;return 0;
 }
 if(!hf_arc_private_owned()){s->poisoned=1;return 0;}
 *arc_id=17;return 1;
}
