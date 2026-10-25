#include "plas39_profile.h"

#include <string.h>

#include "plas39_source_guard.generated.h"

static uint32_t hf_plas39_u32(const uint8_t* bytes, size_t offset)
{
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1u] << 8) |
           ((uint32_t)bytes[offset + 2u] << 16) | ((uint32_t)bytes[offset + 3u] << 24);
}

static void hf_plas39_put_u32(uint8_t* bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8);
    bytes[offset + 2u] = (uint8_t)(value >> 16);
    bytes[offset + 3u] = (uint8_t)(value >> 24);
}

static int hf_plas39_mode_valid(uint32_t mode)
{
    return mode == HF_PLAS39_MODE_SEMI || mode == HF_PLAS39_MODE_BURST;
}

int hf_plas39_weapon_profile(const uint8_t* input, size_t size, uint8_t* output)
{
    uint8_t staged[HF_PLAS39_WEAPON_PROFILE_SIZE];
    uint32_t burst, primary, secondary, extra_a, extra_b, left, right;
    if (input == NULL || output == NULL || size != sizeof(staged))
        return 0;
    burst = hf_plas39_u32(input, 140u);
    primary = hf_plas39_u32(input, 144u);
    secondary = hf_plas39_u32(input, 148u);
    extra_a = hf_plas39_u32(input, 152u);
    extra_b = hf_plas39_u32(input, 156u);
    left = hf_plas39_u32(input, 184u);
    right = hf_plas39_u32(input, 188u);
    if ((burst != 3u && burst != 1u) ||
        !((primary == 2u && secondary == 0u) || (primary == 3u && secondary == 2u)) ||
        extra_a != 0u || extra_b != 0u ||
        !((left == 0u && right == 3u) || (left == 3u && right == 0u)))
        return 0;
    memcpy(staged, input, sizeof(staged));
    hf_plas39_put_u32(staged, 140u, 1u);
    hf_plas39_put_u32(staged, 144u, 3u);
    hf_plas39_put_u32(staged, 148u, 2u);
    hf_plas39_put_u32(staged, 184u, 3u);
    hf_plas39_put_u32(staged, 188u, 0u);
    memcpy(output, staged, sizeof(staged));
    return 1;
}

int hf_plas39_charge_profile(const uint8_t* input, size_t size, uint32_t mode,
                             uint8_t* output)
{
    uint8_t canonical[HF_PLAS39_CHARGE_PROFILE_SIZE];
    uint8_t staged[HF_PLAS39_CHARGE_PROFILE_SIZE];
    static const uint8_t min_semi[4] = {0x0Au, 0xD7u, 0x23u, 0x3Cu};
    if (input == NULL || output == NULL || size != sizeof(staged) || !hf_plas39_mode_valid(mode))
        return 0;
    memcpy(canonical, input, sizeof(canonical));
    memcpy(canonical, HF_PLAS39_CHARGE_STOCK, 4u);
    memcpy(canonical + 188u, HF_PLAS39_CHARGE_STOCK + 188u, 4u);
    if (memcmp(canonical, HF_PLAS39_CHARGE_STOCK, sizeof(canonical)) != 0)
        return 0;
    memcpy(staged, input, sizeof(staged));
    if (mode == HF_PLAS39_MODE_SEMI)
    {
        memcpy(staged, min_semi, sizeof(min_semi));
        hf_plas39_put_u32(staged, 188u, 0u);
    }
    else
    {
        memcpy(staged, HF_PLAS39_CHARGE_STOCK, 4u);
        hf_plas39_put_u32(staged, 188u, 2u);
    }
    memcpy(output, staged, sizeof(staged));
    return 1;
}

int hf_plas39_projectile_profile(const uint8_t* input, size_t size, uint32_t mode,
                                 uint8_t* output)
{
    uint8_t staged[HF_PLAS39_PROJECTILE_PROFILE_SIZE];
    static const uint8_t rpm_stock[4] = {0x00u, 0x80u, 0x09u, 0x44u};
    static const uint8_t rpm_semi[4] = {0x00u, 0x00u, 0x48u, 0x43u};
    static const uint8_t audio_stock[4] = {0x01u, 0x00u, 0x00u, 0x00u};
    static const uint8_t audio_semi[4] = {0x01u, 0x01u, 0x00u, 0x00u};
    const uint8_t* target_rpm;
    const uint8_t* target_audio;
    if (input == NULL || output == NULL || size != sizeof(staged) || !hf_plas39_mode_valid(mode) ||
        hf_plas39_u32(input, 0u) != 155u ||
        (memcmp(input + 8u, rpm_stock, 4u) != 0 && memcmp(input + 8u, rpm_semi, 4u) != 0) ||
        (memcmp(input + 236u, audio_stock, 4u) != 0 && memcmp(input + 236u, audio_semi, 4u) != 0))
        return 0;
    target_rpm = mode == HF_PLAS39_MODE_SEMI ? rpm_semi : rpm_stock;
    target_audio = mode == HF_PLAS39_MODE_SEMI ? audio_semi : audio_stock;
    memcpy(staged, input, sizeof(staged));
    memcpy(staged + 8u, target_rpm, 4u);
    memcpy(staged + 236u, target_audio, 4u);
    memcpy(output, staged, sizeof(staged));
    return 1;
}

int hf_plas39_cadence_profile(const uint8_t input[4], uint32_t mode, uint8_t output[4])
{
    static const uint8_t stock[4] = {0x0Eu, 0x6Bu, 0xDFu, 0x3Du};
    static const uint8_t semi[4] = {0x9Au, 0x99u, 0x99u, 0x3Eu};
    static const uint8_t stock_reduced[4] = {0x10u, 0x3Eu, 0xF8u, 0x3Du};
    static const uint8_t semi_reduced[4] = {0xABu, 0xAAu, 0xAAu, 0x3Eu};
    const uint8_t* target;
    int reduced;
    if (input == NULL || output == NULL || !hf_plas39_mode_valid(mode))
        return 0;
    reduced = memcmp(input, stock_reduced, sizeof(stock)) == 0 ||
              memcmp(input, semi_reduced, sizeof(stock)) == 0;
    if (memcmp(input, stock, sizeof(stock)) != 0 && memcmp(input, semi, sizeof(semi)) != 0 &&
        memcmp(input, stock_reduced, sizeof(stock_reduced)) != 0 &&
        memcmp(input, semi_reduced, sizeof(semi_reduced)) != 0)
        return 0;
    if (mode == HF_PLAS39_MODE_SEMI)
        target = reduced ? semi_reduced : semi;
    else
        target = reduced ? stock_reduced : stock;
    memcpy(output, target, 4u);
    return 1;
}

int hf_plas39_needs_repetition_clear(int had_previous, uint32_t previous_mode,
                                     uint32_t current_mode)
{
    if (!hf_plas39_mode_valid(current_mode))
        return 0;
    return current_mode == HF_PLAS39_MODE_SEMI &&
           (!had_previous || previous_mode != HF_PLAS39_MODE_SEMI);
}
