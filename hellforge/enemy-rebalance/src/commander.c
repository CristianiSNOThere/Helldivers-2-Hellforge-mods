#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "hellforge_mod.h"

#define ALPHA_TABLE_RVA UINT32_C(0x21326A0)
#define ALPHA_SUMMON_RVA UINT32_C(0x2132B20)
#define ALPHA_SUMMON_ROW_SIZE 96u
#define ALPHA_SUMMON_BOUNDS_OFFSET 36u
#define ALPHA_SUMMON_ABILITY 463u

typedef struct { uint32_t rva; size_t size; const char *pattern; } CodeGuard;

/* These guards identify Behavior 139's Alpha action table, its summon row,
 * and the native timer consumer traced for the supported 25480438 image. */
static const CodeGuard code_guards[] = {
    {UINT32_C(0x4A1CEF), 14u, "4D 8B C7 49 8B D6 48 8B CE E8 53 79 D9 FF"},
    {UINT32_C(0x239650), 16u, "48 8B C4 48 89 58 20 55 56 57 41 54 41 55 41 56"},
    {UINT32_C(0x2395D0), 16u, "48 89 5C 24 08 57 48 83 EC 20 48 8B 41 08 8B DA"},
    {UINT32_C(0x23A9D2), 7u, "48 8D 05 C7 7C EF 01"},
    {UINT32_C(0xAA42D0), 14u, "40 55 56 57 41 54 41 55 41 57 48 8D AC 24"},
    {UINT32_C(0xAA7197), 17u, "F3 41 0F 10 4C 24 24 0F 57 D2 48 8B 05 90 CA 9D 02"}
};

/* Complete stock Alpha Commander action 463 row at game.dll+0x2132B20.
 * Only the four timer bounds at +36/+40/+44/+48 are changed. */
static const uint32_t alpha_summon_before[24] = {
    0x000001CFu,0x00000000u,0x00000000u,0x41200000u,
    0x428C0000u,0x00000000u,0x41A00000u,0x00000000u,
    0x00000000u,0x41700000u,0x41A00000u,0x41700000u,
    0x41A00000u,0xBF800000u,0x00000000u,0x00000000u,
    0x00000000u,0x00000000u,0x40E00000u,0x40E00000u,
    0x41F00000u,0x00000007u,0x00000000u,0x00000000u
};

/* 15/20-second stock bounds become 35/45 seconds in both timer ranges. */
static const uint32_t alpha_summon_after[24] = {
    0x000001CFu,0x00000000u,0x00000000u,0x41200000u,
    0x428C0000u,0x00000000u,0x41A00000u,0x00000000u,
    0x00000000u,0x420C0000u,0x42340000u,0x420C0000u,
    0x42340000u,0xBF800000u,0x00000000u,0x00000000u,
    0x00000000u,0x00000000u,0x40E00000u,0x40E00000u,
    0x41F00000u,0x00000007u,0x00000000u,0x00000000u
};

static const HfApi *api;
static HfMod *self;
static uintptr_t applied_image;
static uintptr_t applied_row;

static int image_range(uintptr_t image, size_t image_size, uint32_t rva, size_t size)
{
    if ((size_t)rva > image_size || size > image_size-(size_t)rva) return 0;
    if (image > UINTPTR_MAX-(uintptr_t)rva || image+(uintptr_t)rva > UINTPTR_MAX-size) return 0;
    return 1;
}

static int verify_code(uintptr_t image, size_t image_size)
{
    for (size_t i=0;i<sizeof(code_guards)/sizeof(code_guards[0]);++i) {
        const CodeGuard *g=&code_guards[i];
        if (!image_range(image,image_size,g->rva,g->size) ||
            !api->code_matches(image+(uintptr_t)g->rva,g->pattern)) {
            api->log(self,"COMMANDER REFUSED: native code signature mismatch at RVA 0x%X",g->rva);
            return 0;
        }
    }
    return 1;
}

static int current_image(uintptr_t *image, size_t *image_size)
{
    const char *build=api->build_id();
    if (!build || strcmp(build,"25480438")) {
        api->log(self,"COMMANDER REFUSED: supported game build is 25480438");
        return 0;
    }
    *image_size=0;
    *image=api->module_base("game.dll",image_size);
    if (!*image || !image_range(*image,*image_size,ALPHA_SUMMON_RVA,ALPHA_SUMMON_ROW_SIZE) ||
        !image_range(*image,*image_size,ALPHA_TABLE_RVA,13u*ALPHA_SUMMON_ROW_SIZE)) {
        api->log(self,"COMMANDER REFUSED: game.dll image or Alpha action table is unavailable");
        return 0;
    }
    return verify_code(*image,*image_size);
}

static int rollback_bounds(uintptr_t row)
{
    uint8_t now[ALPHA_SUMMON_ROW_SIZE];
    if (api->read(row,now,sizeof now)) return 0;
    if (memcmp(now,alpha_summon_before,sizeof now)==0) return 1;
    if (memcmp(now,alpha_summon_before,ALPHA_SUMMON_BOUNDS_OFFSET)!=0 ||
        memcmp(now+ALPHA_SUMMON_BOUNDS_OFFSET+16u,
               (const uint8_t *)alpha_summon_before+ALPHA_SUMMON_BOUNDS_OFFSET+16u,
               sizeof now-ALPHA_SUMMON_BOUNDS_OFFSET-16u)!=0) return 0;

    /* Roll back partial writes only when every target word is still exactly
     * our old or new value. Any third value or unrelated edit is preserved. */
    for (unsigned i=0;i<4;++i) {
        size_t offset=ALPHA_SUMMON_BOUNDS_OFFSET+i*4u;
        uint32_t value=0;
        memcpy(&value,now+offset,sizeof value);
        if (value!=alpha_summon_before[9+i] && value!=alpha_summon_after[9+i]) return 0;
    }
    uintptr_t image=0;
    size_t image_size=0;
    if (!current_image(&image,&image_size) || row!=image+(uintptr_t)ALPHA_SUMMON_RVA) return 0;
    for (unsigned i=0;i<4;++i) {
        size_t offset=ALPHA_SUMMON_BOUNDS_OFFSET+i*4u;
        uint32_t value=0;
        if (api->read(row,now,sizeof now)) return 0;
        memcpy(&value,now+offset,sizeof value);
        if (value==alpha_summon_before[9+i]) continue;
        if (value!=alpha_summon_after[9+i] ||
            memcmp(now,alpha_summon_before,ALPHA_SUMMON_BOUNDS_OFFSET)!=0 ||
            memcmp(now+ALPHA_SUMMON_BOUNDS_OFFSET+16u,
                   (const uint8_t *)alpha_summon_before+ALPHA_SUMMON_BOUNDS_OFFSET+16u,
                   sizeof now-ALPHA_SUMMON_BOUNDS_OFFSET-16u)!=0 ||
            !current_image(&image,&image_size) || row!=image+(uintptr_t)ALPHA_SUMMON_RVA) return 0;
        uint32_t before=alpha_summon_before[9+i];
        if (api->write(row+offset,&before,sizeof before) ||
            api->read(row+offset,&value,sizeof value) || value!=before) return 0;
    }
    return api->read(row,now,sizeof now)==0 &&
           memcmp(now,alpha_summon_before,sizeof now)==0;
}

static void commander_ready(HfMod *mod, HfEvent event, void *user)
{
    (void)mod; (void)user;
    if (event!=HF_EVENT_ENTITIES_READY) return;

    uintptr_t image=0;
    size_t image_size=0;
    if (!current_image(&image,&image_size)) return;
    uintptr_t row=image+(uintptr_t)ALPHA_SUMMON_RVA;
    uint8_t current[ALPHA_SUMMON_ROW_SIZE];
    if (api->read(row,current,sizeof current)) {
        api->log(self,"COMMANDER REFUSED: could not read the Alpha summon profile");
        return;
    }
    if (memcmp(current,alpha_summon_after,sizeof current)==0) {
        if (applied_image==image && applied_row==row) {
            api->log(self,"COMMANDER ALREADY_APPLIED: Alpha summon cooldown remains 35-45 seconds");
        } else {
            api->log(self,"COMMANDER REFUSED: Alpha summon profile was already changed by another owner");
        }
        return;
    }
    if (memcmp(current,alpha_summon_before,sizeof current)!=0) {
        api->log(self,"COMMANDER REFUSED: Alpha summon row or one of its guarded fields changed");
        return;
    }
    if (alpha_summon_before[0]!=ALPHA_SUMMON_ABILITY ||
        ALPHA_SUMMON_RVA!=ALPHA_TABLE_RVA+12u*ALPHA_SUMMON_ROW_SIZE ||
        ALPHA_SUMMON_BOUNDS_OFFSET+16u>ALPHA_SUMMON_ROW_SIZE) {
        api->log(self,"COMMANDER REFUSED: internal Alpha summon profile identity check failed");
        return;
    }

    /* Rejoin the module, code signatures, and entire stock row immediately
     * before the one contiguous 16-byte write. */
    uintptr_t check_image=0;
    size_t check_size=0;
    uint8_t check_row[ALPHA_SUMMON_ROW_SIZE];
    if (!current_image(&check_image,&check_size) || check_image!=image ||
        api->read(row,check_row,sizeof check_row) ||
        memcmp(check_row,alpha_summon_before,sizeof check_row)!=0) {
        api->log(self,"COMMANDER REFUSED: Alpha summon profile changed before commit");
        return;
    }
    if (api->write(row+ALPHA_SUMMON_BOUNDS_OFFSET,
                   (const uint8_t *)alpha_summon_after+ALPHA_SUMMON_BOUNDS_OFFSET,16)) {
        if (!rollback_bounds(row))
            api->log(self,"COMMANDER REFUSED: cooldown write failed; partial or foreign state preserved");
        else
            api->log(self,"COMMANDER REFUSED: cooldown write failed; stock Alpha profile restored");
        return;
    }
    if (api->read(row,current,sizeof current) || memcmp(current,alpha_summon_after,sizeof current)!=0) {
        if (!rollback_bounds(row))
            api->log(self,"COMMANDER REFUSED: cooldown readback failed; partial or foreign state preserved");
        else
            api->log(self,"COMMANDER REFUSED: cooldown readback failed; stock Alpha profile restored");
        return;
    }
    applied_image=image;
    applied_row=row;
    api->log(self,"COMMANDER APPLIED: Alpha Warrior summon cooldown bounds 15-20 -> 35-45 seconds; Brood Commander row not modified");
}

int enemy_commander_init(const HfApi *hf, HfMod *mod)
{
    if (!hf || hf->version<13 ||
        hf->size<offsetof(HfApi,code_matches)+sizeof hf->code_matches ||
        !hf->log || !hf->build_id || !hf->module_base || !hf->code_matches ||
        !hf->read || !hf->write || !hf->on) return 1;
    api=hf; self=mod;
    api->log(self,"INIT: Enemy Rebalance 0.7.0 Alpha Commander cooldown; waiting for entity data");
    if (api->on(mod,HF_EVENT_ENTITIES_READY,commander_ready,NULL)) {
        api->log(self,"COMMANDER REFUSED: could not register the entity-ready callback");
    }
    return 0;
}
