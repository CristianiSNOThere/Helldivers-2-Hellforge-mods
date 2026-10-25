/* Byte-exact VG-70 transforms ported from source/vg70-variable/component-profile.lua
 * and mode-profile.lua. These routines validate before writing any output. */
#ifndef HF_VG70_PROFILE_H
#define HF_VG70_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#define HF_VG70_WEAPON_SIZE 1232u
#define HF_VG70_PROJECTILE_SIZE 616u
#define HF_VG70_MAGAZINE_SIZE 160u
#define HF_VG70_WEAPON_STATE_SIZE 1008u
#define HF_VG70_RATE_STATE_SIZE 32u
#define HF_VG70_CURRENT_RPM_SIZE 12u
#define HF_VG70_PROJECTILE_STATE_SIZE 168u
#define HF_VG70_MAX_INSTANCES 512u

typedef struct HfVg70Selection
{
    uint32_t rpm;
    uint32_t slot;
    uint32_t projectile;
    uint32_t rates[3];
} HfVg70Selection;

typedef struct HfVg70ModeMemory
{
    uint64_t entity_id;
    uintptr_t entity_identity;
    uint32_t auto_rpm;
    uint32_t mode;
    uint32_t active;
} HfVg70ModeMemory;

typedef struct HfVg70MemoryOps
{
    void* context;
    int (*read)(void* context, uintptr_t address, void* output, size_t size);
    int (*write)(void* context, uintptr_t address, const void* input, size_t size);
    int (*data_access)(void* context, uintptr_t address, size_t size);
} HfVg70MemoryOps;

typedef enum HfVg70ApplyResult
{
    HF_VG70_FATAL = -1,
    HF_VG70_REFUSED = 0,
    HF_VG70_APPLIED = 1
} HfVg70ApplyResult;

typedef struct HfVg70NativePlan
{
    uintptr_t game, owner, entities, projectile_map;
    uintptr_t weapon_address, projectile_address, magazine_address;
    uintptr_t weapon_manager, projectile_manager, magazine_manager;
    uint8_t weapon_header[192], projectile_header[224], magazine_header[256];
} HfVg70NativePlan;

typedef struct HfVg70Instance
{
    uint32_t id, mode, slot, weapon_index, projectile_index, magazine_index;
    uint32_t weapon_private_index, projectile_private_index, magazine_private_index;
    uintptr_t entity, mode_address, weapon, projectile, magazine;
    uintptr_t weapon_state, projectile_state, rate_state, current_rpm_state;
    uintptr_t magazine_ammo, magazine_effective;
    uint32_t weapon_cached, projectile_cached, magazine_cached;
    uint8_t identity[24];
    uint8_t weapon_row[HF_VG70_WEAPON_SIZE];
    uint8_t projectile_row[HF_VG70_PROJECTILE_SIZE];
    uint8_t magazine_row[HF_VG70_MAGAZINE_SIZE];
    uint8_t weapon_state_row[HF_VG70_WEAPON_STATE_SIZE];
    uint8_t rate_state_row[HF_VG70_RATE_STATE_SIZE];
    uint8_t current_rpm_row[HF_VG70_CURRENT_RPM_SIZE];
    uint8_t projectile_state_row[HF_VG70_PROJECTILE_STATE_SIZE];
} HfVg70Instance;

/* mode is 0 for the shared source row, or 1 (Auto), 4 (Volley), 7 (Total). */
int hf_vg70_weapon_data(const uint8_t* input, size_t size, uint32_t mode, uint8_t* output);
int hf_vg70_projectile_weapon(const uint8_t* input, size_t size, uint32_t projectile,
                              const uint32_t rates[3], uint8_t* output);
int hf_vg70_magazine(const uint8_t* input, size_t size, uint8_t* output);
int hf_vg70_weapon_state(const uint8_t* input, size_t size, uint32_t mode, uint8_t* output);
int hf_vg70_rate_state(const uint8_t* input, size_t size, uint32_t slot,
                       const uint32_t rates[3], uint8_t* output);
int hf_vg70_current_rpm_state(const uint8_t* input, size_t size, uint32_t rpm, uint8_t* output);
int hf_vg70_cadence(const uint8_t* input, size_t size, uint32_t current_rpm,
                    uint32_t target_rpm, uint8_t* output, float* multiplier);

/* One memory record belongs to one stable live weapon identity. */
int hf_vg70_mode_step(HfVg70ModeMemory* memory, uint64_t entity_id,
                      uintptr_t entity_identity, uint32_t mode, uint32_t selected_slot,
                      uint32_t current_rpm, const uint32_t current_vector[3],
                      int force_default, HfVg70Selection* output);

/* Apply the accepted shared projectile/damage table profiles as one guarded transaction. */
HfVg70ApplyResult hf_vg70_apply_shared_profiles(const HfVg70MemoryOps* ops, uintptr_t game,
                                                 uint32_t* changed_fields,
                                                 char* reason, size_t reason_capacity);
HfVg70ApplyResult hf_vg70_apply_all_profiles(const HfVg70MemoryOps* ops, uintptr_t game,
                                              HfVg70NativePlan* plan,
                                              uint32_t* changed_fields,
                                              char* reason, size_t reason_capacity);
int hf_vg70_native_prepare(const HfVg70MemoryOps* ops, uintptr_t game,
                           HfVg70NativePlan* plan, char* reason, size_t reason_capacity);
int hf_vg70_native_discover(const HfVg70MemoryOps* ops, HfVg70NativePlan* plan,
                            HfVg70Instance* instances, size_t capacity, size_t* count,
                            char* reason, size_t reason_capacity);
int hf_vg70_native_validate(const HfVg70MemoryOps* ops, const HfVg70NativePlan* plan,
                            const HfVg70Instance* instances, size_t count);
HfVg70ApplyResult hf_vg70_native_isolate(const HfVg70MemoryOps* ops,
                                          HfVg70NativePlan* plan,
                                          HfVg70Instance* instances, size_t count,
                                          uint32_t* new_rows,
                                          char* reason, size_t reason_capacity);
HfVg70ApplyResult hf_vg70_native_configure(const HfVg70MemoryOps* ops,
                                            const HfVg70NativePlan* plan,
                                            HfVg70Instance* instances, size_t count,
                                            HfVg70ModeMemory* memories, size_t memory_capacity,
                                            int boot_sweep_done, uint32_t* changed_fields,
                                            char* reason, size_t reason_capacity);

#endif
