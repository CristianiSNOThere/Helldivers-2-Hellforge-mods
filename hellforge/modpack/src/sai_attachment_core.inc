/* Exact-table proof and reversible writes for SAI's three unlisted lens fields. */
#include "sai_attachment_source.generated.h"

typedef struct HfSaiAttachmentOps
{
    void* context;
    int (*read)(void*, uintptr_t, void*, size_t); /* SDK convention: zero succeeds. */
    int (*write)(void*, uintptr_t, const void*, size_t); /* -2: protection restore failed. */
    int (*access)(void*, uintptr_t, size_t);
    int (*hash)(void*, const void*, size_t, char[65]);
    int (*companion_owned)(void*, unsigned);
} HfSaiAttachmentOps;

typedef struct HfSaiAttachmentState
{
    uintptr_t row;
    uint32_t attempted;
    uint32_t poisoned;
} HfSaiAttachmentState;

static size_t hf_sai_lens_offset(unsigned field)
{
    return field == 0u ? 0u : field == 1u ? 8u : 12u;
}

static int hf_sai_attachment_proof(const HfSaiAttachmentOps* ops,
                                   const HfSaiAttachmentState* state)
{
    static const uint32_t companion_offsets[2] = {170592u,175452u};
    static const uint8_t companion_before[2][4] = {{0x28u,0u,0u,0u},{0u,0u,0xC0u,0x3Fu}};
    static const uint8_t companion_after[2][4] = {{0x41u,0u,0u,0u},{0u,0u,0x80u,0x3Fu}};
    uint8_t* bytes;
    char digest[65];
    size_t i, j;
    int valid = 0;
    if (sizeof(uintptr_t) != 8u || ops == NULL || ops->read == NULL || ops->access == NULL || ops->hash == NULL ||
        state == NULL || state->poisoned || state->row < (uintptr_t)65536u ||
        state->row > (uintptr_t)UINT64_C(0x800000000000) - HF_SAI_ATTACHMENT_SIZE ||
        !ops->access(ops->context, state->row, HF_SAI_ATTACHMENT_SIZE))
        return 0;
    bytes = malloc(HF_SAI_ATTACHMENT_SIZE);
    if (bytes == NULL) return 0;
    if (ops->read(ops->context, state->row, bytes, HF_SAI_ATTACHMENT_SIZE) != 0 ||
        memcmp(bytes, HF_SAI_DELTA_HEADER, sizeof(HF_SAI_DELTA_HEADER)) != 0)
        goto done;
    for (i = 0u; i < 5u; ++i)
    {
        uintptr_t pointer;
        uint32_t count, flags;
        const uint32_t* array = HF_SAI_DELTA_ARRAYS[i];
        memcpy(&pointer, bytes + array[0], 8u);
        memcpy(&count, bytes + array[0] + 8u, 4u);
        memcpy(&flags, bytes + array[0] + 12u, 4u);
        if (pointer != state->row + array[1] || count != array[2] || flags != 0u)
            goto done;
    }
    for (i = 0u; i < 3u; ++i)
    {
        size_t offset = HF_SAI_FOCUS_OFFSETS[i];
        const uint8_t* before = HF_SAI_FOCUS_BEFORE + hf_sai_lens_offset((unsigned)i);
        const uint8_t* after = HF_SAI_FOCUS_AFTER + hf_sai_lens_offset((unsigned)i);
        if ((state->attempted & (1u << i)) == 0u)
        {
            if (memcmp(bytes + offset, before, 4u) != 0) goto done;
        }
        else
        {
            /* Only bytes from our attempted four-byte write may be normalized.
             * This also permits an owned partial write to be rolled back. */
            for (j = 0u; j < 4u; ++j)
                if (bytes[offset + j] != before[j] && bytes[offset + j] != after[j]) goto done;
            memcpy(bytes + offset, before, 4u);
        }
    }
    if (memcmp(bytes + 181581u, HF_SAI_FOCUS_SIGNATURE, sizeof(HF_SAI_FOCUS_SIGNATURE)) != 0)
        goto done;
    for (i = 0u; i < 2u; ++i)
    {
        uint8_t* field = bytes + companion_offsets[i];
        if (memcmp(field, companion_before[i], 4u) == 0) continue;
        if (memcmp(field, companion_after[i], 4u) != 0 || ops->companion_owned == NULL ||
            !ops->companion_owned(ops->context, (unsigned)i)) goto done;
        memcpy(field, companion_before[i], 4u);
    }
    if (!ops->hash(ops->context, bytes + 108u, HF_SAI_ATTACHMENT_SIZE - 108u, digest)) goto done;
    digest[64] = '\0';
    valid = strcmp(digest, HF_SAI_ATTACHMENT_HASH) == 0;
done:
    free(bytes);
    return valid;
}

static int hf_sai_attachment_reset(const HfSaiAttachmentOps* ops,
                                   HfSaiAttachmentState* state, unsigned field)
{
    uint8_t current[4];
    const uint8_t* before;
    if (field >= 3u || ops == NULL || ops->write == NULL ||
        !hf_sai_attachment_proof(ops, state)) return 0;
    if ((state->attempted & (1u << field)) == 0u) return 1;
    before = HF_SAI_FOCUS_BEFORE + hf_sai_lens_offset(field);
    if (ops->read(ops->context, state->row + HF_SAI_FOCUS_OFFSETS[field], current, 4u) != 0 ||
        (memcmp(current,before,4u) != 0 &&
         ops->write(ops->context, state->row + HF_SAI_FOCUS_OFFSETS[field], before, 4u) != 0) ||
        ops->read(ops->context, state->row + HF_SAI_FOCUS_OFFSETS[field], current, 4u) != 0 ||
        memcmp(current, before, 4u) != 0)
    {
        state->poisoned = 1u;
        return 0;
    }
    state->attempted &= ~(1u << field);
    if (!hf_sai_attachment_proof(ops, state))
    {
        state->poisoned = 1u;
        return 0;
    }
    return 1;
}

static int hf_sai_attachment_apply(const HfSaiAttachmentOps* ops,
                                   HfSaiAttachmentState* state, unsigned field)
{
    uint8_t current[4];
    const uint8_t* after;
    int result;
    if (field >= 3u || ops == NULL || ops->write == NULL || state == NULL ||
        state->poisoned || (state->attempted & (1u << field)) != 0u ||
        !hf_sai_attachment_proof(ops, state)) return 0;
    after = HF_SAI_FOCUS_AFTER + hf_sai_lens_offset(field);
    state->attempted |= 1u << field;
    result = ops->write(ops->context, state->row + HF_SAI_FOCUS_OFFSETS[field], after, 4u);
    if (result == -2)
    {
        state->poisoned = 1u;
        return -1;
    }
    if (result == 0 &&
        ops->read(ops->context, state->row + HF_SAI_FOCUS_OFFSETS[field], current, 4u) == 0 &&
        memcmp(current, after, 4u) == 0 && hf_sai_attachment_proof(ops, state)) return 1;
    if (!hf_sai_attachment_reset(ops, state, field))
    {
        state->poisoned = 1u;
        return -1;
    }
    return 0;
}
