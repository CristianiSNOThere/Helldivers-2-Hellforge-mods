#ifndef HF_PLAS39_RUNTIME_H
#define HF_PLAS39_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "plas39_profile.h"

#define HF_PLAS39_MAX_INSTANCES 512u
#define HF_PLAS39_MAX_EDITS (HF_PLAS39_MAX_INSTANCES * 240u + 32u)
#define HF_PLAS39_EDIT_HASH_SIZE 262144u
#define HF_PLAS39_DATA_HEADER_SIZE 192u
#define HF_PLAS39_CHARGE_HEADER_SIZE 160u
#define HF_PLAS39_PROJECTILE_HEADER_SIZE 224u
#define HF_PLAS39_PROJECTILE_STRIDE 616u

typedef struct HfPlas39MemoryOps
{
    void* context;
    int (*read)(void* context, uintptr_t address, void* output, size_t size);
    int (*write)(void* context, uintptr_t address, const void* input, size_t size,
                 int protected_write);
    int (*can_write)(void* context, uintptr_t address, size_t size, int protected_write);
    int (*data_access)(void* context, uintptr_t address, size_t size);
} HfPlas39MemoryOps;

typedef struct HfPlas39Edit
{
    uintptr_t address;
    uint8_t before[4];
    uint8_t after[4];
    uint8_t clone;
    uint8_t attempted;
    uint16_t reserved;
} HfPlas39Edit;

typedef struct HfPlas39EditHashSlot
{
    uintptr_t address;
    uint32_t index_plus_one;
    uint32_t generation;
} HfPlas39EditHashSlot;

typedef struct HfPlas39Transaction
{
    HfPlas39Edit* edits;
    size_t edit_capacity;
    size_t edit_count;
    HfPlas39EditHashSlot* hash;
    size_t hash_capacity;
    uint32_t generation;
} HfPlas39Transaction;

typedef struct HfPlas39Instance
{
    uint32_t id, mode;
    uintptr_t entity, mode_address, weapon, charge, charge_state, projectile, projectile_state;
    uint32_t charge_cached, projectile_cached, charge_cloned, projectile_cloned;
    uint8_t identity[24];
    uint8_t charge_row[HF_PLAS39_CHARGE_PROFILE_SIZE];
    uint8_t projectile_row[HF_PLAS39_PROJECTILE_STRIDE];
} HfPlas39Instance;

typedef struct HfPlas39Snapshot
{
    uintptr_t data_manager, charge_manager, projectile_manager;
    uint8_t data_header[HF_PLAS39_DATA_HEADER_SIZE];
    uint8_t charge_header[HF_PLAS39_CHARGE_HEADER_SIZE];
    uint8_t projectile_header[HF_PLAS39_PROJECTILE_HEADER_SIZE];
} HfPlas39Snapshot;

typedef struct HfPlas39Plan
{
    uintptr_t game, owner, weapon_group, charge_group, projectile_map;
    uintptr_t weapon_address, charge_address, projectile_address;
    HfPlas39Snapshot snapshot;
} HfPlas39Plan;

typedef struct HfPlas39PreviousMode
{
    uint32_t id, mode;
} HfPlas39PreviousMode;

typedef enum HfPlas39ApplyResult
{
    HF_PLAS39_FATAL = -1,
    HF_PLAS39_REFUSED = 0,
    HF_PLAS39_APPLIED = 1,
    HF_PLAS39_NOT_READY = 2,
    HF_PLAS39_RETRY_RACE = 3,
    HF_PLAS39_NOT_FOUND = 4
} HfPlas39ApplyResult;

int hf_plas39_transaction_init(HfPlas39Transaction* transaction,
                               HfPlas39Edit* edits, size_t edit_capacity,
                               HfPlas39EditHashSlot* hash, size_t hash_capacity);
void hf_plas39_transaction_reset(HfPlas39Transaction* transaction);
HfPlas39ApplyResult hf_plas39_transaction_apply(const HfPlas39MemoryOps* ops,
                                                 HfPlas39Transaction* transaction,
                                                 uint32_t* changed,
                                                 char* reason, size_t reason_capacity);
HfPlas39ApplyResult hf_plas39_native_prepare(const HfPlas39MemoryOps* ops, uintptr_t game,
                                              HfPlas39Transaction* transaction,
                                              HfPlas39Plan* plan, uint32_t* changed,
                                              char* reason, size_t reason_capacity);
HfPlas39ApplyResult hf_plas39_native_discover(const HfPlas39MemoryOps* ops,
                                               HfPlas39Plan* plan,
                                               HfPlas39Instance* instances,
                                               size_t capacity, size_t* count,
                                               char* reason, size_t reason_capacity);
HfPlas39ApplyResult hf_plas39_native_validate(const HfPlas39MemoryOps* ops,
                                               const HfPlas39Plan* plan,
                                               const HfPlas39Instance* instances,
                                               size_t count);
HfPlas39ApplyResult hf_plas39_synchronize(const HfPlas39MemoryOps* ops,
                                            HfPlas39Plan* plan,
                                            HfPlas39Instance* instances, size_t count,
                                            HfPlas39PreviousMode* previous, size_t previous_capacity,
                                            size_t* previous_count,
                                            HfPlas39Transaction* transaction,
                                            uint32_t* changed, char* status,
                                            size_t status_capacity);

#endif
