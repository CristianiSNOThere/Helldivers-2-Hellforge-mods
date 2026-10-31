/* Primary-only damage ownership and the shared private explosion reservation. */
#if HF_PORT_CQC_20_BREACHING_HAMMER && HF_PORT_ALLOW_NATIVE_RUNTIME
#include "hammer_combat_guard.generated.h"
#if !HF_PORT_PUNISHER_PLASMA
#include "explosion_registry.h"
static HfxPlan HF_HAMMER_EXPLOSIONS;
#define HF_HAMMER_EXPLOSION_PLAN (&HF_HAMMER_EXPLOSIONS)
#else
#define HF_HAMMER_EXPLOSION_PLAN (&HF_PUNISHER_EXPLOSIONS.registry)
#endif
static const HfApi* HF_HAMMER_API;
static HfMod* HF_HAMMER_MOD;
static uint32_t HF_HAMMER_DAMAGE_IDS[2];
static uintptr_t HF_HAMMER_DAMAGE_ROWS[2];
static uint8_t HF_HAMMER_DAMAGE_AFTER[2][76];
static int HF_HAMMER_PROFILE_READY,HF_HAMMER_PROFILE_POISONED;
static const char* const HF_HAMMER_DAMAGE_NAMES[]={"superearth.arsenal-native-port/primary-hammer/strike","superearth.arsenal-native-port/primary-hammer/blast"};
static int hf_hammer_read(void* context,uintptr_t at,void* out,size_t n) {return ((const HfApi*)context)->read(at,out,n);}
static int hf_hammer_write(void* context,uintptr_t at,const void* in,size_t n) {return ((const HfApi*)context)->write(at,in,n);}
static int hf_hammer_hash(void* context,const void* in,size_t n,const char* expected) {
    HfSickleCrypto crypto={0};char hash[65];(void)context;
    return sickle_crypto_init(&crypto) && sickle_hash_bytes(&crypto,in,n,hash) && !strcmp(hash,expected);
}
static HfxIo hf_hammer_io(const HfApi* api) {HfxIo io={(void*)api,hf_hammer_read,hf_hammer_write,hf_hammer_hash};return io;}
static int hf_hammer_register_combat(const HfApi* api,HfMod* mod) {
    if(!api || !api->add_damage || !api->damage_row || !api->content || !api->read || !api->write)return 0;
    HF_HAMMER_API=api;HF_HAMMER_MOD=mod;
    for(unsigned i=0;i<2;i++) {
        HfDamage d={0};d.size=sizeof(d);d.base=i?562:560;d.name=HF_HAMMER_DAMAGE_NAMES[i];
        HF_HAMMER_DAMAGE_IDS[i]=api->add_damage(mod,&d);
        if(HF_HAMMER_DAMAGE_IDS[i]<650 || HF_HAMMER_DAMAGE_IDS[i]>713 || (i&&HF_HAMMER_DAMAGE_IDS[0]==HF_HAMMER_DAMAGE_IDS[1]))return 0;
    }return 1;
}
static int hf_hammer_damage_owned(unsigned which,uintptr_t* out) {
    const HfApi* api=HF_HAMMER_API;size_t size=0;uintptr_t root=0,row,pointers[714];unsigned owners=0,ended=0;
    if(which>1 || !api || HF_HAMMER_PROFILE_POISONED || !HF_HAMMER_DAMAGE_IDS[which])return 0;
    uintptr_t game=api->module_base("game.dll",&size);row=api->damage_row(HF_HAMMER_DAMAGE_IDS[which]);
    if(!game || !row || !hf_registry_root(api,game,size,HF_REGISTRY_DAMAGE,&root) || api->read(root,pointers,sizeof(pointers)) || pointers[HF_HAMMER_DAMAGE_IDS[which]]!=row)return 0;
    for(unsigned i=0;i<714;i++)if(i!=HF_HAMMER_DAMAGE_IDS[which]&&pointers[i]==row)return 0;
    for(unsigned i=0;i<4096;i++) {
        HfContent c={0};c.size=sizeof(c);if(!api->content(HF_HAMMER_MOD,i,&c)){ended=1;break;}
        if(c.kind!=HF_KIND_DAMAGE || c.id!=HF_HAMMER_DAMAGE_IDS[which])continue;
        if(!c.mod || strcmp(c.mod,"superearth.arsenal-native-port") || !c.name || strcmp(c.name,HF_HAMMER_DAMAGE_NAMES[which]) || c.base!=(which?562u:560u))return 0;owners++;
    }
    if(!ended || owners!=1)return 0;*out=row;return 1;
}
static int hf_hammer_profiles(void) {
    const HfApi* api=HF_HAMMER_API;uintptr_t rows[2];uint8_t before[2][76],after[2][76],actual[76];unsigned attempted=0;
    if(!api || HF_HAMMER_PROFILE_POISONED)return 0;
    if(HF_HAMMER_PROFILE_READY) {
        for(unsigned i=0;i<2;i++)if(!hf_hammer_damage_owned(i,&rows[i]) || rows[i]!=HF_HAMMER_DAMAGE_ROWS[i] || api->read(rows[i],actual,76) || memcmp(actual,HF_HAMMER_DAMAGE_AFTER[i],76))return 0;
        return 1;
    }
    for(unsigned i=0;i<2;i++) {
        const uint8_t* expected=i?HF_HAMMER_DAMAGE_562:HF_HAMMER_DAMAGE_560;uint8_t normalized[76];
        if(!hf_hammer_damage_owned(i,&rows[i]) || api->read(rows[i],before[i],76))return 0;
        memcpy(normalized,before[i],76);memcpy(normalized,expected,4);
        /* Registration can precede the accepted pack's AP7 edit. Recognize
           only the complete captured stock AP6 row or complete accepted AP7
           row, including every non-AP byte. Mixed/unknown values refuse. */
        if(memcmp(normalized,expected,76) &&
           (!i || memcmp(normalized,HF_HAMMER_DAMAGE_562_STOCK,76)))return 0;
        memcpy(after[i],before[i],76);uint32_t demolition=50;memcpy(after[i]+28,&demolition,4);
        if(i)memcpy(after[i]+12,expected+12,12);
    }
    for(unsigned i=0;i<2;i++) {
        uintptr_t current=0;
        if(!hf_hammer_damage_owned(i,&current) || current!=rows[i] || api->read(rows[i],actual,76) || memcmp(actual,before[i],76))goto rollback;
        attempted=i+1;
        if(api->write(rows[i],after[i],76) || api->read(rows[i],actual,76) || memcmp(actual,after[i],76))goto rollback;
    }
    for(unsigned i=0;i<2;i++){HF_HAMMER_DAMAGE_ROWS[i]=rows[i];memcpy(HF_HAMMER_DAMAGE_AFTER[i],after[i],76);}
    HF_HAMMER_PROFILE_READY=1;return 1;
rollback:
    while(attempted) {
        unsigned i=--attempted;uintptr_t current=0;
        if(!hf_hammer_damage_owned(i,&current) || current!=rows[i] || api->read(rows[i],actual,76)){HF_HAMMER_PROFILE_POISONED=1;continue;}
        unsigned n;for(n=0;n<76;n++)if(actual[n]!=before[i][n] && actual[n]!=after[i][n])break;
        if(n!=76 || api->write(rows[i],before[i],76) || api->read(rows[i],actual,76) || memcmp(actual,before[i],76))HF_HAMMER_PROFILE_POISONED=1;
    }return 0;
}
static int hf_hammer_blast_append(const HfApi* api,HfxPlan* plan) {
    HfxIo io=hf_hammer_io(api);return hf_hammer_profiles() && hfx_add_hammer(&io,plan,HF_HAMMER_DAMAGE_IDS[1]);
}
#if !HF_PORT_PUNISHER_PLASMA
static uintptr_t hf_hammer_allocate(uintptr_t game,size_t image) {
    uintptr_t at=(game+image+65535u)&~(uintptr_t)65535u,end=game+INT32_MAX-65536u;
    while(at<end) {
        MEMORY_BASIC_INFORMATION region;if(!VirtualQuery((void*)at,&region,sizeof(region)))return 0;
        if(region.State==MEM_FREE){uintptr_t p=(uintptr_t)VirtualAlloc((void*)at,sizeof(HfxBlock),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(p)return p;}
        uintptr_t next=((uintptr_t)region.BaseAddress+region.RegionSize+65535u)&~(uintptr_t)65535u;if(next<=at)return 0;at=next;
    }return 0;
}
#endif
int hf_primary_combat_bind(void) {
    const HfApi* api=HF_HAMMER_API;HfxPlan* plan=HF_HAMMER_EXPLOSION_PLAN;
    if(!hf_hammer_profiles())return 0;
#if !HF_PORT_PUNISHER_PLASMA
    if(!plan->state) {
        size_t size=0;uintptr_t game=api->module_base("game.dll",&size),allocation=hf_hammer_allocate(game,size);HfxIo io=hf_hammer_io(api);
        if(!allocation || !hfx_prepare(&io,plan,game,size,allocation,0,0) || !hf_hammer_blast_append(api,plan) || !hfx_commit(&io,plan))return 0;
    }
#endif
    HfxIo io=hf_hammer_io(api);return hfx_owned(&io,plan);
}
uint32_t hf_primary_strike_damage(void) {
    uintptr_t row=0;uint8_t bytes[76];
    return HF_HAMMER_PROFILE_READY && hf_hammer_damage_owned(0,&row) && row==HF_HAMMER_DAMAGE_ROWS[0] && !HF_HAMMER_API->read(row,bytes,76) && !memcmp(bytes,HF_HAMMER_DAMAGE_AFTER[0],76)?HF_HAMMER_DAMAGE_IDS[0]:0;
}
uint32_t hf_primary_blast_type(void) {
    HfxPlan* plan=HF_HAMMER_EXPLOSION_PLAN;HfxIo io=hf_hammer_io(HF_HAMMER_API);HfxBlock actual;uintptr_t row=0;uint8_t bytes[76];
    if(!HF_HAMMER_PROFILE_READY || !hf_hammer_damage_owned(1,&row) || row!=HF_HAMMER_DAMAGE_ROWS[1] || HF_HAMMER_API->read(row,bytes,76) || memcmp(bytes,HF_HAMMER_DAMAGE_AFTER[1],76) ||
       plan->state!=2 || !hfx_codes_match(&io,plan,1) || !hfx_read(&io,plan->allocation,&actual,sizeof(actual)) ||
       memcmp(&actual.slots[HFX_STOCK_SLOTS],&plan->desired.slots[HFX_STOCK_SLOTS],sizeof(actual)-HFX_STOCK_SLOTS*8u))return 0;
    return 425;
}
#else
int hf_primary_combat_bind(void){return 0;}
uint32_t hf_primary_strike_damage(void){return 0;}
uint32_t hf_primary_blast_type(void){return 0;}
#endif
