/* Only the four authored, empty-price child definitions are eligible. Native
 * stock inventory and parent definitions are read-only to this helper. */
#ifndef HF_SICKLE_CATALOG_OWNERSHIP_H
#define HF_SICKLE_CATALOG_OWNERSHIP_H
#include "catalog_children.h"
static int hf_child_ownership(uint32_t choice,uint32_t index,const uint8_t actual[184],
    int parent_owned,int previously_granted,uint8_t desired[184]) {
    if (choice>=4 || !actual || !desired || index>=4096) return -1;
    uint8_t expected[184]={0},normalized[184];
    const HfPrivateChild* e=&HF_PRIVATE_CHILDREN[choice];
    hf_cat_put32(expected,index);hf_cat_put32(expected+4,hf_cat_child(e->id,e->weapon));
    hf_cat_put32(expected+8,e->id);hf_cat_put32(expected+12,2);hf_cat_put32(expected+24,0x7fffffffu);
    memcpy(normalized,actual,184);uint32_t state=hf_cat_u32(actual+20);
    if ((state!=0 && state!=1 && state!=2 && state!=4) ||
        previously_granted<0 || previously_granted>2) return -2;
    /* +181 is the native temporary-disable flag, NOT discovery. Never set or
     * clear it: a foreign/native disable must fail full-row validation. */
    memset(normalized+20,0,4);
    if (memcmp(normalized,expected,184)) return -3;
    memcpy(desired,actual,184);
    /* Native inventory finalization turns absent amount into state 1, owned
     * amount into state 2. A grant token stores the original state plus one. */
    if (parent_owned) { if (state<=1) hf_cat_put32(desired+20,2); }
    else if (previously_granted && state==2) { hf_cat_put32(desired+20,(uint32_t)previously_granted-1); }
    return memcmp(desired,actual,184)?1:0;
}
#endif
