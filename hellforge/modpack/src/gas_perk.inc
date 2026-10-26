/* Native fallback for the reviewed Advanced Filtration row when a peer extends
 * the passive catalogue beyond the SDK's fixed 32-row reader. */
#include "gas_perk_pins.generated.h"
#include "data_transaction.h"
typedef struct HfGasPerkPlan {
    uintptr_t manager, pointers, row, entry;
    uint32_t count, slot;
    uint8_t identity[52], original[16], expected[16];
    int ready, applied;
} HfGasPerkPlan;
static HfGasPerkPlan HF_GAS_PERK;
static int hf_gas_perk_field(const HfPortEdit* edit) {
    return edit && edit->kind==PORT_EDIT_LIVE && edit->option && !strcmp(edit->option,"gas-overhaul") &&
        edit->target==208u && edit->field.component && !strcmp(edit->field.component,"PassiveModifier") &&
        edit->field.layout==25480438u && edit->field.offset==8u && edit->field.size==4u &&
        edit->field.type==HF_TYPE_F32 && edit->value.type==HF_TYPE_F32 && (float)edit->value.number==0.0f &&
        edit->expected_base.type==HF_TYPE_F32 && (float)edit->expected_base.number==0.2f &&
        edit->has_expected_base && edit->flags==HF_LIVE_SHARED;
}
static uint32_t hf_gas_perk_u32(const uint8_t* p) {uint32_t v;memcpy(&v,p,4);return v;}
static uint64_t hf_gas_perk_u64(const uint8_t* p) {uint64_t v;memcpy(&v,p,8);return v;}
static int hf_gas_perk_write(void* context,uintptr_t at,const void* bytes,size_t size) {
    const HfApi* api=context;return api->write(at,bytes,size);
}
static int hf_gas_perk_resolve(const HfApi* api,HfGasPerkPlan* out) {
    uintptr_t game,manager,pointers,row,entry;size_t image=0;
    uint32_t head[2],slot;uint8_t identity[52],item[16];
    if (!api || !api->module_base || !api->read || !(game=api->module_base("game.dll",&image)) ||
        image<0x3326500u) return 0;
    for (size_t i=0;i<sizeof(HF_GAS_PERK_PINS)/sizeof(HF_GAS_PERK_PINS[0]);i++) {
        const HfGasPerkPin* p=&HF_GAS_PERK_PINS[i];uint8_t bytes[16];
        if (p->rva>image || p->size>image-p->rva || api->read(game+p->rva,bytes,p->size) || memcmp(bytes,p->bytes,p->size)) return 0;
    }
    if (api->read(game+0x33264f8u,&manager,8) || !hf_component_pointer(manager,0xd8u) ||
        api->read(manager+0x20u,&pointers,8) || api->read(manager+0x28u,head,8) ||
        (head[0]!=32u && head[0]!=33u) || (head[1] && (head[1]<head[0] || head[1]>64u)) ||
        !hf_component_pointer(pointers,(size_t)head[0]*8u) || api->read(manager+0x64u,&slot,4) || slot>=32u ||
        api->read(pointers+(size_t)slot*8u,&row,8) || !hf_component_pointer(row,52u) || api->read(row,identity,52u)) return 0;
    /* Only the peer's appended passive is accepted: existing IDs remain unique
     * and correctly indexed, including Advanced Filtration in its stock slot. */
    uint64_t seen=0;
    for (uint32_t i=0;i<head[0];i++) {
        uintptr_t p;uint32_t id,mapped;
        if (api->read(pointers+(size_t)i*8u,&p,8) || !hf_component_pointer(p,52u) || api->read(p,&id,4) ||
            id>=42u || (seen&(UINT64_C(1)<<id)) || api->read(manager+0x30u+id*4u,&mapped,4) || mapped!=i ||
            (i==32u && id!=22u)) return 0;
        seen|=UINT64_C(1)<<id;
    }
    if (hf_gas_perk_u32(identity)!=13u || hf_gas_perk_u32(identity+4)!=0x7cefecb4u ||
        hf_gas_perk_u64(identity+8)!=UINT64_C(0x501271ef8b35994e) ||
        hf_gas_perk_u32(identity+24)!=1u || hf_gas_perk_u32(identity+28) ||
        hf_gas_perk_u64(identity+32) || hf_gas_perk_u64(identity+40) || hf_gas_perk_u32(identity+48) ||
        !(entry=(uintptr_t)hf_gas_perk_u64(identity+16)) || !hf_component_pointer(entry,16u) ||
        api->read(entry,item,16u) || hf_gas_perk_u32(item)!=0x6e99cce5u || hf_gas_perk_u32(item+4)!=2u) return 0;
    memset(out,0,sizeof(*out));out->manager=manager;out->pointers=pointers;out->row=row;out->entry=entry;
    out->count=head[0];out->slot=slot;memcpy(out->identity,identity,52u);memcpy(out->original,item,16u);
    return 1;
}
static int hf_gas_perk_same(const HfGasPerkPlan* a,const HfGasPerkPlan* b) {
    return a->manager==b->manager && a->pointers==b->pointers && a->row==b->row && a->entry==b->entry &&
        a->count==b->count && a->slot==b->slot && !memcmp(a->identity,b->identity,52u);
}
static int hf_gas_perk_preflight(const HfApi* api,char* reason,size_t capacity) {
    float value;HfGasPerkPlan p;
    memset(&HF_GAS_PERK,0,sizeof(HF_GAS_PERK));
    if (!hf_gas_perk_resolve(api,&p)) goto refuse;
    memcpy(&value,p.original+8,4);if (value!=0.2f) goto refuse;
    p.ready=1;memcpy(p.expected,p.original,16u);memset(p.expected+8,0,4);HF_GAS_PERK=p;return 1;
refuse:
    (void)snprintf(reason,capacity,"Advanced Filtration's native passive table, consumer pins or exact row changed");return 0;
}
static int hf_gas_perk_change(const HfApi* api,int reset,char* reason,size_t capacity) {
    HfGasPerkPlan p;HfGasPerkPlan* s=&HF_GAS_PERK;
    const uint8_t* before=reset?s->expected:s->original;const uint8_t* after=reset?s->original:s->expected;
    if (!s->ready || !api->write || !hf_gas_perk_resolve(api,&p) || !hf_gas_perk_same(s,&p)) goto refuse;
    if (reset && !s->applied) return !memcmp(p.original,s->original,16u);
    if (memcmp(p.original,before,16u)) goto refuse;
    int result=hf_row_write_if(hf_component_api_read,hf_gas_perk_write,(void*)api,s->entry+8u,before+8u,after+8u,4u);
    if (result!=1) {
        (void)snprintf(reason,capacity,result<0?"FATAL: Advanced Filtration write could not be repaired":"Advanced Filtration write failed and was rolled back");return 0;
    }
    if (!hf_gas_perk_resolve(api,&p) || !hf_gas_perk_same(s,&p)) {
        (void)snprintf(reason,capacity,"FATAL: Advanced Filtration ownership changed during its write");return 0;
    }
    if (memcmp(p.original,after,16u)) {
        if (!hf_row_undo_owned(hf_component_api_read,hf_gas_perk_write,(void*)api,s->entry+8u,before+8u,after+8u,4u)) {
            (void)snprintf(reason,capacity,"FATAL: Advanced Filtration rollback failed");return 0;
        }
        goto refuse;
    }
    s->applied=!reset;return 1;
refuse:
    (void)snprintf(reason,capacity,"Advanced Filtration's active row or ownership changed after preflight");return 0;
}
