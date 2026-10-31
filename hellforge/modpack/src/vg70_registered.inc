/* VG-70 profiles are SDK content, never reservations in a stock lookup slot. */
typedef struct HfVg70Registered {
 const HfApi* api;HfMod* mod;int ready;
} HfVg70Registered;
static HfVg70Registered HF_VG70_REGISTERED;
static const char* const HF_VG70_DAMAGE_NAMES[5]={
 "superearth.arsenal-native-port/vg70/auto-550-damage",
 "superearth.arsenal-native-port/vg70/auto-750-damage",
 "superearth.arsenal-native-port/vg70/auto-1000-damage",
 "superearth.arsenal-native-port/vg70/volley-damage",
 "superearth.arsenal-native-port/vg70/total-damage"};
static const char* const HF_VG70_PROJECTILE_NAMES[5]={
 "superearth.arsenal-native-port/vg70/auto-550-round",
 "superearth.arsenal-native-port/vg70/auto-750-round",
 "superearth.arsenal-native-port/vg70/auto-1000-round",
 "superearth.arsenal-native-port/vg70/volley-round",
 "superearth.arsenal-native-port/vg70/total-round"};
static int hf_vg70_register(const HfApi* api,HfMod* mod) {
 if(!api || !api->add_damage || !api->add_projectile || !api->damage_row || !api->projectile_row ||
    api->size<offsetof(HfApi,content)+sizeof(api->content) || !api->content)return 0;
 HF_VG70_REGISTERED.api=api;HF_VG70_REGISTERED.mod=mod;
 for(size_t i=0;i<5;i++) {
  HfDamage d={0};d.size=sizeof(d);d.base=HF_VG70_DAMAGE_DONORS[i];d.name=HF_VG70_DAMAGE_NAMES[i];
  uint32_t id=api->add_damage(mod,&d);if(id<650 || id>=714)return 0;
  for(size_t j=0;j<i;j++)if(id==HF_VG70_PRIVATE_DAMAGES[j])return 0;
  HF_VG70_PRIVATE_DAMAGES[i]=id;
 }
 for(size_t i=0;i<5;i++) {
  HfProjectile p={0};p.size=sizeof(p);p.base=HF_VG70_PROJECTILE_DONORS[i];p.name=HF_VG70_PROJECTILE_NAMES[i];p.damage=HF_VG70_PRIVATE_DAMAGES[i];
  uint32_t id=api->add_projectile(mod,&p);if(id<351 || id>=415)return 0;
  for(size_t j=0;j<i;j++)if(id==HF_VG70_PRIVATE_PROJECTILES[j])return 0;
  HF_VG70_PRIVATE_PROJECTILES[i]=id;
 }
 if(!HF_REGISTRY_DAMAGE_WITNESS_ID)HF_REGISTRY_DAMAGE_WITNESS_ID=HF_VG70_PRIVATE_DAMAGES[0];
 if(!HF_REGISTRY_PROJECTILE_WITNESS_ID)HF_REGISTRY_PROJECTILE_WITNESS_ID=HF_VG70_PRIVATE_PROJECTILES[0];
 HF_VG70_REGISTERED.ready=1;return 1;
}
static int hf_vg70_registered_profile(void* context,int kind,uint32_t donor,HfVg70ProfileBinding* out) {
 const HfApi* api=HF_VG70_REGISTERED.api;HfMod* mod=HF_VG70_REGISTERED.mod;
 size_t index=5,image_size=0;uintptr_t game,root,row,lookup;uint32_t id,actual,slots,content_kind;const char* name;
 (void)context;
 if(!HF_VG70_REGISTERED.ready || !out || (kind!=1 && kind!=2))return 0;
 for(size_t i=0;i<5;i++)if((kind==1?HF_VG70_DAMAGE_DONORS[i]:HF_VG70_PROJECTILE_DONORS[i])==donor){index=i;break;}
 if(index==5)return 0;
 id=kind==1?HF_VG70_PRIVATE_DAMAGES[index]:HF_VG70_PRIVATE_PROJECTILES[index];
 row=kind==1?api->damage_row(id):api->projectile_row(id);
 name=kind==1?HF_VG70_DAMAGE_NAMES[index]:HF_VG70_PROJECTILE_NAMES[index];content_kind=kind==1?HF_KIND_DAMAGE:HF_KIND_PROJECTILE;
 game=api->module_base("game.dll",&image_size);
 if(!game || !hf_registry_root(api,game,image_size,kind,&root) || !row ||
    api->read(root+(uintptr_t)id*8,&lookup,8) || lookup!=row || api->read(row,&actual,4) || actual!=id)return 0;
 /* Check the SDK registration's actual owner/name/base, not an unreferenced
  * serialized id or only a matching row payload. */
 unsigned owners=0;int ended=0;
 for(uint32_t i=0;i<4096;i++) {
  HfContent c={0};c.size=sizeof(c);if(!api->content(mod,i,&c)){ended=1;break;}
  if(c.kind!=content_kind || c.id!=id)continue;
  if(!c.mod || strcmp(c.mod,"superearth.arsenal-native-port") || !c.name || strcmp(c.name,name) || c.base!=donor)return 0;
  if(kind==2 && c.link!=HF_VG70_PRIVATE_DAMAGES[index])return 0;
  owners++;
 }
 if(!ended || owners!=1)return 0;
 /* The pinned stock SDK allocation contains all 64 extension slots. Refuse
  * aliases under stock or foreign registered ids without rewriting a pointer. */
 slots=kind==1?714u:415u;
 uintptr_t pointers[714];if(api->read(root,pointers,(size_t)slots*8))return 0;
 for(uint32_t i=0;i<slots;i++)if(i!=id && pointers[i]==row)return 0;
 out->id=id;out->row=row;out->slot=root+(uintptr_t)id*8;out->source_slot=root+(uintptr_t)donor*8;
 return 1;
}

static int hf_vg70_registered_owned(void) {
 const HfApi* api=HF_VG70_REGISTERED.api;
 if(!HF_VG70_REGISTERED.ready)return 0;
 for(unsigned t=0;t<2;t++)for(unsigned i=0;i<5;i++) {
  const HfVg70DataRecord* record=&HF_VG70_TABLES[t].records[i];
  HfVg70ProfileBinding binding={0};uint8_t actual[272],expected[272];uintptr_t source=0;
  if(!hf_vg70_registered_profile(NULL,t==0?2:1,record->id,&binding) ||
     api->read(binding.source_slot,&source,8) || api->read(source,actual,record->size))return 0;
  memcpy(expected,record->before,record->size);
  if(record->foliage_query){uint32_t q=get_u32(actual,244);if(q!=0 && q!=2)return 0;put_u32(expected,244,q);}
  if(memcmp(actual,expected,record->size) || api->read(binding.row,actual,record->size))return 0;
  memcpy(expected,record->after,record->size);put_u32(expected,0,binding.id);
  if(t==0) {
   put_u32(expected,60,HF_VG70_PRIVATE_DAMAGES[i]);
   if(record->foliage_query){uint32_t q=get_u32(actual,244);if(q!=0 && q!=2)return 0;put_u32(expected,244,q);}
  }
  if(memcmp(actual,expected,record->size))return 0;
 }
 return 1;
}
