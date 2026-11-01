/* Incoming melee protection for the Helldiver wearing Rock-Solid.
 * Uses the SDK's pre-damage event; no passive values or enemy damage rows are written. */
#include "rock_solid_pins.generated.h"
typedef struct HfRockWitness {
    uintptr_t factory, avatars, custom, passive;
    uint32_t avatar_index, player, custom_index;
    uint8_t avatar[24], player_descriptor[24], customization[68], passive_row[52];
} HfRockWitness;
static uintptr_t HF_ROCK_GAME;
static int HF_ROCK_READY, HF_ROCK_STOPPED, HF_ROCK_QUEUED;
static float HF_ROCK_WAIT, HF_ROCK_POLL;
static uint64_t HF_ROCK_HITS;
static int hf_rock_needs_driver(void) { return HF_ROCK_QUEUED && !HF_ROCK_READY && !HF_ROCK_STOPPED; }
static void hf_rock_driver_failed(const HfApi* api,HfMod* mod) {
    if(hf_rock_needs_driver()) {
        HF_ROCK_STOPPED=1;
        api->log(mod,"Rock-Solid refused: shared pack game-thread driver unavailable; existing option retries retain their own refusal state");
    }
}

static int hf_rock_read(const HfApi* api,uintptr_t at,void* data,size_t size) {
    return api && api->read && hf_component_pointer(at,size) && !api->read(at,data,size);
}
static uint32_t hf_rock_u32(const void* data) { uint32_t v;memcpy(&v,data,4);return v; }
static uint64_t hf_rock_u64(const void* data) { uint64_t v;memcpy(&v,data,8);return v; }
static int hf_rock_map(const HfApi* api,uintptr_t at,uint32_t key,uint32_t limit,uint32_t* index) {
    uint8_t header[20],again[20];uint32_t pair[2];
    if(!hf_rock_read(api,at,header,sizeof header))return 0;
    uintptr_t slots=(uintptr_t)hf_rock_u64(header);uint32_t cap=hf_rock_u32(header+8),empty=hf_rock_u32(header+12),mult=hf_rock_u32(header+16);
    if(!cap || cap>limit || (cap&(cap-1u)) || key==empty || !hf_component_pointer(slots,(size_t)cap*8u))return 0;
    for(uint32_t i=0;i<cap && i<4096u;i++) {
        uint32_t slot=(key*mult+i)&(cap-1u);
        if(!hf_rock_read(api,slots+(size_t)slot*8u,pair,8u))return 0;
        if(pair[0]==key) {
            if(pair[1]==UINT32_MAX || !hf_rock_read(api,at,again,sizeof again) || memcmp(header,again,sizeof header))return 0;
            *index=pair[1];return 1;
        }
        if(pair[0]==empty)return 0;
    }
    return 0;
}
static int hf_rock_descriptor(const HfApi* api,uintptr_t factory,uint32_t entity,uint8_t out[24]) {
    uint32_t index;
    return hf_rock_map(api,factory+0xf1aeb0u,entity,1048576u,&index) && index<1048576u &&
        hf_rock_read(api,factory+0xf32f18u+(size_t)index*24u,out,24u) &&
        hf_rock_u32(out+8)==entity && (hf_rock_u32(out+20)&3u)==1u;
}
static int hf_rock_passive(const HfApi* api,uintptr_t custom,HfRockWitness* out) {
    uint32_t head[2],index;uintptr_t rows;
    if(!hf_rock_read(api,custom+0x20u,&rows,8u) || !hf_rock_read(api,custom+0x28u,head,8u) ||
       (head[0]!=32u && head[0]!=33u) || (head[1] && (head[1]<head[0] || head[1]>64u)))return 0;
    /* The existing Scope backport may append passive 22. Every id stays unique and indexed. */
    uint64_t seen=0;
    for(uint32_t i=0;i<head[0];i++) {
        uintptr_t row;uint32_t id,mapped;
        if(!hf_rock_read(api,rows+(size_t)i*8u,&row,8u) || !hf_rock_read(api,row,&id,4u) ||
           id>=42u || (seen&(UINT64_C(1)<<id)) || !hf_rock_read(api,custom+0x30u+id*4u,&mapped,4u) || mapped!=i ||
           (i==32u && id!=22u))return 0;
        seen|=UINT64_C(1)<<id;
    }
    if(!(seen&(UINT64_C(1)<<33)) || !hf_rock_read(api,custom+0xb4u,&index,4u) || index>=head[0] ||
       !hf_rock_read(api,rows+(size_t)index*8u,&out->passive,8u) ||
       !hf_rock_read(api,out->passive,out->passive_row,52u))return 0;
    const uint8_t* r=out->passive_row;
    return hf_rock_u32(r)==33u && hf_rock_u32(r+4)==0x0dd98619u &&
        hf_rock_u64(r+8)==UINT64_C(0x90502679ec8f7942) &&
        hf_rock_u32(r+24)==3u && hf_rock_u32(r+28)==0u &&
        hf_rock_u64(r+32)==0u && hf_rock_u32(r+40)==0u &&
        hf_rock_u32(r+44)==0u && hf_rock_u32(r+48)==0u;
}
static int hf_rock_wearer(const HfApi* api,uint32_t target,HfRockWitness* out) {
    uint32_t ai,goid,ni,ci,count;uintptr_t actor_descriptor,custom_descriptor;uint8_t actor_owner[12],custom_owner[12];
    memset(out,0,sizeof(*out));
    if(!hf_rock_read(api,HF_ROCK_GAME+0x346bf98u,&out->factory,8u) || !hf_component_pointer(out->factory,0x2732f18u) ||
       !hf_rock_descriptor(api,out->factory,target,out->avatar) ||
       hf_rock_u64(out->avatar)!=UINT64_C(0x4d1c334d294dfa97) ||
       !hf_rock_read(api,HF_ROCK_GAME+0x3326d20u,&out->avatars,8u) || !hf_component_pointer(out->avatars,0x5658a4u) ||
       !hf_rock_map(api,out->avatars+0xf8u,target,4096u,&ai) || ai>=1024u ||
       !hf_rock_read(api,out->avatars+0x110u+(size_t)ai*8u,&actor_descriptor,8u) ||
       !hf_rock_read(api,actor_descriptor,actor_owner,sizeof actor_owner) || hf_rock_u32(actor_owner+8)!=target ||
       !hf_rock_read(api,out->avatars+0x5478a4u+(size_t)ai*120u,&goid,4u) || goid==0x7fffu ||
       !hf_rock_map(api,out->factory+0xf22ec8u,goid,1048576u,&ni) || ni>=1048576u ||
       !hf_rock_read(api,out->factory+0xf32f20u+(size_t)ni*24u,&out->player,4u) ||
       !hf_rock_descriptor(api,out->factory,out->player,out->player_descriptor) ||
       !hf_rock_read(api,HF_ROCK_GAME+0x33264f8u,&out->custom,8u) || !hf_component_pointer(out->custom,0xb7cu) ||
       !hf_rock_map(api,out->custom+0x930u,out->player,64u,&ci) ||
       !hf_rock_read(api,out->custom+0x8e8u,&count,4u) || count>4u || ci>=count ||
       !hf_rock_read(api,out->custom+0x948u+(size_t)ci*8u,&custom_descriptor,8u) ||
       !hf_rock_read(api,custom_descriptor,custom_owner,sizeof custom_owner) || hf_rock_u32(custom_owner+8)!=out->player ||
       !hf_rock_read(api,out->custom+0x96cu+(size_t)ci*68u,out->customization,68u))return 0;
    out->avatar_index=ai;out->custom_index=ci;
    if(hf_rock_u32(out->customization+60u)!=33u && hf_rock_u32(out->customization+56u)!=33u)return 0;
    return hf_rock_passive(api,out->custom,out);
}
static void hf_rock_damage(HfMod* mod,const HfEventInfo* event,void* user) {
    const HfApi* api=(const HfApi*)user;
    if(!HF_ROCK_READY || HF_ROCK_STOPPED || !event || event->kind!=HF_ON_DAMAGE_PRE || event->size<sizeof(HfDamagePreEvent))return;
    HfDamagePreEvent* hit=(HfDamagePreEvent*)(uintptr_t)event;
    /* Native kind 6 is the ability/melee hit path (also the game's melee-kill classification). */
    if(hit->cancel || hit->damage.kind!=6u || hit->damage.amount<=0 || api->game_state(mod)!=HF_STATE_MISSION)return;
    HfRockWitness first,again;
    if(!hf_rock_wearer(api,hit->damage.target,&first) || !hf_rock_wearer(api,hit->damage.target,&again) || memcmp(&first,&again,sizeof first))return;
    int32_t before=hit->damage.amount;
    hit->damage.amount=before/2;
    HF_ROCK_HITS++;
    if(HF_ROCK_HITS<=8u || !(HF_ROCK_HITS%64u))
        api->log(mod,"Rock-Solid: incoming melee to Helldiver %u reduced %d -> %d; protected hits %llu",hit->damage.target,before,hit->damage.amount,(unsigned long long)HF_ROCK_HITS);
}
static void hf_rock_tick(HfMod* mod,float dt,void* user) {
    const HfApi* api=(const HfApi*)user;
    if(HF_ROCK_READY || HF_ROCK_STOPPED || !(dt>0.0f && dt<=10.0f))return;
    HF_ROCK_WAIT+=dt;HF_ROCK_POLL+=dt;if(HF_ROCK_POLL<0.25f)return;HF_ROCK_POLL=0.0f;
    size_t size=0;uintptr_t game=api->module_base("game.dll",&size);int matched=game && size>=0x346bfa0u;
    for(size_t i=0;matched && i<sizeof(HF_ROCK_PINS)/sizeof(HF_ROCK_PINS[0]);i++)
        matched=api->code_matches(game+HF_ROCK_PINS[i].rva,HF_ROCK_PINS[i].pattern);
    if(matched) { HF_ROCK_GAME=game;HF_ROCK_READY=1;api->log(mod,"Rock-Solid: Ready; incoming melee damage x0.5 for Helldiver wearers; existing passive bonuses retained"); }
    else if(HF_ROCK_WAIT>=60.0f) { HF_ROCK_STOPPED=1;api->log(mod,"Rock-Solid refused: build-25480438 melee/target/passive consumer code was not ready within 60 seconds; other mods continue"); }
}
static int hf_rock_begin(const HfApi* api,HfMod* mod) {
    if(api->size<offsetof(HfApi,code_matches)+sizeof(api->code_matches) || !api->read || !api->module_base ||
       !api->code_matches || !api->on_game_thread || !api->subscribe || !api->game_state) {
        api->log(mod,"Rock-Solid refused: required SDK13 pre-damage/readiness API unavailable");return 0;
    }
    if(!api->subscribe(mod,HF_ON_DAMAGE_PRE,hf_rock_damage,(void*)api,0)) {
        HF_ROCK_STOPPED=1;api->log(mod,"Rock-Solid refused: could not register damage callback; other mods continue");return 0;
    }
    HF_ROCK_QUEUED=1;
    api->log(mod,"Rock-Solid queued: 50%% incoming melee reduction for the Helldiver wearing the passive");return 1;
}
