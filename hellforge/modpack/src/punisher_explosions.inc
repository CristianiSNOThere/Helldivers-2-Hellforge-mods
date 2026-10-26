/* A mod-owned explosion table and two owned projectile edges. Stock explosion
 * data stays under HellForge's live-field API and is never edited here. */
#include "explosion_registry.h"
typedef struct HfPunisherExplosions {
    HfxPlan registry;
    uintptr_t projectiles[2];
    uint32_t ids[2];
    uint32_t linked;
    int poisoned;
} HfPunisherExplosions;
static HfPunisherExplosions HF_PUNISHER_EXPLOSIONS;
static int hf_explosion_read(void* context,uintptr_t at,void* bytes,size_t size) {
    return ((const HfApi*)context)->read(at,bytes,size);
}
static int hf_explosion_write(void* context,uintptr_t at,const void* bytes,size_t size) {
    return ((const HfApi*)context)->write(at,bytes,size);
}
static int hf_explosion_hash(void* context,const void* bytes,size_t size,const char* expected) {
    (void)context;return hf_punisher_hash_matches(bytes,size,expected);
}
static HfxIo hf_explosion_io(const HfApi* api) {
    HfxIo io={(void*)api,hf_explosion_read,hf_explosion_write,hf_explosion_hash};return io;
}
static int hf_punisher_explosion_pins_ready(const HfApi* api) {
    HfxIo io=hf_explosion_io(api);HfxPlan probe={0};
    if(!api || !api->read || !api->module_base)return 0;
    if(HF_PUNISHER_EXPLOSIONS.registry.state==2)
        return hfx_codes_match(&io,&HF_PUNISHER_EXPLOSIONS.registry,1);
    probe.game=api->module_base("game.dll",&probe.image_size);
    return probe.game && hfx_codes_match(&io,&probe,0);
}
#ifndef HF_PUNISHER_EXPLOSION_HOST_TEST
static uintptr_t hf_explosion_allocate(uintptr_t game,size_t size) {
    uintptr_t at=(game+size+65535u)&~(uintptr_t)65535u;
    uintptr_t end=game+(uintptr_t)INT32_MAX-65536u;
    while(at<end) {
        MEMORY_BASIC_INFORMATION region;
        if(!VirtualQuery((void*)at,&region,sizeof(region)))return 0;
        if(region.State==MEM_FREE) {
            uintptr_t allocation=(uintptr_t)VirtualAlloc((void*)at,sizeof(HfxBlock),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
            if(allocation)return allocation;
        }
        uintptr_t next=((uintptr_t)region.BaseAddress+region.RegionSize+65535u)&~(uintptr_t)65535u;
        if(next<=at)return 0;at=next;
    }
    return 0;
}
#endif

static int hf_punisher_explosion_edge(const HfApi* api,HfPunisherExplosions* state,
                                      unsigned index,uintptr_t* address,uint32_t* impact) {
    size_t size=0;uintptr_t game=api->module_base("game.dll",&size),table=0,slot=0;
    uint32_t id=0;uintptr_t row=api->projectile_row(state->ids[index]);
    if(!game || !row || row!=state->projectiles[index] ||
       !hf_registry_root(api,game,size,HF_REGISTRY_PROJECTILE,&table) ||
       api->read(table+(uintptr_t)state->ids[index]*8u,&slot,8) || slot!=row ||
       api->read(row,&id,4) || id!=state->ids[index] || api->read(row+144u,impact,4))return 0;
    *address=row+144u;return 1;
}

static int hf_punisher_explosion_reset(const HfApi* api,char* reason,size_t capacity) {
    HfPunisherExplosions* state=&HF_PUNISHER_EXPLOSIONS;int failed=0;
    if(state->linked && (!api || !api->read || !api->write || !api->module_base || !api->projectile_row)) {
        state->poisoned=1;(void)snprintf(reason,capacity,"private explosion reset API unavailable");return -1;
    }
    while(state->linked) {
        unsigned index=--state->linked;uintptr_t at;uint32_t current;
        uint32_t original=index?355u:290u,private_id=HFX_STOCK_SLOTS+index;
        if(!hf_punisher_explosion_edge(api,state,index,&at,&current)) {failed=1;continue;}
        if(current==original)continue;
        if(current!=private_id){failed=1;continue;}
        (void)api->write(at,&original,4);
        if(api->read(at,&current,4) || current!=original)failed=1;
    }
    /* The completed table reservation and its storage live until process exit.
     * Reload can free the old stock pointer list, so it must not be restored. */
    if(failed){state->poisoned=1;(void)snprintf(reason,capacity,"private explosion projectile edge changed or failed to restore");}
    return failed?-1:1;
}

static int hf_punisher_explosion_check(const HfApi* api,char* reason,size_t capacity) {
    HfPunisherExplosions* state=&HF_PUNISHER_EXPLOSIONS;HfxIo io=hf_explosion_io(api);
    if(!api || !api->read || !api->module_base || !api->projectile_row ||
       state->poisoned || state->linked!=2u || !hfx_owned(&io,&state->registry))goto bad;
    for(unsigned i=0;i<2;i++) {
        uintptr_t at;uint32_t value;
        if(!hf_punisher_explosion_edge(api,state,i,&at,&value) || value!=HFX_STOCK_SLOTS+i)goto bad;
    }
    return 1;
bad:(void)snprintf(reason,capacity,"private explosion registry, row or projectile ownership changed");return 0;
}

static int hf_punisher_explosion_bind(const HfApi* api,const HfPunisherContent* content,
                                     char* reason,size_t capacity) {
    HfPunisherExplosions* state=&HF_PUNISHER_EXPLOSIONS;HfxIo io=hf_explosion_io(api);
    size_t size=0;uintptr_t game=0;
    if(!api || !api->read || !api->write || !api->module_base || !api->projectile_row || !content ||
       content->normal_projectile<351u || content->normal_projectile>414u ||
       content->pellet_projectile<351u || content->pellet_projectile>414u ||
       content->normal_projectile==content->pellet_projectile)goto refused;
    game=api->module_base("game.dll",&size);
    if(state->poisoned || state->registry.state<0)goto refused;
    if(state->linked)return hf_punisher_explosion_check(api,reason,capacity);
    state->ids[0]=content->normal_projectile;state->ids[1]=content->pellet_projectile;
    state->projectiles[0]=api->projectile_row(state->ids[0]);
    state->projectiles[1]=api->projectile_row(state->ids[1]);
    if(!game || !state->projectiles[0] || !state->projectiles[1] ||
       state->projectiles[0]==state->projectiles[1])goto refused;
    /* All owned SDK row hashes were verified immediately before this call. */
    for(unsigned i=0;i<2;i++) {
        uintptr_t at;uint32_t current;
        if(!hf_punisher_explosion_edge(api,state,i,&at,&current) || current!=(i?355u:290u))goto refused;
    }
    if(state->registry.state==0) {
        uintptr_t allocation=hf_explosion_allocate(game,size);
        if(!allocation || !hfx_prepare(&io,&state->registry,game,size,allocation,
                content->normal_damage,content->pellet_damage) || !hfx_commit(&io,&state->registry))goto refused;
    }
    if(!hfx_owned(&io,&state->registry))goto refused;
    for(unsigned i=0;i<2;i++) {
        uintptr_t at;uint32_t current,value=HFX_STOCK_SLOTS+i,original=i?355u:290u;
        if(!hf_punisher_explosion_edge(api,state,i,&at,&current) || current!=original)goto rollback;
        state->linked=i+1u;
        if(api->write(at,&value,4) || !hf_punisher_explosion_edge(api,state,i,&at,&current) || current!=value)goto rollback;
    }
    if(hf_punisher_explosion_check(api,reason,capacity))return 1;
rollback:
    if(hf_punisher_explosion_reset(api,reason,capacity)<0)return -1;
refused:
    (void)snprintf(reason,capacity,"private explosion lookup, donor, allocation or projectile guard refused");
    return state->poisoned || state->registry.state<0 ? -1:0;
}
