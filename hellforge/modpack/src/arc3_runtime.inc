/* Exact-build ARC-3 source discovery and bounded Rapid-mode startup runner. */
#define HF_ARC3_DAMAGE_TABLE_SIZE 49424u
#define HF_ARC3_DAMAGE_ROW_OFFSET 15072u
#define HF_ARC3_ARC_TABLE_SIZE 1708u
#define HF_ARC3_ARC_ROW_OFFSET 44u
#define HF_ARC3_CHARGE_GROUP_BACK 1644u
#define HF_ARC3_CHARGE_GROUP_SIZE 2724u
#define HF_ARC3_PRIVATE_REGION_SIZE 46616576u
#define HF_ARC3_CHARGE_IN_REGION 7566708u
#define HF_ARC3_WEAPON_IN_REGION 26566228u
#define HF_ARC3_SCAN_BLOCK_SIZE 65536u
#define HF_ARC3_SCAN_SECTION_LIMIT 33554432u
#define HF_ARC3_SCAN_REGIONS_PER_TICK 256u

typedef struct HfArc3ModuleDiscovery
{
    uintptr_t module;
    uintptr_t section_table;
    size_t module_size;
    uint32_t image_size;
    uint32_t section_count;
    uint32_t section_index;
    uint32_t section_rva;
    uint32_t section_size;
    size_t section_offset;
    size_t section_total;
    size_t block_length;
    size_t block_slot;
    uintptr_t damage_candidates[512];
    uintptr_t arc_candidates[512];
    size_t damage_candidate_count;
    size_t arc_candidate_count;
    size_t damage_candidate_index;
    size_t arc_candidate_index;
    size_t damage_valid_count;
    size_t arc_valid_count;
    uintptr_t damage_row;
    uintptr_t arc_row;
    uint8_t block[HF_ARC3_SCAN_BLOCK_SIZE];
    int initialized;
    int scanning_complete;
    int validating_complete;
    int failed;
} HfArc3ModuleDiscovery;

typedef struct HfArc3PrivateDiscovery
{
    uintptr_t cursor;
    size_t regions;
    size_t charge_count;
    size_t weapon_count;
    uintptr_t charge_row;
    uintptr_t weapon_row;
    int initialized;
    int complete;
    int failed;
} HfArc3PrivateDiscovery;

typedef struct HfArc3Runner
{
    const HfApi* api;
    HfMod* mod;
    HfArc3MemoryOps memory;
    HfSickleCrypto crypto;
    HfArc3ScRuntime supercharge;
    HfArc3ModuleDiscovery module_discovery;
    HfArc3PrivateDiscovery private_discovery;
    uintptr_t executable;
    uintptr_t game;
    uint64_t next_attempt_ms;
    uint64_t settle_until_ms;
    unsigned readiness_attempts;
    unsigned data_attempts;
    int registered;
    int hashes_checked;
    int done;
    char last_message[512];
} HfArc3Runner;

static HfArc3Runner HF_ARC3_RUNNER;

static uint32_t hf_arc3_u32(const uint8_t* bytes, size_t offset)
{
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1u] << 8) |
           ((uint32_t)bytes[offset + 2u] << 16) | ((uint32_t)bytes[offset + 3u] << 24);
}

static uint64_t hf_arc3_u64(const uint8_t* bytes, size_t offset)
{
    uint64_t value = 0u;
    memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

static int hf_arc3_api_read(void* context, uintptr_t address, void* output, size_t size)
{
    HfArc3Runner* runner = (HfArc3Runner*)context;
    return runner != NULL && runner->api->read != NULL &&
           runner->api->read(address, output, size) == 0;
}

static int hf_arc3_api_write(void* context, uintptr_t address, const void* input, size_t size,
                             int protected_write)
{
    HfArc3Runner* runner = (HfArc3Runner*)context;
    return runner != NULL && runner->api->write != NULL &&
           sickle_native_page_access(address, size, protected_write ? 0 : 1,
                                     MEM_PRIVATE, MEM_PRIVATE) &&
           runner->api->write(address, input, size) == 0;
}

static int hf_arc3_api_can_write(void* context, uintptr_t address, size_t size, int protected_write)
{
    (void)context;
    return sickle_native_page_access(address, size, protected_write ? 0 : 1,
                                     MEM_PRIVATE, MEM_PRIVATE);
}

static int hf_arc3_private_access(void* context, uintptr_t address, size_t size)
{
    (void)context;
    return sickle_native_data_access(NULL, address, size, 0);
}

static void hf_arc3_log_once(HfArc3Runner* runner, const char* message)
{
    if (runner == NULL || message == NULL || strcmp(runner->last_message, message) == 0)
        return;
    (void)snprintf(runner->last_message, sizeof(runner->last_message), "%s", message);
    runner->api->log(runner->mod, "%s", message);
}

static int hf_arc3_valid_damage_table(HfArc3Runner* runner, uintptr_t table);
static int hf_arc3_valid_arc_table(HfArc3Runner* runner, uintptr_t table);
static int hf_arc3_valid_charge_record(HfArc3Runner* runner, uintptr_t address);

static int hf_arc3_header_matches(const HfApi* api, uintptr_t address,
                                  const uint8_t* header, int damage_header)
{
    uint8_t bytes[16];
    if (api->read(address, bytes, sizeof(bytes)) != 0)
        return 0;
    if (header != NULL)
        return memcmp(bytes, header, sizeof(bytes)) == 0;
    return damage_header && hf_arc3_u32(bytes, 0u) == 2u &&
           memcmp(bytes + 4u, "LDLD", 4u) == 0 &&
           hf_arc3_u32(bytes, 12u) == 3943969754u;
}

static int hf_arc3_add_candidate(uintptr_t* candidates, size_t* count, size_t capacity,
                                 uintptr_t address)
{
    size_t i;
    for (i = 0u; i < *count; ++i)
        if (candidates[i] == address)
            return 1;
    if (*count >= capacity)
        return 0;
    candidates[(*count)++] = address;
    return 1;
}

static int hf_arc3_module_discovery_initialize(HfArc3Runner* runner)
{
    HfArc3ModuleDiscovery* scan = &runner->module_discovery;
    uint8_t dos[64], nt[264];
    uint32_t nt_offset, image_size;
    uint16_t sections, optional_size;
    uintptr_t section_table;
    scan->module = runner->api->module_base("game.dll", &scan->module_size);
    if (scan->module == 0u || scan->module_size < sizeof(dos) ||
        runner->api->read(scan->module, dos, sizeof(dos)) != 0 ||
        dos[0] != 'M' || dos[1] != 'Z')
        goto fail;
    nt_offset = hf_arc3_u32(dos, 60u);
    if (nt_offset >= 1048576u || (size_t)nt_offset + sizeof(nt) > scan->module_size ||
        runner->api->read(scan->module + nt_offset, nt, sizeof(nt)) != 0 ||
        memcmp(nt, "PE\0\0", 4u) != 0 || nt[24] != 0x0Bu || nt[25] != 0x02u)
        goto fail;
    sections = (uint16_t)nt[6] | ((uint16_t)nt[7] << 8);
    optional_size = (uint16_t)nt[20] | ((uint16_t)nt[21] << 8);
    image_size = hf_arc3_u32(nt, 80u);
    if (sections == 0u || sections > 96u || optional_size < 112u || optional_size > 512u ||
        image_size == 0u || image_size > scan->module_size)
        goto fail;
    section_table = scan->module + nt_offset + 24u + optional_size;
    if (section_table < scan->module ||
        (size_t)(section_table - scan->module) + (size_t)sections * 40u > scan->module_size)
        goto fail;
    scan->section_table = section_table;
    scan->image_size = image_size;
    scan->section_count = sections;
    scan->initialized = 1;
    return 1;

fail:
    scan->failed = 1;
    return -1;
}

static int hf_arc3_module_discovery_step(HfArc3Runner* runner)
{
    HfArc3ModuleDiscovery* scan = &runner->module_discovery;
    LARGE_INTEGER start, now, frequency = {0};
    unsigned inspected = 0u;
    if (scan->failed)
        return -1;
    if (scan->scanning_complete)
        return 1;
    if (!scan->initialized && hf_arc3_module_discovery_initialize(runner) < 0)
        return -1;
    (void)QueryPerformanceFrequency(&frequency);
    (void)QueryPerformanceCounter(&start);
    while (!scan->scanning_complete)
    {
        if (scan->section_size == 0u)
        {
            uint8_t section[40];
            uint32_t flags;
            while (scan->section_index < scan->section_count)
            {
                uintptr_t address = scan->section_table + (uintptr_t)scan->section_index * sizeof(section);
                uint32_t size, rva;
                ++scan->section_index;
                if (runner->api->read(address, section, sizeof(section)) != 0)
                    goto fail;
                size = hf_arc3_u32(section, 8u);
                rva = hf_arc3_u32(section, 12u);
                flags = hf_arc3_u32(section, 36u);
                if ((flags & 0x40000000u) == 0u || (flags & 0x80000000u) == 0u ||
                    (flags & 0x20000000u) != 0u)
                    continue;
                if (size > HF_ARC3_SCAN_SECTION_LIMIT ||
                    (uint64_t)rva + size > scan->image_size ||
                    scan->section_total + size > HF_ARC3_SCAN_SECTION_LIMIT)
                    goto fail;
                scan->section_total += size;
                scan->section_rva = rva;
                scan->section_size = size;
                scan->section_offset = 0u;
                break;
            }
            if (scan->section_size == 0u && scan->section_index >= scan->section_count)
            {
                scan->scanning_complete = 1;
                break;
            }
            if (scan->section_size == 0u)
                continue;
        }
        if (scan->block_length == 0u)
        {
            size_t remaining = scan->section_size - scan->section_offset;
            scan->block_length = remaining > sizeof(scan->block) ? sizeof(scan->block) : remaining;
            scan->block_slot = 0u;
            if (scan->block_length == 0u ||
                runner->api->read(scan->module + scan->section_rva + scan->section_offset,
                                  scan->block, scan->block_length) != 0)
                goto fail;
        }
        while (scan->block_slot + sizeof(uintptr_t) <= scan->block_length)
        {
            size_t slot = scan->block_slot;
            uint64_t value = hf_arc3_u64(scan->block, slot);
            scan->block_slot += sizeof(uintptr_t);
            ++inspected;
            if (value > UINT64_C(65536) && value < UINT64_C(0x800000000000))
            {
                uintptr_t candidate = (uintptr_t)value;
                if (hf_arc3_header_matches(runner->api, candidate, NULL, 1) &&
                    !hf_arc3_add_candidate(scan->damage_candidates,
                                           &scan->damage_candidate_count, 512u, candidate))
                    goto fail;
                if (hf_arc3_header_matches(runner->api, candidate, HF_ARC3_ARC_HEADER, 0) &&
                    !hf_arc3_add_candidate(scan->arc_candidates,
                                           &scan->arc_candidate_count, 512u, candidate))
                    goto fail;
            }
            if (inspected >= 256u)
            {
                inspected = 0u;
                if (frequency.QuadPart > 0 &&
                    QueryPerformanceCounter(&now) != 0 &&
                    (now.QuadPart - start.QuadPart) * 1000 / frequency.QuadPart >= 4)
                    return 0;
            }
        }
        if (scan->block_slot + sizeof(uintptr_t) > scan->block_length)
        {
            scan->section_offset += scan->block_length;
            scan->block_length = 0u;
            scan->block_slot = 0u;
            if (scan->section_offset >= scan->section_size)
            {
                scan->section_size = 0u;
                scan->section_offset = 0u;
            }
        }
        if (frequency.QuadPart > 0 && QueryPerformanceCounter(&now) != 0 &&
            (now.QuadPart - start.QuadPart) * 1000 / frequency.QuadPart >= 4)
            return 0;
    }
    return scan->scanning_complete ? 1 : 0;

fail:
    scan->failed = 1;
    return -1;
}

static int hf_arc3_module_validation_step(HfArc3Runner* runner,
                                          uintptr_t* damage_row, uintptr_t* arc_row)
{
    HfArc3ModuleDiscovery* scan = &runner->module_discovery;
    size_t budget = 4u;
    if (!scan->scanning_complete || scan->failed)
        return -1;
    if (scan->validating_complete)
    {
        if (scan->damage_valid_count == 1u && scan->arc_valid_count == 1u)
        {
            *damage_row = scan->damage_row;
            *arc_row = scan->arc_row;
            return 1;
        }
        return scan->damage_valid_count > 1u || scan->arc_valid_count > 1u ? -1 : 0;
    }
    while (scan->damage_candidate_index < scan->damage_candidate_count && budget > 0u)
    {
        uintptr_t candidate = scan->damage_candidates[scan->damage_candidate_index++];
        if (hf_arc3_valid_damage_table(runner, candidate))
        {
            ++scan->damage_valid_count;
            scan->damage_row = candidate + HF_ARC3_DAMAGE_ROW_OFFSET;
        }
        --budget;
    }
    while (scan->arc_candidate_index < scan->arc_candidate_count && budget > 0u)
    {
        uintptr_t candidate = scan->arc_candidates[scan->arc_candidate_index++];
        if (hf_arc3_valid_arc_table(runner, candidate))
        {
            ++scan->arc_valid_count;
            scan->arc_row = candidate + HF_ARC3_ARC_ROW_OFFSET;
        }
        --budget;
    }
    if (scan->damage_candidate_index < scan->damage_candidate_count ||
        scan->arc_candidate_index < scan->arc_candidate_count)
        return 2;
    scan->validating_complete = 1;
    if (scan->damage_valid_count == 1u && scan->arc_valid_count == 1u)
    {
        *damage_row = scan->damage_row;
        *arc_row = scan->arc_row;
        return 1;
    }
    return scan->damage_valid_count > 1u || scan->arc_valid_count > 1u ? -1 : 0;
}

static int hf_arc3_private_discovery_step(HfArc3Runner* runner,
                                          uintptr_t* charge, uintptr_t* weapon)
{
    HfArc3PrivateDiscovery* scan = &runner->private_discovery;
    unsigned queried_regions = 0u;
    if (scan->failed)
        return -1;
    if (!scan->initialized)
    {
        scan->cursor = (uintptr_t)65536u;
        scan->initialized = 1;
    }
    if (scan->complete)
        return scan->charge_count == 1u && scan->weapon_count == 1u ?
               (*charge = scan->charge_row, *weapon = scan->weapon_row, 1) :
               (scan->charge_count > 1u || scan->weapon_count > 1u ? -1 : 0);
    while ((uint64_t)scan->cursor < UINT64_C(0x800000000000) &&
           queried_regions < HF_ARC3_SCAN_REGIONS_PER_TICK)
    {
        MEMORY_BASIC_INFORMATION info;
        SIZE_T queried = VirtualQuery((LPCVOID)scan->cursor, &info, sizeof(info));
        uintptr_t next;
        if (queried != sizeof(info) || info.RegionSize == 0u)
        {
            scan->complete = 1;
            break;
        }
        next = (uintptr_t)info.BaseAddress + (uintptr_t)info.RegionSize;
        if (next <= scan->cursor)
        {
            scan->failed = 1;
            return -1;
        }
        ++scan->regions;
        ++queried_regions;
        if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE &&
            info.Protect == PAGE_READONLY && info.RegionSize == HF_ARC3_PRIVATE_REGION_SIZE)
        {
            uintptr_t base = (uintptr_t)info.BaseAddress;
            uintptr_t candidate_charge = base + HF_ARC3_CHARGE_IN_REGION;
            uintptr_t candidate_weapon = base + HF_ARC3_WEAPON_IN_REGION;
            if (hf_arc3_valid_charge_record(runner, candidate_charge))
            {
                scan->charge_row = candidate_charge;
                if (++scan->charge_count > 1u)
                    return -1;
            }
            if (sickle_native_data_access(NULL, candidate_weapon, sizeof(HF_ARC3_WEAPON_ROW), 0))
            {
                uint8_t row[sizeof(HF_ARC3_WEAPON_ROW)];
                if (runner->api->read(candidate_weapon, row, sizeof(row)) == 0 &&
                    memcmp(row, HF_ARC3_WEAPON_ROW, sizeof(row)) == 0)
                {
                    scan->weapon_row = candidate_weapon;
                    if (++scan->weapon_count > 1u)
                        return -1;
                }
            }
        }
        scan->cursor = next;
    }
    if (scan->regions >= 262144u && (uint64_t)scan->cursor < UINT64_C(0x800000000000))
        return -1;
    if ((uint64_t)scan->cursor >= UINT64_C(0x800000000000))
        scan->complete = 1;
    if (!scan->complete)
        return 2;
    if (scan->charge_count == 1u && scan->weapon_count == 1u)
    {
        *charge = scan->charge_row;
        *weapon = scan->weapon_row;
        return 1;
    }
    return scan->charge_count > 1u || scan->weapon_count > 1u ? -1 : 0;
}

static void hf_arc3_reset_discovery(HfArc3Runner* runner)
{
    memset(&runner->module_discovery, 0, sizeof(runner->module_discovery));
    memset(&runner->private_discovery, 0, sizeof(runner->private_discovery));
}

static int hf_arc3_valid_damage_table(HfArc3Runner* runner, uintptr_t table)
{
    static const uint8_t header_a[24] = {
        0x4Cu,0x44u,0x4Cu,0x44u,0x01u,0x00u,0x00u,0x00u,0xDAu,0x33u,0x14u,0xEBu,
        0x20u,0x00u,0x00u,0x00u,0x01u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u
    };
    static const uint8_t header_b[24] = {
        0x4Cu,0x44u,0x4Cu,0x44u,0x01u,0x00u,0x00u,0x00u,0xF0u,0x2Cu,0xA7u,0xE0u,
        0xBCu,0xC0u,0x00u,0x00u,0x01u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u
    };
    uint8_t* bytes;
    uintptr_t row_pointer;
    int valid = 0;
    if (!sickle_native_data_access(NULL, table, HF_ARC3_DAMAGE_TABLE_SIZE, 1))
        return 0;
    bytes = (uint8_t*)malloc(HF_ARC3_DAMAGE_TABLE_SIZE);
    if (bytes == NULL)
        return 0;
    if (runner->api->read(table, bytes, HF_ARC3_DAMAGE_TABLE_SIZE) == 0 &&
        hf_arc3_u32(bytes, 0u) == 2u &&
        memcmp(bytes + 4u, header_a, sizeof(header_a)) == 0 &&
        memcmp(bytes + 60u, header_b, sizeof(header_b)) == 0 &&
        hf_arc3_u32(bytes, 92u) == 649u && hf_arc3_u32(bytes, 96u) == 0u)
    {
        memcpy(&row_pointer, bytes + 84u, sizeof(row_pointer));
        valid = row_pointer == table + 100u &&
                memcmp(bytes + HF_ARC3_DAMAGE_ROW_OFFSET, HF_ARC3_DAMAGE_ROW,
                       sizeof(HF_ARC3_DAMAGE_ROW)) == 0;
    }
    free(bytes);
    return valid;
}

static int hf_arc3_valid_arc_table(HfArc3Runner* runner, uintptr_t table)
{
    uint8_t bytes[HF_ARC3_ARC_TABLE_SIZE];
    uintptr_t row_pointer;
    if (!sickle_native_data_access(NULL, table, sizeof(bytes), 0) ||
        runner->api->read(table, bytes, sizeof(bytes)) != 0 ||
        memcmp(bytes, HF_ARC3_ARC_HEADER, sizeof(HF_ARC3_ARC_HEADER)) != 0 ||
        hf_arc3_u32(bytes, 36u) != 16u || hf_arc3_u32(bytes, 40u) != 0u)
        return 0;
    memcpy(&row_pointer, bytes + 28u, sizeof(row_pointer));
    return row_pointer == table + HF_ARC3_ARC_ROW_OFFSET &&
           memcmp(bytes + HF_ARC3_ARC_ROW_OFFSET, HF_ARC3_ARC_ROW,
                  sizeof(HF_ARC3_ARC_ROW)) == 0;
}

static int hf_arc3_valid_charge_record(HfArc3Runner* runner, uintptr_t address)
{
    uint8_t record[216], group[HF_ARC3_CHARGE_GROUP_SIZE];
    size_t i, owners = 0u;
    if (!sickle_native_data_access(NULL, address - HF_ARC3_CHARGE_GROUP_BACK,
                                   sizeof(group), 0) ||
        runner->api->read(address, record, sizeof(record)) != 0 ||
        (record[184] != 0u && record[184] != 1u))
        return 0;
    if (memcmp(record, HF_ARC3_CHARGE_ROW, 184u) != 0 ||
        memcmp(record + 185u, HF_ARC3_CHARGE_ROW + 185u, sizeof(record) - 185u) != 0 ||
        runner->api->read(address - HF_ARC3_CHARGE_GROUP_BACK, group, sizeof(group)) != 0 ||
        memcmp(group, HF_ARC3_CHARGE_HEADER, sizeof(HF_ARC3_CHARGE_HEADER)) != 0 ||
        memcmp(group + HF_ARC3_CHARGE_GROUP_BACK, record, sizeof(record)) != 0)
        return 0;
    for (i = 0u; i < 20u; ++i)
    {
        size_t offset = 28u + i * 16u;
        if (memcmp(group + offset, HF_ARC3_CHARGE_OWNER, sizeof(HF_ARC3_CHARGE_OWNER)) == 0)
        {
            ++owners;
            if (hf_arc3_u32(group, offset + 8u) != 6u)
                return 0;
        }
    }
    return owners == 1u;
}

static int hf_arc3_find_records(HfArc3Runner* runner, uintptr_t records[4], char* reason,
                                size_t capacity)
{
    HfArc3ModuleDiscovery* scan = &runner->module_discovery;
    int scan_result, table_result, private_result;
    uintptr_t damage_row = 0u, arc_row = 0u, charge_row = 0u, weapon_row = 0u;
    scan_result = hf_arc3_module_discovery_step(runner);
    if (scan_result < 0)
    {
        hf_arc3_reason(reason, capacity, "ARC-3 module-data scan failed or exceeded its bounds");
        return -1;
    }
    if (scan_result == 0)
    {
        hf_arc3_reason(reason, capacity, "Scanning ARC-3 module data in bounded game-thread slices");
        return 2;
    }
    table_result = hf_arc3_module_validation_step(runner, &damage_row, &arc_row);
    if (table_result == 2)
    {
        hf_arc3_reason(reason, capacity, "Validating ARC-3 module-data candidates in bounded slices");
        return 2;
    }
    if (table_result < 0)
    {
        hf_arc3_reason(reason, capacity, "ARC-3 settings table candidates are ambiguous or unreadable");
        return -1;
    }
    if (table_result == 0)
    {
        (void)snprintf(reason, capacity,
                       "ARC-3 settings tables not found (damage candidates %u/%u valid, arc candidates %u/%u valid)",
                       (unsigned)scan->damage_valid_count, (unsigned)scan->damage_candidate_count,
                       (unsigned)scan->arc_valid_count, (unsigned)scan->arc_candidate_count);
        return 0;
    }
    private_result = hf_arc3_private_discovery_step(runner, &charge_row, &weapon_row);
    if (private_result == 2)
    {
        hf_arc3_reason(reason, capacity, "Scanning ARC-3 private records in bounded region batches");
        return 2;
    }
    if (private_result < 0)
    {
        hf_arc3_reason(reason, capacity, "ARC-3 charge/weapon private records are ambiguous or unreadable");
        return -1;
    }
    if (private_result == 0)
    {
        hf_arc3_reason(reason, capacity, "ARC-3 charge/weapon records are not ready yet");
        return 0;
    }
    records[0] = damage_row;
    records[1] = arc_row;
    records[2] = charge_row;
    records[3] = weapon_row;
    return 1;
}

static int hf_arc3_verify_modules(HfArc3Runner* runner, char* reason, size_t capacity)
{
    static const char expected_exe[] = "F5FEE03DCFDB2E553A4752C283590950AC13316B376D8196AA556FF0400D5F06";
    static const char expected_game[] = "2E2C3B7C2500646DADD5F2B4C6E0504DBB7E7896139F64CDDC0D1813C718F51E";
    size_t executable_size = 0u, game_size = 0u;
    char exe_hash[65], game_hash[65];
    runner->executable = runner->api->module_base("helldivers2.exe", &executable_size);
    runner->game = runner->api->module_base("game.dll", &game_size);
    if (runner->executable == 0u || runner->game == 0u || executable_size == 0u || game_size == 0u)
    {
        hf_arc3_reason(reason, capacity, "Helldivers 2 executable or game.dll is not loaded yet");
        return 0;
    }
    if (!sickle_hash_module_file(&runner->crypto, runner->executable, exe_hash) ||
        !sickle_hash_module_file(&runner->crypto, runner->game, game_hash))
    {
        hf_arc3_reason(reason, capacity, "Could not hash the ARC-3 executable and game.dll");
        return 0;
    }
    if (strcmp(exe_hash, expected_exe) != 0 || strcmp(game_hash, expected_game) != 0)
    {
        hf_arc3_reason(reason, capacity, "Unsupported executable or game.dll hash; no ARC-3 edits were applied");
        return -1;
    }
    return 1;
}

static void hf_arc3_native_tick(HfMod* mod, float dt, void* user)
{
    HfArc3Runner* runner = (HfArc3Runner*)user;
    uint64_t now = GetTickCount64();
    char reason[512];
    int result;
    (void)mod;
    (void)dt;
    if (runner == NULL || runner->done)
        return;
    if (runner->supercharge.prepared)
    {
        if (!hf_arc3_sc_synchronize(&runner->memory, &runner->supercharge))
            runner->done = 1;
        if (runner->supercharge.status[0] != '\0')
            hf_arc3_log_once(runner, runner->supercharge.status);
        return;
    }
    if (now < runner->next_attempt_ms)
        return;
    reason[0] = '\0';
    if (!runner->hashes_checked)
    {
        result = hf_arc3_verify_modules(runner, reason, sizeof(reason));
        if (result < 0)
        {
            runner->done = 1;
            hf_arc3_log_once(runner, reason);
            return;
        }
        if (result == 0)
        {
            ++runner->readiness_attempts;
            if (runner->readiness_attempts >= 8u)
            {
                runner->done = 1;
                hf_arc3_log_once(runner, "Refused ARC-3: executable/game module readiness exceeded eight attempts");
            }
            else
            {
                runner->next_attempt_ms = now + 1000u;
                hf_arc3_log_once(runner, "Waiting for ARC-3 executable and game.dll readiness");
            }
            return;
        }
        runner->hashes_checked = 1;
        runner->settle_until_ms = now + 30000u;
        hf_arc3_log_once(runner, "ARC-3 module hashes match; waiting 30 seconds for game and companion data to settle");
        return;
    }
    if (now < runner->settle_until_ms)
        return;
    {
        uintptr_t records[4] = {0u, 0u, 0u, 0u};
        uint32_t changed = 0u;
        result = hf_arc3_find_records(runner, records, reason, sizeof(reason));
        if (result == 2)
        {
            hf_arc3_log_once(runner, reason);
            return;
        }
        if (result == 0)
        {
            ++runner->data_attempts;
            if (runner->data_attempts < 6u)
            {
                hf_arc3_reset_discovery(runner);
                runner->next_attempt_ms = now + 10000u;
                hf_arc3_log_once(runner, reason);
                return;
            }
            runner->done = 1;
            runner->api->log(runner->mod, "Refused ARC-3 after six bounded data-discovery attempts: %s", reason);
            return;
        }
        if (result < 0)
        {
            runner->done = 1;
            runner->api->log(runner->mod, "Refused ARC-3 before mutation: %s", reason);
            return;
        }
        result = hf_arc3_apply_rapid(&runner->memory, records, &changed, reason, sizeof(reason));
        if (result == HF_ARC3_APPLIED)
        {
            uint32_t supercharge_changed = 0u;
            int prepared = hf_arc3_sc_prepare(&runner->memory, runner->game,
                                              records[0], records[1], records[2], records[3],
                                              &runner->supercharge.profiles,
                                              &supercharge_changed, reason, sizeof(reason));
            if (prepared != HF_ARC3_APPLIED)
            {
                runner->done = 1;
                runner->api->log(runner->mod,
                                 "ARC-3 Rapid profile applied (%u fields); Supercharge preparation stopped: %s",
                                 changed, reason);
                return;
            }
            runner->supercharge.prepared = 1u;
            runner->api->log(runner->mod,
                             "ARC-3 Rapid profile and Supercharge selector prepared (%u + %u guarded fields)",
                             changed, supercharge_changed);
            if (!hf_arc3_sc_synchronize(&runner->memory, &runner->supercharge))
                runner->done = 1;
            if (runner->supercharge.status[0] != '\0')
                hf_arc3_log_once(runner, runner->supercharge.status);
        }
        else
        {
            runner->done = 1;
            runner->api->log(runner->mod, "%s", reason);
        }
    }
}

static void hf_arc3_begin_native(const HfApi* api, HfMod* mod)
{
    HfArc3Runner* runner = &HF_ARC3_RUNNER;
    if (runner->registered || runner->done)
        return;
    runner->api = api;
    runner->mod = mod;
    runner->memory.context = runner;
    runner->memory.read = hf_arc3_api_read;
    runner->memory.write = hf_arc3_api_write;
    runner->memory.can_write = hf_arc3_api_can_write;
    runner->memory.data_access = hf_arc3_private_access;
    if (api->size < (uint32_t)(offsetof(HfApi, on_game_thread) + sizeof(api->on_game_thread)) ||
        api->on_game_thread == NULL || api->module_base == NULL || api->read == NULL || api->write == NULL ||
        api->on_game_thread(mod, hf_arc3_native_tick, runner) == 0)
    {
        runner->done = 1;
        api->log(mod, "Refused ARC-3: required HellForge module-memory/game-thread APIs are unavailable");
        return;
    }
    runner->registered = 1;
    api->log(mod, "ARC-3 Rapid source validation queued on the game thread");
}
