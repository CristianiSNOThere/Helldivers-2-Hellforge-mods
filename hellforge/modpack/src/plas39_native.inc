/* HellForge game-thread runner for the source-pinned PLAS-39 mode runtime. */
#include "plas39_profile.inc"
#include "plas39_runtime.inc"

typedef struct HfPlas39Runner
{
    const HfApi* api;
    HfMod* mod;
    HfPlas39MemoryOps memory;
    HfPlas39Plan plan;
    HfPlas39Transaction transaction;
    HfPlas39Instance* instances;
    HfPlas39PreviousMode previous[HF_PLAS39_MAX_INSTANCES];
    HfPlas39Edit* edit_storage;
    HfPlas39EditHashSlot* edit_hash;
    HfSickleCrypto crypto;
    uintptr_t game;
    uint64_t next_attempt_ms;
    uint64_t retry_since_ms;
    size_t previous_count;
    unsigned module_attempts;
    unsigned prepare_attempts;
    int registered;
    int hashes_checked;
    int prepared;
    int done;
    int fatal_write;
    char last_message[512];
    char last_status[256];
} HfPlas39Runner;

static HfPlas39Runner HF_PLAS39_RUNNER;

static int hf_plas39_api_read(void* context, uintptr_t address, void* output, size_t size)
{
    HfPlas39Runner* runner = (HfPlas39Runner*)context;
    return runner != NULL && runner->api->read != NULL &&
           runner->api->read(address, output, size) == 0;
}

static int hf_plas39_api_data_access(void* context, uintptr_t address, size_t size)
{
    (void)context;
    return sickle_native_data_access(NULL, address, size, 0);
}

static int hf_plas39_api_can_write(void* context, uintptr_t address, size_t size,
                                    int protected_write)
{
    (void)context;
    return sickle_native_page_access(address, size, protected_write ? 0 : 1,
                                     MEM_PRIVATE, MEM_PRIVATE);
}

static int hf_plas39_api_write(void* context, uintptr_t address, const void* input, size_t size,
                                int protected_write)
{
    HfPlas39Runner* runner = (HfPlas39Runner*)context;
    MEMORY_BASIC_INFORMATION info;
    DWORD previous = 0u, ignored = 0u;
    int wrote;
    if (runner == NULL || runner->api->write == NULL || input == NULL || size != 4u ||
        !sickle_native_data_access(NULL, address, size, 0) ||
        VirtualQuery((LPCVOID)address, &info, sizeof(info)) != sizeof(info) ||
        info.Type != MEM_PRIVATE || info.State != MEM_COMMIT)
        return 0;
    if (info.Protect == PAGE_READWRITE)
        return runner->api->write(address, input, size) == 0;
    if (!protected_write || info.Protect != PAGE_READONLY ||
        VirtualProtect((LPVOID)address, size, PAGE_READWRITE, &previous) == 0)
        return 0;
    wrote = runner->api->write(address, input, size) == 0;
    if (VirtualProtect((LPVOID)address, size, previous, &ignored) == 0)
    {
        runner->fatal_write = 1;
        return 0;
    }
    return wrote;
}

static void hf_plas39_log_once(HfPlas39Runner* runner, const char* message)
{
    if (runner == NULL || message == NULL || strcmp(runner->last_message, message) == 0)
        return;
    (void)snprintf(runner->last_message, sizeof(runner->last_message), "%s", message);
    runner->api->log(runner->mod, "%s", message);
}

static int hf_plas39_check_modules(HfPlas39Runner* runner, char* reason, size_t capacity)
{
    uintptr_t executable;
    size_t executable_size = 0u, game_size = 0u;
    char exe_hash[65], game_hash[65];
    uint8_t bytes[HF_PLAS39_PIN_MAX_BYTES];
    size_t i;
    if (runner == NULL || runner->api->module_base == NULL || runner->api->read == NULL)
    {
        (void)snprintf(reason, capacity, "HellForge module-memory APIs are unavailable");
        return -1;
    }
    executable = runner->api->module_base("helldivers2.exe", &executable_size);
    runner->game = runner->api->module_base("game.dll", &game_size);
    if (executable == 0u || executable_size == 0u || runner->game == 0u || game_size == 0u)
    {
        (void)snprintf(reason, capacity, "Helldivers 2 executable or game.dll is unavailable");
        return 0;
    }
    if (!sickle_hash_module_file(&runner->crypto, executable, exe_hash) ||
        !sickle_hash_module_file(&runner->crypto, runner->game, game_hash))
    {
        (void)snprintf(reason, capacity, "Could not verify the PLAS-39 executable and game.dll hashes");
        return 0;
    }
    if (strcmp(exe_hash, HF_PLAS39_EXE_SHA256) != 0 || strcmp(game_hash, HF_PLAS39_GAME_SHA256) != 0)
    {
        (void)snprintf(reason, capacity, "Unsupported PLAS-39 executable or game.dll hash; no PLAS-39 mode edits were applied");
        return -1;
    }
    if (runner->crypto.library != NULL)
    {
        (void)FreeLibrary(runner->crypto.library);
        memset(&runner->crypto, 0, sizeof(runner->crypto));
    }
    for (i = 0u; i < HF_PLAS39_PIN_COUNT; ++i)
    {
        const HfPlas39CodePin* pin = &HF_PLAS39_CODE_PINS[i];
        if ((uint64_t)pin->rva + pin->size > game_size || pin->size > sizeof(bytes) ||
            runner->api->read(runner->game + pin->rva, bytes, pin->size) != 0 ||
            memcmp(bytes, pin->bytes, pin->size) != 0)
        {
            (void)snprintf(reason, capacity, "PLAS-39 native firing code changed at pin %u; no mode edits were applied",
                           (unsigned)i);
            return -1;
        }
    }
    return 1;
}

static int hf_plas39_allocate(HfPlas39Runner* runner)
{
    if (runner->instances != NULL && runner->edit_storage != NULL && runner->edit_hash != NULL)
        return 1;
    runner->instances = (HfPlas39Instance*)calloc(HF_PLAS39_MAX_INSTANCES,
                                                   sizeof(*runner->instances));
    runner->edit_storage = (HfPlas39Edit*)calloc(HF_PLAS39_MAX_EDITS,
                                                  sizeof(*runner->edit_storage));
    runner->edit_hash = (HfPlas39EditHashSlot*)calloc(HF_PLAS39_EDIT_HASH_SIZE,
                                                       sizeof(*runner->edit_hash));
    if (runner->instances == NULL || runner->edit_storage == NULL || runner->edit_hash == NULL ||
        !hf_plas39_transaction_init(&runner->transaction, runner->edit_storage,
                                     HF_PLAS39_MAX_EDITS, runner->edit_hash,
                                     HF_PLAS39_EDIT_HASH_SIZE))
        return 0;
    return 1;
}

static void hf_plas39_native_tick(HfMod* mod, float dt, void* user)
{
    HfPlas39Runner* runner = (HfPlas39Runner*)user;
    uint64_t now = GetTickCount64();
    char reason[512];
    int result;
    (void)mod;
    (void)dt;
    if (runner == NULL || runner->done || now < runner->next_attempt_ms)
        return;
    reason[0] = '\0';
    if (!runner->hashes_checked)
    {
        result = hf_plas39_check_modules(runner, reason, sizeof(reason));
        if (result < 0)
        {
            runner->done = 1;
            hf_plas39_log_once(runner, reason);
            if (runner->crypto.library != NULL)
                (void)FreeLibrary(runner->crypto.library);
            return;
        }
        if (result == 0)
        {
            ++runner->module_attempts;
            if (runner->module_attempts >= 8u)
            {
                char detail[sizeof(reason)];
                (void)snprintf(detail, sizeof(detail), "%s", reason);
                (void)snprintf(reason, sizeof(reason),
                               "Refused PLAS-39: module/hash readiness exceeded eight attempts: %s", detail);
                runner->done = 1;
                hf_plas39_log_once(runner, reason);
            }
            else
            {
                runner->next_attempt_ms = now + 1000u;
                hf_plas39_log_once(runner, "Waiting for PLAS-39 executable and game.dll readiness");
            }
            return;
        }
        runner->hashes_checked = 1;
    }
    if (!hf_plas39_allocate(runner))
    {
        runner->done = 1;
        hf_plas39_log_once(runner, "Refused PLAS-39: bounded native runtime buffers could not be allocated");
        return;
    }
    if (!runner->prepared)
    {
        uint32_t changed = 0u;
        HfPlas39ApplyResult prepared = hf_plas39_native_prepare(&runner->memory, runner->game,
                                                                &runner->transaction, &runner->plan,
                                                                &changed, reason, sizeof(reason));
        if (prepared == HF_PLAS39_NOT_READY)
        {
            ++runner->prepare_attempts;
            if (runner->prepare_attempts >= 60u)
            {
                runner->done = 1;
                (void)snprintf(reason, sizeof(reason),
                               "Refused PLAS-39: native component tables did not become ready after 60 attempts");
                hf_plas39_log_once(runner, reason);
            }
            else
            {
                runner->next_attempt_ms = now + 1000u;
                hf_plas39_log_once(runner, "Waiting for PLAS-39 native component tables");
            }
            return;
        }
        if (prepared != HF_PLAS39_APPLIED)
        {
            runner->done = 1;
            hf_plas39_log_once(runner, reason[0] != '\0' ? reason : "Refused PLAS-39 native profile preparation");
            return;
        }
        runner->prepared = 1;
        (void)snprintf(reason, sizeof(reason),
                       "PLAS-39 guarded global fire-mode profile prepared (%u fields)", changed);
        hf_plas39_log_once(runner, reason);
    }
    {
        size_t count = 0u;
        uint32_t changed = 0u;
        char status[256];
        HfPlas39ApplyResult discovered = hf_plas39_native_discover(&runner->memory, &runner->plan,
                                                                    runner->instances,
                                                                    HF_PLAS39_MAX_INSTANCES,
                                                                    &count, reason, sizeof(reason));
        if (discovered != HF_PLAS39_APPLIED)
        {
            if (discovered == HF_PLAS39_NOT_READY || discovered == HF_PLAS39_RETRY_RACE)
            {
                if (runner->retry_since_ms == 0u)
                {
                    runner->retry_since_ms = now;
                    hf_plas39_log_once(runner, "Waiting for PLAS-39 instance tables to settle");
                }
                if (now - runner->retry_since_ms >= 60000u)
                {
                    runner->done = 1;
                    hf_plas39_log_once(runner, "Refused PLAS-39: instance tables remained unavailable for 60 seconds");
                }
                return;
            }
            runner->done = 1;
            hf_plas39_log_once(runner, reason[0] != '\0' ? reason : "Refused PLAS-39 instance discovery");
            return;
        }
        result = hf_plas39_synchronize(&runner->memory, &runner->plan,
                                       runner->instances, count, runner->previous,
                                       HF_PLAS39_MAX_INSTANCES, &runner->previous_count,
                                       &runner->transaction, &changed, status, sizeof(status));
        if (result == HF_PLAS39_RETRY_RACE || result == HF_PLAS39_NOT_READY)
        {
            if (runner->retry_since_ms == 0u)
            {
                runner->retry_since_ms = now;
                hf_plas39_log_once(runner, status[0] != '\0' ? status : "Waiting for PLAS-39 native state to settle");
            }
            if (now - runner->retry_since_ms >= 60000u)
            {
                runner->done = 1;
                hf_plas39_log_once(runner, "Refused PLAS-39: native mode state remained unstable for 60 seconds");
            }
            return;
        }
        if (result != HF_PLAS39_APPLIED)
        {
            runner->done = 1;
            if (runner->fatal_write)
                hf_plas39_log_once(runner, "FATAL: PLAS-39 page protection could not be restored after a native write");
            else
                hf_plas39_log_once(runner, status[0] != '\0' ? status : "Refused PLAS-39 native mode synchronization");
            return;
        }
        runner->retry_since_ms = 0u;
        if (runner->last_status[0] == '\0' || strcmp(runner->last_status, status) != 0 || changed != 0u)
        {
            (void)snprintf(reason, sizeof(reason), "%s (%u fields updated)", status, changed);
            hf_plas39_log_once(runner, reason);
            (void)snprintf(runner->last_status, sizeof(runner->last_status), "%s", status);
        }
    }
}

static void hf_plas39_begin_native(const HfApi* api, HfMod* mod)
{
    HfPlas39Runner* runner = &HF_PLAS39_RUNNER;
    if (runner->registered || runner->done)
        return;
    runner->api = api;
    runner->mod = mod;
    runner->memory.context = runner;
    runner->memory.read = hf_plas39_api_read;
    runner->memory.write = hf_plas39_api_write;
    runner->memory.can_write = hf_plas39_api_can_write;
    runner->memory.data_access = hf_plas39_api_data_access;
    memset(&runner->crypto, 0, sizeof(runner->crypto));
    if (!hf_plas39_allocate(runner))
    {
        runner->done = 1;
        api->log(mod, "Refused PLAS-39: bounded native runtime buffers could not be allocated");
        return;
    }
    if (api->size < (uint32_t)(offsetof(HfApi, on_game_thread) + sizeof(api->on_game_thread)) ||
        api->on_game_thread == NULL || api->on_game_thread(mod, hf_plas39_native_tick, runner) == 0)
    {
        runner->done = 1;
        api->log(mod, "Refused PLAS-39: HellForge game-thread scheduling is unavailable");
        return;
    }
    runner->registered = 1;
    api->log(mod, "PLAS-39 native runtime queued; it will verify the accepted build and synchronize per-instance modes");
}
