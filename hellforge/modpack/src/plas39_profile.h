#ifndef HF_PLAS39_PROFILE_H
#define HF_PLAS39_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#define HF_PLAS39_WEAPON_PROFILE_SIZE 192u
#define HF_PLAS39_CHARGE_PROFILE_SIZE 216u
#define HF_PLAS39_PROJECTILE_PROFILE_SIZE 264u

enum HfPlas39Mode
{
    HF_PLAS39_MODE_SEMI = 2u,
    HF_PLAS39_MODE_BURST = 3u
};

/* Each helper validates the exact accepted alternatives before writing output. */
int hf_plas39_weapon_profile(const uint8_t* input, size_t size, uint8_t* output);
int hf_plas39_charge_profile(const uint8_t* input, size_t size, uint32_t mode,
                             uint8_t* output);
int hf_plas39_projectile_profile(const uint8_t* input, size_t size, uint32_t mode,
                                 uint8_t* output);
int hf_plas39_cadence_profile(const uint8_t input[4], uint32_t mode, uint8_t output[4]);
int hf_plas39_needs_repetition_clear(int had_previous, uint32_t previous_mode,
                                     uint32_t current_mode);

#endif
