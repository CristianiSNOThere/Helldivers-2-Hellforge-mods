/* Per-instance extension of the completion-only stop phase. Native actor and
 * movement cleanup at phase 60 is unchanged. Only accepted phase-0 instances
 * may hold phase 65 for another 30 ticks (0.5 seconds at the pinned 60 Hz). */
typedef struct ChargerHold {
    uintptr_t entity;
    uint8_t identity[16];
    unsigned target,started;
} ChargerHold;
static ChargerHold c_holds[128];
static atomic_flag c_hold_lock=ATOMIC_FLAG_INIT;
static void c_hold_enter(void) { while(atomic_flag_test_and_set_explicit(&c_hold_lock,memory_order_acquire)){} }
static void c_hold_leave(void) { atomic_flag_clear_explicit(&c_hold_lock,memory_order_release); }
static void c_hold_clear(uintptr_t entity)
{
    c_hold_enter();
    for(unsigned i=0;i<128;++i)if(!entity || c_holds[i].entity==entity)memset(&c_holds[i],0,sizeof c_holds[i]);
    c_hold_leave();
}
/* A data reload invalidates unstarted candidates. Already-started holds keep
 * their instance token so their completion-only phase can still finish safely. */
static void c_hold_reload(void)
{
    c_hold_enter();
    for(unsigned i=0;i<128;++i)if(!c_holds[i].started)memset(&c_holds[i],0,sizeof c_holds[i]);
    c_hold_leave();
}
static int c_hold_arm(uintptr_t entity,const uint8_t identity[24],unsigned target)
{
    int found=0;c_hold_enter();
    for(unsigned i=0;i<128;++i)if(!c_holds[i].entity) {
        c_holds[i].entity=entity;memcpy(c_holds[i].identity,identity,16);
        c_holds[i].target=target;c_holds[i].started=0;found=1;break;
    }
    c_hold_leave();return found;
}
static uint32_t c_hold_phase(void *entity,uint32_t phase,int *waiting)
{
    *waiting=0;
    if(phase==0){c_hold_clear((uintptr_t)entity);return phase;}
    if(phase<65)return phase;
    ChargerHold token={0};unsigned slot=128;
    c_hold_enter();
    for(unsigned i=0;i<128;++i)if(c_holds[i].entity==(uintptr_t)entity){token=c_holds[i];slot=i;break;}
    c_hold_leave();
    if(slot==128)return phase;
    uint8_t current[24];
    if(c_api->read((uintptr_t)entity,current,sizeof current)) {
        if(phase==65)c_hold_clear((uintptr_t)entity);
        return phase;
    }
    if(memcmp(current,token.identity,16) || !(current[20]&1u)) {
        c_hold_clear((uintptr_t)entity);return phase;
    }
    if(!token.started) {
        if(phase!=65 || !(atomic_load_explicit(&c_enabled,memory_order_acquire)&(1u<<token.target)) ||
           !c_module() || !c_records(token.target) ||
           !c_match(c_base+0x10A87B0u+32u,c_stop_code+32,sizeof c_stop_code-32u)) {
            c_hold_clear((uintptr_t)entity);return phase;
        }
        c_hold_enter();
        if(c_holds[slot].entity==(uintptr_t)entity && !memcmp(c_holds[slot].identity,token.identity,16))c_holds[slot].started=1;
        else {c_hold_leave();return phase;}
        c_hold_leave();
    }
    if(phase>=95)c_hold_clear((uintptr_t)entity);
    else *waiting=1;
    /* Reviewed phase 65 only returns completion; it has no actor/motion work. */
    return 65;
}
