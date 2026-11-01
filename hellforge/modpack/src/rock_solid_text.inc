/* Rock-Solid's private tooltip line. The modifier's key/type/value stay exact. */
#include <windows.h>
#include "data_transaction.h"
#define HF_ROCK_TEXT_ID 0x5154104fu
#define HF_ROCK_TEXT_STOCK 0xe4369b6bu
static const char HF_ROCK_DESCRIPTION[] =
    "Bearer is more resistant to being knocked prone. Reduces incoming melee damage to the wearer by 50%.";
typedef const char* (*HfRockTextFn)(uint32_t,void*,void*,void*);
static HfRockTextFn HF_ROCK_TEXT_NEXT;
static volatile LONG HF_ROCK_TEXT_ACTIVE;
static int HF_ROCK_TEXT_HOOKED,HF_ROCK_TEXT_STOPPED,HF_ROCK_TEXT_APPLIED;
static float HF_ROCK_TEXT_POLL,HF_ROCK_TEXT_WAIT;
static uintptr_t HF_ROCK_TEXT_ENTRY;
typedef struct HfRockTextPlan {
    uintptr_t custom,passive,modifiers;
    uint8_t row[52],items[48];
} HfRockTextPlan;

#ifdef __cplusplus
extern "C" {
#endif
__attribute__((used,noinline)) static const char* hf_rock_text_dispatch(uint32_t id,void* a,void* b,void* c) {
    if(InterlockedCompareExchange(&HF_ROCK_TEXT_ACTIVE,0,0) && id==HF_ROCK_TEXT_ID)return HF_ROCK_DESCRIPTION;
    return HF_ROCK_TEXT_NEXT?HF_ROCK_TEXT_NEXT(id,a,b,c):"";
}
/* Preserve the native thunk's live SIMD registers on every route. */
__attribute__((used,noinline,naked)) static const char* hf_rock_text_entry(uint32_t id,void* a,void* b,void* c) {
    __asm__ volatile(
      ".seh_proc hf_rock_text_entry\n"
      "subq $136, %rsp\n"
      ".seh_stackalloc 136\n"
      ".seh_endprologue\n"
      "movdqu %xmm0, 32(%rsp)\n"
      "movdqu %xmm1, 48(%rsp)\n"
      "movdqu %xmm2, 64(%rsp)\n"
      "movdqu %xmm3, 80(%rsp)\n"
      "movdqu %xmm4, 96(%rsp)\n"
      "movdqu %xmm5, 112(%rsp)\n"
      "callq hf_rock_text_dispatch\n"
      "movdqu 32(%rsp), %xmm0\n"
      "movdqu 48(%rsp), %xmm1\n"
      "movdqu 64(%rsp), %xmm2\n"
      "movdqu 80(%rsp), %xmm3\n"
      "movdqu 96(%rsp), %xmm4\n"
      "movdqu 112(%rsp), %xmm5\n"
      "addq $136, %rsp\n"
      "retq\n"
      ".seh_endproc\n");
}
#ifdef __cplusplus
}
#endif
static int hf_rock_text_io_read(void* context,uintptr_t at,void* out,size_t n) {
    const HfApi* api=(const HfApi*)context;return api->read(at,out,n);
}
static int hf_rock_text_io_write(void* context,uintptr_t at,const void* data,size_t n) {
    const HfApi* api=(const HfApi*)context;return api->write(at,data,n);
}
static int hf_rock_text_plan(const HfApi* api,HfRockTextPlan* out) {
    HfRockWitness witness;memset(&witness,0,sizeof witness);memset(out,0,sizeof(*out));
    if(!HF_ROCK_READY || !hf_rock_read(api,HF_ROCK_GAME+0x33264f8u,&out->custom,8u) ||
       !hf_component_pointer(out->custom,0xb7cu) || !hf_rock_passive(api,out->custom,&witness))return 0;
    out->passive=witness.passive;memcpy(out->row,witness.passive_row,52u);
    out->modifiers=(uintptr_t)hf_rock_u64(out->row+16);
    if(!hf_rock_read(api,out->modifiers,out->items,48u))return 0;
    const uint32_t keys[3]={0x2559b40du,0x0dff0e42u,0x11a3c04cu};
    const uint32_t types[3]={2u,0u,2u},values[3]={0x3fb33333u,0x3f800000u,0x3f333333u};
    for(unsigned i=0;i<3;i++)if(hf_rock_u32(out->items+i*16u)!=keys[i] ||
        hf_rock_u32(out->items+i*16u+4u)!=types[i] || hf_rock_u32(out->items+i*16u+8u)!=values[i])return 0;
    uint32_t text=hf_rock_u32(out->items+44);
    if(hf_rock_u32(out->items+12)!=0x07013e97u || hf_rock_u32(out->items+28)!=0u ||
       (text!=HF_ROCK_TEXT_STOCK && !(HF_ROCK_TEXT_APPLIED && text==HF_ROCK_TEXT_ID)))return 0;
    uintptr_t rows;uint32_t count;
    if(!hf_rock_read(api,out->custom+0x20u,&rows,8u) || !hf_rock_read(api,out->custom+0x28u,&count,4u))return 0;
    for(uint32_t i=0;i<count;i++) {
        uintptr_t row,items;uint32_t n;
        if(!hf_rock_read(api,rows+(size_t)i*8u,&row,8u))return 0;
        if(row==out->passive)continue;
        if(!hf_rock_read(api,row+16u,&items,8u) || !hf_rock_read(api,row+24u,&n,4u) || n>64u)return 0;
        if(n && (!hf_component_pointer(items,(size_t)n*16u) ||
           (out->modifiers+44u>=items && out->modifiers+44u<items+(size_t)n*16u)))return 0;
    }
    return 1;
}
static int hf_rock_text_apply(const HfApi* api,HfMod* mod) {
    HfRockTextPlan first,again;
    if(!hf_rock_text_plan(api,&first) || !hf_rock_text_plan(api,&again) || memcmp(&first,&again,sizeof first))return 0;
    if(hf_rock_u32(first.items+44)==HF_ROCK_TEXT_ID)return 1;
    uint32_t before=HF_ROCK_TEXT_STOCK,after=HF_ROCK_TEXT_ID;
    int result=hf_row_write_if(hf_rock_text_io_read,hf_rock_text_io_write,(void*)api,first.modifiers+44u,(const uint8_t*)&before,(const uint8_t*)&after,4u);
    if(result!=1)return result;
    /* A failed postcheck repairs only our text scalar; foreign bytes are preserved. */
    HF_ROCK_TEXT_APPLIED=1;
    if(!hf_rock_text_plan(api,&again) || first.custom!=again.custom || first.passive!=again.passive ||
       first.modifiers!=again.modifiers || memcmp(first.row,again.row,52u) || memcmp(first.items,again.items,44u)) {
        int repaired=hf_row_undo_owned(hf_rock_text_io_read,hf_rock_text_io_write,(void*)api,first.modifiers+44u,(const uint8_t*)&before,(const uint8_t*)&after,4u);
        HF_ROCK_TEXT_APPLIED=0;return repaired?0:-1;
    }
    HF_ROCK_TEXT_ENTRY=first.modifiers+44u;
    api->log(mod,"Rock-Solid description Applied: existing bonuses plus 50%% incoming melee damage reduction; private text5154104f");
    return 1;
}
static void hf_rock_text_tick(HfMod* mod,float dt,void* user) {
    const HfApi* api=(const HfApi*)user;
    if(HF_ROCK_TEXT_STOPPED || !HF_ROCK_READY || !(dt>0.0f && dt<=10.0f))return;
    HF_ROCK_TEXT_POLL+=dt;if(HF_ROCK_TEXT_POLL<0.5f)return;HF_ROCK_TEXT_POLL=0.0f;HF_ROCK_TEXT_WAIT+=0.5f;
    if(!api->write || !api->hook_ex) {HF_ROCK_TEXT_STOPPED=1;api->log(mod,"Rock-Solid description refused: SDK text hooks/writes unavailable; melee protection unchanged");return;}
    if(!HF_ROCK_TEXT_HOOKED) {
        size_t image=0;uintptr_t exe=api->module_base("helldivers2.exe",&image);
        if(!exe || image<=0x321dc0u || !api->code_matches(exe+0x321da0u,"33 D2 E9 99 FE FF FF CC CC CC CC CC CC CC CC CC"))goto waiting;
        if(api->hook_ex(mod,exe+0x321da0u,(void*)hf_rock_text_entry,(void**)&HF_ROCK_TEXT_NEXT,0) || !HF_ROCK_TEXT_NEXT) {
            HF_ROCK_TEXT_STOPPED=1;api->log(mod,"Rock-Solid description refused: native text hook unavailable; melee protection unchanged");return;
        }
        HF_ROCK_TEXT_HOOKED=1;
        const char* foreign=HF_ROCK_TEXT_NEXT(HF_ROCK_TEXT_ID,NULL,NULL,NULL);
        if(foreign && foreign[0]) {HF_ROCK_TEXT_STOPPED=1;api->log(mod,"Rock-Solid description refused: private text ID already owned; original descriptions preserved");return;}
        InterlockedExchange(&HF_ROCK_TEXT_ACTIVE,1);
    }
    {
        int result=hf_rock_text_apply(api,mod);
        if(result==1) {HF_ROCK_TEXT_WAIT=0.0f;return;}
        if(result<0) {HF_ROCK_TEXT_STOPPED=1;api->log(mod,"Rock-Solid description stopped: text scalar rollback incomplete; gameplay values unchanged");return;}
    }
waiting:
    if(HF_ROCK_TEXT_WAIT>=60.0f) {HF_ROCK_TEXT_STOPPED=1;api->log(mod,"Rock-Solid description refused: text/passive ownership not ready within60s; melee protection unchanged");}
}
