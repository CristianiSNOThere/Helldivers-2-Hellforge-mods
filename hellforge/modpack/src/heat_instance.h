/* Prepare a guarded per-entity heat transition and retain explicit provenance.
 * The adapter supplies a freshly resolved native instance, never a guessed
 * capacity match. Bindings expire with the entity-data epoch/heat manager. */
#ifndef HF_SICKLE_HEAT_INSTANCE_H
#define HF_SICKLE_HEAT_INSTANCE_H
#include <math.h>
#include "heatsink_overlay.h"
typedef struct HfHeatBinding {
    uint64_t epoch,manager,resource;
    uint32_t entity,choice;
    uint8_t profile[HF_HEAT_BYTES];
} HfHeatBinding;
static int hf_heat_prepare(const HfHeatBinding* previous,uint64_t epoch,uint64_t manager,uint32_t entity,
    uint64_t resource,const uint64_t* deltas,uint32_t count,const uint8_t* expected_base,
    const uint8_t* actual,uint8_t* desired,HfHeatBinding* next) {
    uint8_t work[HF_HEAT_BYTES];
    if (!expected_base || !actual || !desired || !next || !epoch || !manager || !entity) return -5;
    int owned=previous && previous->epoch==epoch && previous->manager==manager && previous->entity==entity &&
        previous->resource==resource && previous->choice>=1 && previous->choice<=4 &&
        !memcmp(previous->profile,actual,HF_HEAT_BYTES);
    /* A previous owned output may be restored or switched; any unrelated edit
     * to that output loses provenance and must not be normalized away. */
    if (memcmp(expected_base,actual,HF_HEAT_BYTES) && !owned) return -4;
    memcpy(work,expected_base,HF_HEAT_BYTES);
    int choice=hf_heat_overlay(resource,deltas,count,expected_base,work);
    if (choice<0) return choice;
    if (!choice && !owned) return 0;
    memset(next,0,sizeof(*next));memcpy(desired,work,HF_HEAT_BYTES);
    if (choice) {
        next->epoch=epoch;next->manager=manager;next->resource=resource;next->entity=entity;next->choice=(uint32_t)choice;
        memcpy(next->profile,work,HF_HEAT_BYTES);
    }
    return choice?choice:5; /* 5: restore the prior owned choice to base */
}
/* The adapter resolves held from this exact target's wielder and validates its
 * live engine generation, descriptor resource, and current native heat row.
 * DamageInfo is separately guarded as the accepted port's 542..544 records.
 * The native HitInfo damage at +0x98 already blends regular/durable; its armour
 * penetration and all later game damage modifiers are left intact. */
static int hf_heat_damage(const HfHeatBinding* binding,uint64_t epoch,uint64_t manager,uint32_t target,
    uint32_t held,uint64_t resource,int held_alive,const uint8_t* current_heat,uint32_t damage_id,
    uint8_t* hit,size_t hit_size) {
    if (!binding || !epoch || !target || target==held || !held_alive || resource!=HF_DOUBLE_EDGE_ENTITY ||
        binding->epoch!=epoch || binding->manager!=manager || binding->entity!=held || binding->resource!=resource ||
        binding->choice!=4 || damage_id<542 || damage_id>544 || !current_heat ||
        memcmp(binding->profile,current_heat,HF_HEAT_BYTES) || !hit || hit_size<0x9c) return 0;
    float damage;memcpy(&damage,hit+0x98,4);
    if (!isfinite(damage) || damage<0.0f) return 0;
    damage*=2.0f/3.0f;memcpy(hit+0x98,&damage,4);return 1;
}
#endif
