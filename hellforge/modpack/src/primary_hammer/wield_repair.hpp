#pragma once
#include "wield_policy.hpp"
namespace hammer {
struct WieldRepair {
 WieldPolicy policy;uintptr_t exe_base=0;uint32_t logged_avatar=0;
 static uint32_t lookup(const Io& io,uintptr_t map,uint32_t key){
  auto h=io.bytes(map,20);auto buckets=get<uintptr_t>(h,0);auto cap=get<uint32_t>(h,8),empty=get<uint32_t>(h,12),mult=get<uint32_t>(h,16);
  require(buckets && cap && cap<=1048576 && !(cap&(cap-1)),"wield map bounds");
  if(key==empty)return UINT32_MAX;
  for(uint32_t i=0;i<std::min(cap,4096u);i++){
   auto pair=io.bytes(buckets+size_t((uint32_t(key*mult)+i)&(cap-1))*8,8);auto found=get<uint32_t>(pair,0);
   if(found==key)return get<uint32_t>(pair,4);if(found==empty)return UINT32_MAX;
  }throw std::runtime_error("wield map probe limit");
 }
 static Bytes descriptor(const Io& io,uintptr_t factory,uint32_t entity){
  auto index=lookup(io,factory+0xf1aeb0,entity);require(index<1048576,"wield entity absent");
  auto d=io.bytes(factory+0xf32f18+size_t(index)*24,24);
  require(get<uint32_t>(d,8)==entity && (get<uint32_t>(d,20)&1),"wield inactive entity");return d;
 }
 static uintptr_t unit_address(const Io& io,uintptr_t exe,uint32_t id){
  auto registry=io.value<uintptr_t>(exe+0x1a100f0);auto n=io.value<uint32_t>(registry+0x98);auto slot=id&0x3fffff;
  require(slot<n && n<=4194304,"wield unit bounds");
  auto generations=io.value<uintptr_t>(registry+0xa0);auto gen=io.value<uint8_t>(generations+slot);
  require(id==(slot|(uint32_t(gen)<<22)),"wield stale unit");
  auto objects=io.value<uintptr_t>(registry+0x88);auto at=io.value<uintptr_t>(objects+size_t(slot)*8);
  require(io.value<uint32_t>(at+8)==id,"wield unit identity");return at;
 }
 static uintptr_t relative(uintptr_t base,uint32_t off,size_t size){
  require(off>=12 && off<16777216 && size<=16777216-off,"wield graph bounds");return base+off;
 }
 static uint64_t layer(const Io& io,uintptr_t controller,uintptr_t machine,uint32_t index,uint64_t before,uint64_t after){
  auto group_list=relative(machine,io.value<uint32_t>(machine+8),128);
  require(io.value<uint32_t>(group_list)==31,"wield group count");
  auto group=relative(group_list,io.value<uint32_t>(group_list+4+index*4),12);
  auto n=io.value<uint32_t>(group+8);require(n && n<=4096,"wield state count");
  auto states=io.value<uintptr_t>(controller+0x50);auto current=io.value<uintptr_t>(states+index*8);bool owned=false;
  for(uint32_t i=0;i<n;i++)if(relative(group,io.value<uint32_t>(group+12+i*4),88)==current){owned=true;break;}
  require(owned,"wield current state outside graph");auto hash=io.value<uint64_t>(current);
  if(hash==after)return hash;if(hash!=before)return hash;
  auto ec=io.value<uint32_t>(current+40),lc=io.value<uint32_t>(current+48);require(ec && ec<=4096 && lc && lc<=4096,"wield event bounds");
  auto events=relative(current,io.value<uint32_t>(current+44),size_t(ec)*8);int32_t link=-1;uint32_t matches=0;
  for(uint32_t i=0;i<ec;i++)if(io.value<uint32_t>(events+i*8)==0x2c8839e3){link=io.value<int32_t>(events+i*8+4);matches++;}
  require(matches==1 && link>=0 && uint32_t(link)<lc,"wield draw transition absent/conditional");
  auto links=relative(current,io.value<uint32_t>(current+52),size_t(lc)*16);auto row=io.bytes(links+size_t(link)*16,16);
  auto dest=get<uint32_t>(row,0);require(dest<n && get<float>(row,4)>.199f && get<float>(row,4)<.201f && get<uint32_t>(row,8)==0,"wield draw link changed");
  auto target=relative(group,io.value<uint32_t>(group+12+dest*4),88);
  require(io.value<uint64_t>(target)==after,"wield draw target changed");return hash;
 }
 void tick(const HfApi* api,HfMod* owner,const Io& io,uintptr_t game,uint64_t primary,float dt){
  try{
   if(!exe_base){size_t size=0;exe_base=api->module_base("helldivers2.exe",&size);}auto exe=exe_base;require(exe!=0,"wield executable unavailable");
   auto players=io.value<uintptr_t>(game+0x3326468),context=io.value<uintptr_t>(game+0x347cef0),factory=io.value<uintptr_t>(game+0x346bf98);
   if(!players || !context || !factory)return;
   auto count=io.value<uint32_t>(players+0x84);require(count<=4,"wield player bounds");auto peer=io.value<uintptr_t>(context+0xb398);if(!peer)return;
   uint32_t avatar=0;
   for(uint32_t i=0;i<count;i++){
    if(io.value<uintptr_t>(players+0x2c8+i*0x38)!=peer)continue;
    auto network=io.value<uint32_t>(players+0x3a8+i*0x20),index=lookup(io,factory+0xf22ec8,network);
    if(index==UINT32_MAX)continue;require(index<1048576 && !avatar,"wield local avatar ambiguity");
    avatar=io.value<uint32_t>(factory+0xf32f20+size_t(index)*24);
   }if(!avatar)return;
   auto desc=descriptor(io,factory,avatar);require(get<uint64_t>(desc,0)==UINT64_C(0x4d1c334d294dfa97),"wield avatar resource");
   auto id=get<uint32_t>(desc,12);auto address=unit_address(io,exe,id);
   if(policy.avatar==avatar && policy.unit==id && policy.done)return;
   auto wielder=io.value<uintptr_t>(game+0x3326420);auto wi=lookup(io,wielder+0x30,avatar);
   if(wi==UINT32_MAX)return;require(wi<65536,"wield held index");
   auto held=io.value<uint32_t>(io.value<uintptr_t>(wielder+0x60)+size_t(wi)*0x1d0);
   if(!held){policy.observe(avatar,id,dt,false,false,false);return;}
   auto weapon=descriptor(io,factory,held);
   if(get<uint64_t>(weapon,0)!=primary){policy.observe(avatar,id,dt,true,false,false);return;}
   auto inventory=io.value<uintptr_t>(game+0x3326738);auto ii=lookup(io,inventory+0x28,avatar);
   if(ii==UINT32_MAX)return;require(ii<65536,"wield inventory index");
   auto selection=io.value<uint32_t>(io.value<uintptr_t>(inventory+0x50)+size_t(ii)*0x30+0x1c);
   auto health=io.value<uintptr_t>(game+0x3326688);auto hi=lookup(io,health+0x1030,avatar);
   if(hi==UINT32_MAX)return;require(hi<io.value<uint32_t>(health+0x1020),"wield health index");
   auto hr=io.value<uintptr_t>(health+0x1058)+size_t(hi)*0x1b8;
   if(selection!=1 || io.value<int32_t>(hr+20)<=0 || io.value<uint32_t>(hr+0x19c)!=0){policy.observe(avatar,id,dt,false,false,false);return;}
   auto state_system=io.value<uintptr_t>(game+0x3326d20);auto si=lookup(io,state_system+0xf8,avatar);
   if(si==UINT32_MAX)return;require(si<1024,"wield avatar state index");
   auto state_desc=io.value<uintptr_t>(state_system+0x110+size_t(si)*8);require(io.value<uint32_t>(state_desc+8)==avatar,"wield avatar state owner");
   auto flags=io.bytes(state_system+0x53d900+size_t(si)*0x1238+0xf80,24);
   // The captured respawn transient (second mask bit55) lasts ~7 seconds.
   // Do not queue a draw until it clears and native held/melee tags are set.
   if(((get<uint64_t>(flags,0)>>50)&3)!=3 || (get<uint64_t>(flags,8)&(UINT64_C(1)<<55))){policy.observe(avatar,id,dt,false,false,false);return;}
   auto controller=io.value<uintptr_t>(address+0x178);if(!controller)return;
   auto machine=io.value<uintptr_t>(controller+0x28);require(io.value<uint32_t>(machine+4)==31 && io.value<uint32_t>(controller+0x48)==31 && io.value<uint32_t>(machine+20)==54,"wield controller shape");
   auto hashes=relative(machine,io.value<uint32_t>(machine+24),54*4);require(io.value<uint32_t>(hashes+14*4)==0xc617794e,"wield grip variable changed");
   auto values=io.value<uintptr_t>(controller+0x80);if(io.value<float>(values+14*4)!=50.f){policy.observe(avatar,id,dt,false,false,false);return;}
   auto l9=layer(io,controller,machine,9,UINT64_C(0x33fd52dc20c64280),UINT64_C(0xbb196972b6c033a2));
   auto l25=layer(io,controller,machine,25,UINT64_C(0x70c9f2778791153a),UINT64_C(0xdd8348d6f01b9f18));
   bool eligible=l9==UINT64_C(0x33fd52dc20c64280) && l25==UINT64_C(0x70c9f2778791153a);
   // Once any other animation state has been reached, leave this avatar to
   // the normal wield/holster/attack flow for the rest of its lifetime.
   if(!policy.observe(avatar,id,dt,false,eligible,!eligible))return;
   // Native UnitApi animation_event queues this event; no synthetic keypress,
   // slot change, variable overwrite or shared character-resource mutation.
   auto ua=io.value<uintptr_t>(io.value<uintptr_t>(game+0x3326308)+0x18);auto emit=io.value<uintptr_t>(ua+0x370);
   require(emit==exe+0x201900 && api->code_matches(emit,"48 89 5C 24 08 57 48 83 EC 20 8B DA 8B F9 E8 AD BF E9 FF 44 8B C3 8B D7 48 8B 48 18 48 8B 5C 24 30 48 83 C4 20 5F E9 D5 E3 F2 FF"),"wield event API pin");
   require(descriptor(io,factory,avatar)==desc && descriptor(io,factory,held)==weapon && io.value<uintptr_t>(game+0x346bf98)==factory && unit_address(io,exe,id)==address && io.value<uintptr_t>(address+0x178)==controller && io.value<uint32_t>(io.value<uintptr_t>(wielder+0x60)+size_t(wi)*0x1d0)==held && io.value<uint32_t>(io.value<uintptr_t>(inventory+0x50)+size_t(ii)*0x30+0x1c)==1,"wield identity changed before event");
   ((void(*)(uint32_t,uint32_t))emit)(id,0x2c8839e3);
   api->log(owner,"Primary hammer: queued native draw once for new avatar %u / hammer %u; visual and attack acceptance pending",avatar,held);
  }catch(const std::exception& e){
   // No raw memory write or partial repair exists to roll back. A failed read
   // skips the attempt; repeated output is bounded to one notice per avatar.
   auto key=policy.avatar?policy.avatar:UINT32_MAX;if(logged_avatar!=key){api->log(owner,"Primary hammer draw repair waiting/refused: %s",e.what());logged_avatar=key;}
  }
 }
};
}
