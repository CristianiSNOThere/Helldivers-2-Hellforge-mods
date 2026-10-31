#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "hellforge_mod.h"
#include "../pack_config.h"
#if HF_PORT_CQC_20_BREACHING_HAMMER
#include "transaction.hpp"
#include "wield_repair.hpp"
#include "combat_policy.hpp"
#include "flinch_pins.hpp"
#include "enemies.hpp"
#include "../hammer_combat_guard.generated.h"
#include <algorithm>
#include <array>
extern "C" int hf_primary_combat_bind(void);
extern "C" uint32_t hf_primary_strike_damage(void);
extern "C" uint32_t hf_primary_blast_type(void);
namespace {
using namespace hammer;
const HfApi* api;HfMod* mod;Io io;uintptr_t game,exe;uint64_t primary;
bool hooks,ready,stopped;float elapsed,poll,metrics;
using ZoneFn=uint8_t(*)(uintptr_t,uint32_t,const uint8_t*);
using ModifierFn=uintptr_t(*)(uint32_t,uint32_t,uint32_t);
using ExplosionFn=uint64_t(*)(uintptr_t,const float*,uint32_t,uint32_t,uint32_t,uint64_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
ZoneFn next_zone;ModifierFn next_modifier;ExplosionFn next_explosion;
std::array<std::array<uint8_t,248>,2> volumes{};
uint32_t volume_damage;uint64_t strike_routes,blast_routes,flinch_routes,stun_requests,stun_refusals;
std::array<uint64_t,5> reported{};
alignas(16) const uint32_t flinch_token[4]={0x73734D67u,0,0,0};
struct Pulse {float position[3];uintptr_t factory,health;uint32_t cursor;uint64_t since;};
std::array<Pulse,16> pulses{};unsigned head,tail,count;
bool actor_primary(uint32_t actor) {
 try {
  uintptr_t factory=io.value<uintptr_t>(game+0x346bf98);auto identity=WieldRepair::descriptor(io,factory,actor);
  if(get<uint64_t>(identity,0)!=hammer_combat::AVATAR || (get<uint32_t>(identity,20)&3)!=1)return false;
  auto health=io.value<uintptr_t>(game+0x3326688);auto hi=WieldRepair::lookup(io,health+0x1030,actor);
  if(hi==UINT32_MAX || hi>=io.value<uint32_t>(health+0x1020))return false;
  auto row=io.value<uintptr_t>(health+0x1058)+size_t(hi)*440;auto hp=io.value<int32_t>(row+20);auto life=io.value<uint32_t>(row+412);
  auto wielder=io.value<uintptr_t>(game+0x3326420);auto wi=WieldRepair::lookup(io,wielder+0x30,actor);
  auto inventory=io.value<uintptr_t>(game+0x3326738);auto ii=WieldRepair::lookup(io,inventory+0x28,actor);
  if(wi==UINT32_MAX || ii==UINT32_MAX || wi>=65536 || ii>=65536)return false;
  auto id=io.value<uint32_t>(io.value<uintptr_t>(wielder+0x60)+size_t(wi)*0x1d0);auto held=WieldRepair::descriptor(io,factory,id);
  auto selection=io.value<uint32_t>(io.value<uintptr_t>(inventory+0x50)+size_t(ii)*48+28);
  auto again=WieldRepair::descriptor(io,factory,actor);
  return identity==again && primary==hammer_combat::PRIMARY && hammer_combat::primary_actor(get<uint64_t>(identity,0),life,hp,get<uint64_t>(held,0),selection) &&
   io.value<uint32_t>(io.value<uintptr_t>(wielder+0x60)+size_t(wi)*0x1d0)==id;
 }catch(...){return false;}
}
uint8_t zone_hook(uintptr_t context,uint32_t actor,const uint8_t* volume) {
 const uint8_t* chosen=volume;
 if(ready&&!stopped&&actor_primary(actor)) {
  unsigned index=volume==(const uint8_t*)(game+0x3487e90)?0:volume==(const uint8_t*)(game+0x3489990)?1:2;
  if(index<2 && hf_primary_strike_damage()==volume_damage) {
   const uint8_t* expected=index?HF_HAMMER_VOLUME_1:HF_HAMMER_VOLUME_0;uint8_t bytes[248];
   if(!api->read((uintptr_t)volume,bytes,sizeof(bytes))&&!memcmp(bytes,expected,sizeof(bytes))){chosen=volumes[index].data();strike_routes++;}
  }
 }return next_zone(context,actor,chosen);
}
uintptr_t modifier_result(uint32_t actor,uint32_t key,uint32_t flags,uintptr_t original,uintptr_t caller) {
 if(!original && ready&&hooks&&!stopped && key==0x73734D67u && flags==1 && actor_primary(actor)) {
  for(const auto& pin:HAMMER_FLINCH_PINS)if(caller==game+pin.call+5 && api->code_matches(game+pin.start,pin.pattern)){flinch_routes++;return (uintptr_t)flinch_token;}
 }return original;
}
/* The pinned native getter uses only integer registers. Its game callers
 * retain live floating-point calculations across the call. Preserve all six
 * volatile SSE registers, beyond the generic Windows ABI, on every path. */
extern "C" __attribute__((used,noinline)) uintptr_t hf_primary_modifier_dispatch(uint32_t actor,uint32_t key,uint32_t flags,uintptr_t caller) {
 return modifier_result(actor,key,flags,next_modifier(actor,key,flags),caller);
}
extern "C" __attribute__((naked)) uintptr_t hf_primary_modifier_entry(uint32_t,uint32_t,uint32_t) {
 __asm__ volatile(
  ".seh_proc hf_primary_modifier_entry\n"
  "subq $136, %rsp\n"
  ".seh_stackalloc 136\n"
  ".seh_endprologue\n"
  "movdqu %xmm0, 32(%rsp)\n"
  "movdqu %xmm1, 48(%rsp)\n"
  "movdqu %xmm2, 64(%rsp)\n"
  "movdqu %xmm3, 80(%rsp)\n"
  "movdqu %xmm4, 96(%rsp)\n"
  "movdqu %xmm5, 112(%rsp)\n"
  "movq 136(%rsp), %r9\n"
  "callq hf_primary_modifier_dispatch\n"
  "movdqu 32(%rsp), %xmm0\n"
  "movdqu 48(%rsp), %xmm1\n"
  "movdqu 64(%rsp), %xmm2\n"
  "movdqu 80(%rsp), %xmm3\n"
  "movdqu 96(%rsp), %xmm4\n"
  "movdqu 112(%rsp), %xmm5\n"
  "addq $136, %rsp\n"
  "retq\n"
  ".seh_endproc\n");
}
void queue_stun(const float* position) {
 if(count==pulses.size())return;
 try {
  Pulse pulse{};memcpy(pulse.position,position,12);if(!hammer_combat::in_stun_area(position,position))return;
  pulse.factory=io.value<uintptr_t>(game+0x346bf98);pulse.health=io.value<uintptr_t>(game+0x3326688);pulse.since=GetTickCount64();
  pulses[tail]=pulse;tail=(tail+1)%pulses.size();count++;
 }catch(...){}
}
uint64_t explosion_hook(uintptr_t queue,const float* position,uint32_t type,uint32_t source,uint32_t owner,uint64_t creditor,uintptr_t a7,uintptr_t a8,uintptr_t a9,uintptr_t a10,uintptr_t a11,uintptr_t a12,uintptr_t a13,uintptr_t a14,uintptr_t a15) {
 uint32_t routed=type,before=UINT32_MAX;float point[3]{};
 if(ready&&!stopped && type==19 && source==owner && actor_primary(source)) {
  try {
   if(queue==io.value<uintptr_t>(game+0x346d558) && !api->read((uintptr_t)position,point,12) && hammer_combat::in_stun_area(point,point)) {
    before=io.value<uint32_t>(queue+32);auto id=hf_primary_blast_type();if(before<256 && id==425)routed=id;
   }
  }catch(...){}
 }
 auto result=next_explosion(queue,position,routed,source,owner,creditor,a7,a8,a9,a10,a11,a12,a13,a14,a15);
 if(routed!=type)try {
  auto row=queue+40+size_t(before)*152;
  if(io.value<uint32_t>(queue+32)==before+1 && io.value<uint32_t>(row+12)==425 && io.value<uint32_t>(row+16)==source && io.value<uint32_t>(row+20)==owner) {
   blast_routes++;queue_stun(point);
  }
 }catch(...){}
 return result;
}
bool enemy(uint64_t resource) {return std::find(std::begin(HF_HAMMER_ENEMIES),std::end(HF_HAMMER_ENEMIES),resource)!=std::end(HF_HAMMER_ENEMIES) && resource!=hammer_combat::AVATAR;}
bool position(uint32_t unit,float out[3]) {
 try {
  auto object=WieldRepair::unit_address(io,exe,unit),vtable=io.value<uintptr_t>(object),method=io.value<uintptr_t>(vtable+0xe8);uintptr_t graph;
  if(vtable==exe+0x16770e0 && method==exe+0x2cac50 && api->code_matches(method,"48 8D 41 48 C3"))graph=object+0x48;
  else if(method==exe+0x2bd870 && api->code_matches(method,"48 8D 41 60 C3"))graph=object+0x60;
  else return false;
  auto poses=io.value<uintptr_t>(graph+40);auto b=io.bytes(poses+48,12);memcpy(out,b.data(),12);return hammer_combat::in_stun_area(out,out);
 }catch(...){return false;}
}
void stun_tick() {
 if(!count || !api->session || !api->game_state || !api->apply_status)return;
 HfSession session{};api->session(mod,&session);
 if((session.role!=HF_SESSION_SOLO&&session.role!=HF_SESSION_HOST)||api->game_state(mod)!=HF_STATE_MISSION){head=tail=count=0;return;}
 auto& pulse=pulses[head];bool done=false;
 try {
  if(GetTickCount64()-pulse.since>1000 || pulse.factory!=io.value<uintptr_t>(game+0x346bf98) || pulse.health!=io.value<uintptr_t>(game+0x3326688))done=true;
  else {
   uint32_t n=io.value<uint32_t>(pulse.health+4128),capacity=io.value<uint32_t>(pulse.health+4112);require(n<=4096&&n<=capacity,"hammer health bounds");
   auto descriptors=io.value<uintptr_t>(pulse.health+4168),rows=io.value<uintptr_t>(pulse.health+4184);
   for(unsigned budget=0;budget<256 && pulse.cursor<n;budget++,pulse.cursor++) {
    auto descriptor=io.value<uintptr_t>(descriptors+size_t(pulse.cursor)*8);if(!descriptor)continue;
    auto raw=io.bytes(descriptor,24);if(!enemy(get<uint64_t>(raw,0)) || (get<uint32_t>(raw,20)&3)!=1)continue;
    auto id=get<uint32_t>(raw,8);auto row=rows+size_t(pulse.cursor)*440;auto hp=io.value<int32_t>(row+20);auto life=io.value<uint32_t>(row+412);float point[3];
    if(!id||id==UINT32_MAX || hp<=0 || life || !position(get<uint32_t>(raw,12),point) || !hammer_combat::in_stun_area(pulse.position,point))continue;
    if(WieldRepair::descriptor(io,pulse.factory,id)!=raw || io.bytes(descriptor,24)!=raw || io.value<uintptr_t>(descriptors+size_t(pulse.cursor)*8)!=descriptor ||
       io.value<uintptr_t>(pulse.health+4184)!=rows || io.value<int32_t>(row+20)!=hp || io.value<uint32_t>(row+412)!=0)continue;
    int result=api->apply_status(mod,id,HF_STATUS_STUN_LARGE,100.0f);
    if(result==HF_ACT_REQUESTED || result==HF_ACT_DONE)stun_requests++;else stun_refusals++;
   }done=pulse.cursor>=n;
  }
 }catch(...){done=true;}
 if(done){head=(head+1)%pulses.size();count--;}
}
void tick(HfMod*,float dt,void*) {
 elapsed+=dt;if(stopped||!game)return;
 if(!ready) {
  poll+=dt;if(poll<.5f)return;poll=0;
  if(hf_primary_combat_bind()) {
   volume_damage=hf_primary_strike_damage();if(!volume_damage)return;
   memcpy(volumes[0].data(),HF_HAMMER_VOLUME_0,248);memcpy(volumes[1].data(),HF_HAMMER_VOLUME_1,248);
   for(auto& volume:volumes)memcpy(volume.data()+4,&volume_damage,4);
   ready=true;api->log(mod,"Primary hammer combat ready: private demolition50 strike%u/blast425, scoped flinch prevention, host-only enemy large stun radius6m",volume_damage);
  }else if(elapsed>90){stopped=true;api->log(mod,"Primary hammer combat refused: private profile/shared explosion registry readiness exceeded90s; original damage retained");}
 }else {
  stun_tick();metrics+=dt;
  if(metrics>=1.0f) {
   metrics=0;std::array<uint64_t,5> current{strike_routes,blast_routes,flinch_routes,stun_requests,stun_refusals};
   if(current!=reported){reported=current;api->log(mod,"Primary hammer combat counters: strikes%llu blasts%llu flinch%llu enemy-stun%llu status-refusals%llu",strike_routes,blast_routes,flinch_routes,stun_requests,stun_refusals);}
  }
 }
}
void loaded(HfMod*,HfEvent,void*) {
 size_t size=0;game=api->module_base("game.dll",&size);exe=api->module_base("helldivers2.exe",&size);
 if(!game||!exe || !api->code_matches(game+0x7d38d0,"48 89 5C 24 10 48 89 7C 24 18 55 41 56 41 57 48 8D AC 24 40 FC FF FF") ||
  !api->code_matches(game+0x11d9df0,"48 89 5C 24 10 48 89 6C 24 18 56 57 41 54 41 56 41 57 48 83 EC 20") ||
  !api->code_matches(game+0x13c0a80,"40 53 48 83 EC 30 8B 41 20 4C 8B DA 48 8B D9 3D 00 01 00 00 0F 83 69 02 00 00")) {stopped=true;api->log(mod,"Primary hammer combat refused: native source code pin changed");return;}
 for(const auto& pin:HAMMER_FLINCH_PINS)if(!api->code_matches(game+pin.start,pin.pattern)){stopped=true;api->log(mod,"Primary hammer combat refused: flinch reader pin changed");return;}
 if(api->hook_ex(mod,game+0x7d38d0,(void*)zone_hook,(void**)&next_zone,0) || api->hook_ex(mod,game+0x11d9df0,(void*)hf_primary_modifier_entry,(void**)&next_modifier,0) ||
    api->hook_ex(mod,game+0x13c0a80,(void*)explosion_hook,(void**)&next_explosion,0)){stopped=true;api->log(mod,"Primary hammer combat refused: native hook chain unavailable");return;}
 hooks=true;api->log(mod,"Primary hammer scoped combat hooks installed; private profile readiness and gameplay pending");
}
}
extern "C" int hf_primary_combat_init(const HfApi* hf,HfMod* owner,uint64_t resource) {
 if(!hf||resource!=hammer_combat::PRIMARY||!hf->code_matches||!hf->hook_ex||!hf->read||!hf->on||!hf->on_game_thread)return 1;
 api=hf;mod=owner;primary=resource;io.read=hf->read;io.write=hf->write;
 return hf->on(owner,HF_EVENT_GAME_LOADED,loaded,nullptr) || !hf->on_game_thread(owner,tick,nullptr);
}
#else
extern "C" int hf_primary_combat_init(const HfApi*,HfMod*,uint64_t){return 0;}
#endif
