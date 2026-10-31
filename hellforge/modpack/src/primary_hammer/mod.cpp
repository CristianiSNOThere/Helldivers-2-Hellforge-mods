#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "hellforge_mod.h"
#include "transaction.hpp"
#include <array>
#include <algorithm>
#include <set>
#include <string>
using namespace hammer;
static const HfApi* api;static HfMod* owner;static uintptr_t game;
static uint32_t item;static uint64_t entity;static bool failed,applied;static float elapsed;
static uintptr_t generation, progression_generation;
static std::vector<Edit> committed;
static constexpr uint64_t SUPPORT=UINT64_C(0x5f3ec9bda2bd8553), PRIMARY=UINT64_C(0x4fb0f8c02f55c82b);
static Io memory;
#include "wield_repair.hpp"
static WieldRepair wield_repair;
using CatalogFn=uint64_t(*)(const uint8_t*,uint32_t,uintptr_t);
static CatalogFn next_catalog;
static volatile LONG catalogue_seen;
static volatile LONG entities_seen;
static uint64_t catalogue_hook(const uint8_t* data,uint32_t count,uintptr_t context){
 try{
  Bytes source=memory.bytes((uintptr_t)data,size_t(count)*112);
  Bytes extended=catalogue(source,count,item);
  uint64_t result=next_catalog(extended.data(),count+1,context);
  InterlockedExchange(&catalogue_seen,1);
  api->log(owner,"Primary hammer: one uncustomizable catalogue root parsed");
  return result;
 }catch(const std::exception& e){api->log(owner,"Primary hammer catalogue refused: %s",e.what());return next_catalog(data,count,context);}
}
// Index tables retain the engine's fixed capacity and open-addressing rules.
static size_t index_of(const Bytes& b,uint32_t slots,uint64_t key){
 require(b.size()>=size_t(slots)*16,"index extent");
 for(uint32_t i=0;i<slots;i++){size_t n=size_t((key%slots+i)%slots)*16;auto k=get<uint64_t>(b,n);if(k==key)return n;if(!k)break;}
 throw std::runtime_error("entity missing from component index");
}
static uintptr_t private_record(Transaction& tx,uintptr_t factory,uint32_t component,uint32_t slots,uint32_t stride,uint32_t schema,const char* name){
 auto table=memory.value<uintptr_t>(factory+0xf12478+component*8);auto h=memory.bytes(table-24,24);
 require(get<uint64_t>(h,0)==UINT64_C(0x00000001444c444c) && get<uint32_t>(h,8)==schema,"component schema mismatch");
 uint32_t bytes=get<uint32_t>(h,12);require(bytes>=slots*16 && bytes<4194304,"component extent mismatch");
 auto b=memory.bytes(table,size_t(slots)*16);size_t n=index_of(b,slots,entity);uint32_t record=get<uint32_t>(b,n+8),owners=0;
 tx.observe(factory+0xf12478+component*8,8);tx.observe(table-24,24);tx.observe(table,size_t(slots)*16);
 require(get<uint32_t>(b,n+12)==0,"flagged component index");
 // The loader copies vanilla headers unchanged, then appends its registered records.
 uint32_t first=component==29?193:(bytes-slots*16+stride-1)/stride;
 require(record>=first && record<first+192,"record outside registered extension");
 for(uint32_t i=0;i<slots;i++)if(get<uint64_t>(b,i*16) && get<uint32_t>(b,i*16+8)==record)owners++;
 require(owners==1,"component record shared");
 uint32_t sdk_size=0;uintptr_t sdk=api->component_record(name,entity,&sdk_size);
 uintptr_t at=table+slots*16+size_t(record)*stride;
 // SDK15 has no OwnedTable descriptor for its separately managed Loadout table;
 // its generic lookup rejects records beyond the 193 vanilla records. The live
 // hash index, unique owner and registered extension bounds validate that case.
 bool loadout_lookup_gap=component==29 && sdk==0 && sdk_size==0 && bytes==13128;
 if(!loadout_lookup_gap && (sdk!=at || sdk_size!=stride))throw std::runtime_error(std::string(name)+": SDK/live record mismatch");
 memory.bytes(at,stride);return at;
}
static void prepare(Transaction& tx,uintptr_t factory,uintptr_t prog){
 uintptr_t gear=private_record(tx,factory,29,386,32,0x4f2cf417,"LoadoutEntryComponentData");
 auto load=memory.bytes(gear,32);require(get<uint32_t>(load,0)==item && get<uint32_t>(load,4)==2,"new loadout shape");
 // Preserve the hammer's native trait slots; Torcher's first trait is Incendiary.
 set<uint32_t>(load,4,1);tx.add(gear,load);
 uintptr_t customization=private_record(tx,factory,271,382,4872,0xeba8f3d0,"WeaponCustomizationComponentData");
 auto custom=memory.bytes(customization,4872);
 require(get<uint32_t>(custom,0x12fc)==0,"unexpected mastery package");
 for(size_t k=0;k<160;k++)require(custom[k]==0,"customization slots unexpectedly present");
 tx.observe(customization,160);tx.observe(customization+0x12fc,4);
 // Rebuild only the derived armory rows. Preserve every peer's row and order within its category.
 uintptr_t armory=memory.value<uintptr_t>(game+0x33269e8);auto meta=memory.bytes(armory+0x1b24,56);
 std::array<Bytes,7> groups;Bytes ours;uint32_t start=get<uint32_t>(meta,0),cursor=start,total=0,found=0;
 require(start>=219 && start<1024,"armory storage layout");
 for(uint32_t g=0;g<7;g++){
  auto first=get<uint32_t>(meta,g*4),count=get<uint32_t>(meta,28+g*4);require(first==cursor && count<=512 && total+count<=512,"armory ranges");
  auto rows=count?memory.bytes(armory+size_t(first)*32,size_t(count)*32):Bytes{};
  for(uint32_t j=0;j<count;j++){
   Bytes row(rows.begin()+j*32,rows.begin()+(j+1)*32);require(get<uint32_t>(row,16)==g,"armory category mismatch");
   if(get<uint32_t>(row,0)==item || get<uint64_t>(row,8)==entity){require(get<uint32_t>(row,0)==item && get<uint64_t>(row,8)==entity && g==2,"armory identity conflict");ours=row;found++;}
   else groups[g].insert(groups[g].end(),row.begin(),row.end());
  }cursor+=count;total+=count;
 }
 require(found==1,"new armory entry absent/duplicated");set<uint32_t>(ours,16,1);groups[1].insert(groups[1].end(),ours.begin(),ours.end());
 Bytes joined;cursor=start;
 for(uint32_t g=0;g<7;g++){set<uint32_t>(meta,g*4,cursor);set<uint32_t>(meta,28+g*4,uint32_t(groups[g].size()/32));cursor+=uint32_t(groups[g].size()/32);joined.insert(joined.end(),groups[g].begin(),groups[g].end());}
 require(joined.size()==size_t(total)*32,"armory item count changed");tx.add(armory+size_t(start)*32,joined);tx.add(armory+0x1b24,meta);
 // Equipment is not exposed by the SDK. Reserve an unused row in the loader's
 // existing allocation, preserving its pointer and every peer's appended row.
 uintptr_t slot=factory+0xf12478+233*8,table=memory.value<uintptr_t>(slot);auto header=memory.bytes(table-24,24);
 require(get<uint64_t>(header,0)==UINT64_C(0x00000001444c444c) && get<uint32_t>(header,8)==0x5357a825,"equipment schema");
 uint32_t length=get<uint32_t>(header,12);require(length==91048,"equipment table size");
 require(memory.dedicated && memory.dedicated(table-32,32+688*16+(345+192)*232),"equipment loader reserve unavailable");
 auto equipment=memory.bytes(table,688*16);uint32_t maximum=344;
 for(uint32_t i=0;i<688;i++)if(get<uint64_t>(equipment,i*16)){require(get<uint32_t>(equipment,i*16+12)==0,"equipment index flags");auto r=get<uint32_t>(equipment,i*16+8);require(r<345+192,"equipment record outside reserve");if(r>maximum)maximum=r;}
 require(maximum+1<345+192,"equipment reserve exhausted");
 size_t from=index_of(equipment,688,SUPPORT),to=index_of(equipment,688,entity),primary=index_of(equipment,688,PRIMARY);
 size_t offset=688*16+size_t(get<uint32_t>(equipment,from+8))*232;
 size_t primary_offset=688*16+size_t(get<uint32_t>(equipment,primary+8))*232;
 require(get<uint32_t>(equipment,to+8)==get<uint32_t>(equipment,from+8),"new equipment already owned by another provider");
 Bytes row=memory.bytes(table+offset,232),primary_row=memory.bytes(table+primary_offset,232);
 // Preserve the hammer's complete native equipment settings, including melee
 // classification. Armory placement is controlled separately by loadout type.
 require(get<uint32_t>(row,128)==30 && get<uint32_t>(primary_row,128)==31,"native melee/Special classification changed");
 uintptr_t spare=table+688*16+size_t(maximum+1)*232;auto empty=memory.bytes(spare,232);
 require(std::all_of(empty.begin(),empty.end(),[](uint8_t v){return !v;}),"equipment reserve not empty");
 // Observe every peer index byte while excluding our transaction's record index.
 tx.observe(slot,8);tx.observe(table-24,24);
 tx.observe(table,to+8);tx.observe(table+to+12,688*16-to-12);
 tx.observe(table+offset,232);tx.observe(table+primary_offset,232);
 tx.add(spare,row);tx.add(table+to+8,scalar<uint32_t>(maximum+1));
 // The root was built by native finalization, without descendants or mastery. Grant only if the support-hammer item is owned.
 uint32_t n=memory.value<uint32_t>(prog+0x1ce0);require(n>0 && n<=4096,"progression bounds");
 uintptr_t own=0,base=0;uint32_t ownIndex=0;for(uint32_t i=0;i<n;i++){
  auto state=memory.bytes(prog+0x1ce4+size_t(i)*184,184);
  if(get<uint32_t>(state,4)==item){require(!own && get<uint32_t>(state,0)==i && get<uint32_t>(state,8)==item && get<uint32_t>(state,12)==1,"private root mismatch");own=prog+0x1ce4+size_t(i)*184;ownIndex=i;}
  if(get<uint32_t>(state,4)==3962328364u){require(!base,"ambiguous support purchase");base=prog+0x1ce4+size_t(i)*184;}
 }
 require(own && base,"catalogue root or support purchase missing");auto purchase=memory.bytes(base,184);
 require(get<uint32_t>(purchase,4)==3962328364u && get<uint32_t>(purchase,8)==3962328364u && get<uint32_t>(purchase,20)==2 && purchase[181]==0,"support hammer not owned or disabled");
 auto state=memory.bytes(own,184);require(state[181]==0 && get<uint32_t>(state,20)<=2,"private root disabled or unfamiliar");
 // Native equipment classification supplies the armory group. Owned roots have
 // zero at +0x90; it is not a Special class field.
 set<uint32_t>(state,20,2);set<uint32_t>(state,0xb0,1);tx.add(own,state);
 (void)ownIndex;
}
static void tick(HfMod*,float dt,void*){
 elapsed+=dt;if(failed)return;
 if(applied){try{require(memory.value<uintptr_t>(game+0x346bf98)==generation && memory.value<uintptr_t>(game+0x347cef8)==progression_generation,"world or catalogue replaced; restart required");wield_repair.tick(api,owner,memory,game,entity,dt);}catch(const std::exception& e){failed=true;api->log(owner,"Primary hammer: %s",e.what());}return;}
 if(elapsed>90){failed=true;api->log(owner,"Primary hammer refused: registration not ready within 90 seconds");return;}
 if(!catalogue_seen || !entities_seen || !game)return;
 try{
  auto factory=memory.value<uintptr_t>(game+0x346bf98),prog=memory.value<uintptr_t>(game+0x347cef8);
  if(!factory || !prog || memory.value<uint32_t>(prog+0xddcf8)<=7)return;
  Transaction tx{memory,{}};prepare(tx,factory,prog);
  require(memory.value<uintptr_t>(game+0x346bf98)==factory && memory.value<uintptr_t>(game+0x347cef8)==prog,"world changed before commit");
  tx.commit();committed=std::move(tx.edits);generation=factory;progression_generation=prog;applied=true;api->log(owner,"Primary hammer: primary registration applied; native melee equipment retained; initial equip/gameplay pending");
 }catch(const std::exception& e){failed=true;api->log(owner,"Primary hammer registration refused: %s",e.what());}
}
static void loaded(HfMod*,HfEvent,void*){
 size_t size=0;game=api->module_base("game.dll",&size);
 if(!game || size!=74727424 || !api->code_matches(game+0x1038fb0,"4C 8B DC 41 55 48 81 EC 40 01 00 00 48 8B 05 4D 30 60 01")){
  failed=true;api->log(owner,"Primary hammer: catalogue function pin refused");return;
 }
 if(api->hook_ex(owner,game+0x1038fb0,(void*)catalogue_hook,(void**)&next_catalog,100))failed=true;
}
static void entities_ready(HfMod*,HfEvent,void*){InterlockedExchange(&entities_seen,1);}
extern "C" int hf_primary_combat_init(const HfApi*,HfMod*,uint64_t);
extern "C" int hf_primary_hammer_init(const HfApi* hf,HfMod* mod){
 if(!hf || hf->version<13 || hf->size<offsetof(HfApi,code_matches)+sizeof(hf->code_matches) || !hf->build_id || std::strcmp(hf->build_id(),"25480438") || !hf->read || !hf->write || !hf->hook_ex || !hf->code_matches || !hf->component_record || !hf->on_game_thread || !hf->on || !hf->add_weapon || !hf->weapon_entity || !hf->log || !hf->module_base)return 1;
 api=hf;owner=mod;memory.read=hf->read;memory.write=hf->write;
 memory.dedicated=[](uintptr_t at,size_t n){MEMORY_BASIC_INFORMATION v{};return VirtualQuery((void*)at,&v,sizeof v)==sizeof v && (uintptr_t)v.AllocationBase==at && (uintptr_t)v.BaseAddress==at && v.State==MEM_COMMIT && (v.Protect&0xff)==PAGE_READWRITE && v.RegionSize>=n;};
 const char* own[]={"WeaponCustomizationComponentData",nullptr};HfWeapon w{};w.size=sizeof w;w.base=HF_ITEM_CQC20_BREACHING_HAMMER;
 w.unit="superearth/primary_hammer/cqc20";w.designation="CQC-20";w.name="Primary Breaching Hammer";w.own_components=own;
 item=hf->add_weapon(mod,&w);if(!item)return 1;entity=hf->weapon_entity(item);
 if(hf_primary_combat_init(hf,mod,entity))hf->log(mod,"Primary hammer combat setup refused; primary registration continues");
 if(hf->on(mod,HF_EVENT_GAME_LOADED,loaded,nullptr) || hf->on(mod,HF_EVENT_ENTITIES_READY,entities_ready,nullptr) || !hf->on_game_thread(mod,tick,nullptr))return 1;
 return 0;
}
