#ifndef HF_HEATSINK_PEER_TEXT_GUARD_H
#define HF_HEATSINK_PEER_TEXT_GUARD_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Reviewed Scope of Authority 2.14.15 text dispatcher built by SDK13. */
static int hf_peer_text_source(uintptr_t current,uintptr_t base,size_t size,const char* hash) {
    return base>=65536u && base<UINT64_C(0x800000000000)-0x7df0u &&
        size>0x7df0u && size<UINT64_C(0x800000000000)-base && current==base+0x7dd0u && hash &&
        !strcmp(hash,"550C9978C4F25E582CD84F7DA7A9E14A258B959B992655A657143B3502E75992");
}
#endif
