#include "sai_attachment_core.inc"
static HfSaiAttachmentState HF_SAI_ATTACHMENT_STATE;

static int hf_sai_attachment_field(const HfPortEdit* edit)
{
    unsigned i;
    if (edit == NULL || edit->kind != PORT_EDIT_STAT || edit->option == NULL ||
        strcmp(edit->option, "sai-focus-precision") != 0 || edit->stat.kind == NULL ||
        strcmp(edit->stat.kind, "EntityDelta") != 0 || edit->stat.component == NULL ||
        strcmp(edit->stat.component, "236:960") != 0 ||
        edit->stat.resource != UINT64_C(0xA188CBB5913FBD63) || edit->stat.record != 278 ||
        edit->stat.type != HF_TYPE_F32 || edit->has_expected_base != 1u ||
        edit->value.type != HF_TYPE_F32 || edit->expected_base.type != HF_TYPE_F32 ||
        edit->flags != HF_LIVE_SHARED) return -1;
    for (i = 0u; i < 3u; ++i)
        if (edit->stat.offset == HF_SAI_FOCUS_OFFSETS[i] &&
            edit->expected_base.number == (i == 0u ? 600.0 : 75.0) &&
            edit->value.number == (i == 0u ? 900.0 : 37.5)) return (int)i;
    return -1;
}

static inline int hf_sai_sdk_companion_owned(const HfApi* api, HfMod* mod, unsigned which)
{
    static const HfStat fields[3] = {
        {"EntityDelta","5:136",UINT64_C(0x16AB263BC03B4F96),153,170592u,HF_TYPE_U32},
        {"EntityDelta","266:116",UINT64_C(0x08755D8F453EA7BA),195,175452u,HF_TYPE_F32},
        {"EntityDelta","5:136",UINT64_C(0xACA180FC6F846411),203,175679u,HF_TYPE_U32}
    };
    HfLiveValue value;
    double desired = which == 0u ? 65.0 : which == 1u ? 1.0 : 20.0;
    double original = which == 0u ? 40.0 : which == 1u ? 1.5 : 8.0;
    if (which >= 3u || api == NULL || api->get_stat == NULL ||
        !api->get_stat(mod, &fields[which], &value)) return 0;
    return value.mod != NULL && strcmp(value.mod,"superearth.arsenal-native-port") == 0 &&
           value.now.type == fields[which].type && value.base.type == fields[which].type &&
           value.now.number == desired &&
           (value.base.number == desired || value.base.number == original);
}

#if defined(HF_PORT_TEST) && !defined(HF_SAI_TEST_NATIVE)
static HfSaiAttachmentOps HF_SAI_TEST_OPS;
static int hf_sai_attachment_find(void) { return HF_SAI_ATTACHMENT_STATE.row != 0u ? 1 : 2; }
static HfSaiAttachmentOps hf_sai_attachment_ops(const HfApi* api, HfMod* mod)
{
    (void)api; (void)mod;
    return HF_SAI_TEST_OPS;
}
#else
typedef struct HfSaiAttachmentNative
{
    const HfApi* api;
    HfMod* mod;
    HfSickleCrypto crypto;
    uintptr_t cursor;
    uintptr_t candidate;
    unsigned candidates;
    unsigned regions;
    int complete;
} HfSaiAttachmentNative;
static HfSaiAttachmentNative HF_SAI_ATTACHMENT_NATIVE;

static int hf_sai_attachment_find(void)
{
    HfSaiAttachmentNative* native = &HF_SAI_ATTACHMENT_NATIVE;
    unsigned budget = 512u;
    if (native->cursor == 0u) native->cursor = (uintptr_t)65536u;
    while (!native->complete && budget-- > 0u)
    {
        MEMORY_BASIC_INFORMATION info;
        uintptr_t next;
        uint8_t header[28];
        if ((uint64_t)native->cursor >= UINT64_C(0x800000000000) ||
            VirtualQuery((LPCVOID)native->cursor, &info, sizeof(info)) != sizeof(info))
        { native->complete = 1; break; }
        if (++native->regions > 200000u || info.RegionSize == 0u ||
            (uintptr_t)info.BaseAddress > UINTPTR_MAX - info.RegionSize) return 0;
        next = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (next <= native->cursor) return 0;
        if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE &&
            (info.Protect == PAGE_READONLY || info.Protect == PAGE_READWRITE) &&
            info.BaseAddress == info.AllocationBase &&
            native->api->read((uintptr_t)info.AllocationBase, header, sizeof(header)) == 0 &&
            memcmp(header, HF_SAI_DELTA_HEADER, sizeof(header)) == 0)
        {
            native->candidate = (uintptr_t)info.AllocationBase;
            if (++native->candidates > 1u) return 0;
        }
        native->cursor = next;
    }
    if (!native->complete) return 2;
    if (native->candidates == 0u)
    {
        native->cursor=(uintptr_t)65536u;
        native->regions=0u;
        native->complete=0;
        return 2; /* Late entity-delta data may arrive within the shared deadline. */
    }
    if (native->candidates != 1u) return 0;
    HF_SAI_ATTACHMENT_STATE.row = native->candidate;
    return 1;
}

static int hf_sai_native_read(void* context, uintptr_t address, void* output, size_t size)
{
    HfSaiAttachmentNative* native = context;
    return native->api->read(address, output, size);
}

static int hf_sai_native_write(void* context, uintptr_t address, const void* input, size_t size)
{
    HfSaiAttachmentNative* native = context;
    MEMORY_BASIC_INFORMATION info;
    DWORD previous = 0u, ignored = 0u;
    int result;
    if (input == NULL || size != 4u ||
        !sickle_native_data_access(NULL, address, size, 0) ||
        VirtualQuery((LPCVOID)address, &info, sizeof(info)) != sizeof(info)) return 1;
    if (info.Protect == PAGE_READONLY &&
        !VirtualProtect((LPVOID)address, size, PAGE_READWRITE, &previous)) return 1;
    result = native->api->write(address, input, size);
    if (previous != 0u && !VirtualProtect((LPVOID)address, size, previous, &ignored)) return -2;
    return result;
}

static int hf_sai_native_access(void* context, uintptr_t address, size_t size)
{
    (void)context;
    return sickle_native_data_access(NULL, address, size, 0);
}

static int hf_sai_native_hash(void* context, const void* data, size_t size, char output[65])
{
    HfSaiAttachmentNative* native = context;
    return sickle_crypto_init(&native->crypto) && sickle_hash_bytes(&native->crypto, data, size, output);
}

static int hf_sai_native_companion_owned(void* context, unsigned which)
{
    HfSaiAttachmentNative* native = context;
    return hf_sai_sdk_companion_owned(native->api,native->mod,which);
}

static HfSaiAttachmentOps hf_sai_attachment_ops(const HfApi* api, HfMod* mod)
{
    HfSaiAttachmentNative* native = &HF_SAI_ATTACHMENT_NATIVE;
    native->api = api;
    native->mod = mod;
    return (HfSaiAttachmentOps){native,hf_sai_native_read,hf_sai_native_write,
        hf_sai_native_access,hf_sai_native_hash,hf_sai_native_companion_owned};
}
#endif

static int hf_sai_attachment_preflight(const HfApi* api, HfMod* mod, int* not_ready,
                                       char* reason, size_t capacity)
{
    HfSaiAttachmentOps ops = hf_sai_attachment_ops(api,mod);
    int found = HF_SAI_ATTACHMENT_STATE.row != 0u ? 1 : hf_sai_attachment_find();
    if (found == 2 && not_ready != NULL) *not_ready = 1;
    if (found != 1 || HF_SAI_ATTACHMENT_STATE.attempted != 0u ||
        !hf_sai_attachment_proof(&ops,&HF_SAI_ATTACHMENT_STATE))
    {
        (void)snprintf(reason,capacity,"SAI attachment table is pending, ambiguous, foreign, or changed");
        return 0;
    }
    return 1;
}

static int hf_sai_attachment_change(const HfApi* api, HfMod* mod, unsigned field,
                                    int restore, char* reason, size_t capacity)
{
    HfSaiAttachmentOps ops = hf_sai_attachment_ops(api,mod);
    int result = restore ? hf_sai_attachment_reset(&ops,&HF_SAI_ATTACHMENT_STATE,field) :
                          hf_sai_attachment_apply(&ops,&HF_SAI_ATTACHMENT_STATE,field);
    if (result <= 0)
        (void)snprintf(reason,capacity,"%sSAI attachment field %u failed exact proof, write or rollback",
                       HF_SAI_ATTACHMENT_STATE.poisoned ? "FATAL: " : "",field+1u);
    return result;
}
