/* Authored heatsink fields applied to a fresh WeaponHeat output copy.
 * Caller first runs the real getter and pins expected_base with the accepted
 * source/registry guards. This never edits a game-wide table or source row.
 * No hook installs this prototype yet. */
#ifndef HF_HEATSINK_OVERLAY_H
#define HF_HEATSINK_OVERLAY_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define HF_HEAT_BYTES 592u
#define HF_SICKLE_ENTITY UINT64_C(0x8645F167B3C813A2)
#define HF_DOUBLE_EDGE_ENTITY UINT64_C(0x295BEB26DC4F8FF1)
static const uint64_t HF_HEATSINK_KEYS[4]={
    UINT64_C(0x3C68A05CF9461E36),UINT64_C(0xA57D242D4552CBE3),
    UINT64_C(0x5637B16DBBE7CF51),UINT64_C(0x82022958FF0B89CD)
};
static void hf_heat_put(uint8_t* dst,size_t offset,float value) { memcpy(dst+offset,&value,4); }
/* 0: no private choice; 1..4: accepted choice; negative: refuse without editing. */
static int hf_heat_overlay(uint64_t base_entity,const uint64_t* deltas,uint32_t count,
                           const uint8_t* expected_base,uint8_t* output)
{
    int choice=0;
    if (count>10 || (!deltas && count)) return -1;
    for (uint32_t i=0;i<count;i++) for (unsigned j=0;j<4;j++) if (deltas[i]==HF_HEATSINK_KEYS[j]) {
        if (choice) return -2;
        choice=(int)j+1;
    }
    if (!choice) return 0;
    if (((choice<=2) && base_entity!=HF_SICKLE_ENTITY) ||
        ((choice>=3) && base_entity!=HF_DOUBLE_EDGE_ENTITY)) return -3;
    if (!expected_base || !output || memcmp(expected_base,output,HF_HEAT_BYTES)) return -4;
    if (choice==2) hf_heat_put(output,128,12.0f);
    if (choice==4) {
        hf_heat_put(output,96,150.0f);
        hf_heat_put(output,0,37.5f);hf_heat_put(output,24,75.0f);hf_heat_put(output,48,142.5f);
        hf_heat_put(output,104,60.0f);hf_heat_put(output,108,37.5f);
    }
    return choice;
}
#endif
