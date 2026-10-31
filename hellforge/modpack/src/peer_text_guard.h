#ifndef HF_HEATSINK_PEER_TEXT_GUARD_H
#define HF_HEATSINK_PEER_TEXT_GUARD_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Exact reviewed Scope dispatchers; each version keeps the previous text
 * callback for unknown keys. A new build needs its own identity review. */
static int hf_peer_text_source(uintptr_t current,uintptr_t base,size_t size,const char* hash) {
    if(base<65536u || base>=UINT64_C(0x800000000000) || size>=UINT64_C(0x800000000000)-base || !hash)return 0;
    if(size>0x7df0u && current==base+0x7dd0u &&
       !strcmp(hash,"550C9978C4F25E582CD84F7DA7A9E14A258B959B992655A657143B3502E75992"))return 1;
    return size>0x8170u && current==base+0x8130u &&
       !strcmp(hash,"31580982DB764C70E98273670183B1A5E48BE18EE9A35D7687B907E06E59A1F3");
}
#endif
