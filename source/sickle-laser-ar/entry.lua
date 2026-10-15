-- HD2-Addon: mods/codex/sickle_laser_ar
local make_api=(function()
return function()
    local ffi = require('ffi')
    assert(ffi.abi('64bit'), 'Requires Windows x64 LuaJIT')
    ffi.cdef [[
        void *GetModuleHandleA(const char *);
        void *GetCurrentProcess(void);
        uint64_t GetTickCount64(void);
        uint32_t GetModuleFileNameW(void *, uint16_t *, uint32_t);
        int ReadProcessMemory(void *, const void *, void *, size_t, size_t *);
        int WriteProcessMemory(void *, void *, const void *, size_t, size_t *);
        size_t VirtualQuery(const void *, void *, size_t);
        int VirtualProtect(void *, size_t, uint32_t, uint32_t *);
        void *CreateFileW(const uint16_t *, uint32_t, uint32_t, void *, uint32_t, uint32_t, void *);
        int ReadFile(void *, void *, uint32_t, uint32_t *, void *);
        int CloseHandle(void *);
        int32_t BCryptOpenAlgorithmProvider(void **, const uint16_t *, const uint16_t *, uint32_t);
        int32_t BCryptCloseAlgorithmProvider(void *, uint32_t);
        int32_t BCryptCreateHash(void *, void **, void *, uint32_t, const void *, uint32_t, uint32_t);
        int32_t BCryptHashData(void *, const void *, uint32_t, uint32_t);
        int32_t BCryptFinishHash(void *, void *, uint32_t, uint32_t);
        int32_t BCryptDestroyHash(void *);
        typedef struct {
            void *base; void *allocation; uint32_t initial_protection;
            uint16_t partition; uint16_t reserved; size_t size;
            uint32_t state; uint32_t protection; uint32_t kind;
        } SickleLaserMemoryInfoV1;
    ]]
    local k, b = ffi.load('kernel32'), ffi.load('bcrypt')
    local self = k.GetCurrentProcess()
    local api = {}
    -- Canonical user addresses are below 2^47 and exactly representable as doubles.
    -- The game may hide every cdata value behind the same tostring result.
    function api.identity(p) return tonumber(ffi.cast('uintptr_t',p)) end
    function api.now() return tonumber(k.GetTickCount64()) end
    function api.module(name) return k.GetModuleHandleA(name) end
    function api.distance(a,c) return tonumber(ffi.cast('intptr_t',a)-ffi.cast('intptr_t',c)) end
    function api.pointer(s,o)
        if not s or o<0 or o+8>#s then return nil end
        local v=ffi.new('uintptr_t[1]'); ffi.copy(v,s:sub(o+1,o+8),8)
        if v[0] < 65536 or v[0] >= 0x800000000000 then return nil end
        return ffi.cast('uint8_t *',v[0])
    end
    function api.read(p,n)
        local buf,got=ffi.new('uint8_t[?]',n),ffi.new('size_t[1]')
        if k.ReadProcessMemory(self,p,buf,n,got)==0 or tonumber(got[0])~=n then return nil end
        return ffi.string(buf,n)
    end
    function api.writable(p,n)
        local info=ffi.new('SickleLaserMemoryInfoV1[1]')
        local cursor=ffi.cast('uint8_t *',p)
        while n>0 do
            if k.VirtualQuery(cursor,ffi.cast('void *',info),ffi.sizeof(info[0]))~=ffi.sizeof(info[0]) then return false end
            if info[0].state~=0x1000 or info[0].kind~=0x20000 or info[0].protection~=4 then return false end
            local remaining=tonumber(info[0].size)-api.distance(cursor,info[0].base)
            if remaining<=0 then return false end
            local take=math.min(n,remaining); cursor=cursor+take; n=n-take
        end
        return true
    end
    function api.write(p,s)
        if not api.writable(p,#s) then return false end
        local count=ffi.new('size_t[1]')
        return k.WriteProcessMemory(self,p,s,#s,count)~=0 and tonumber(count[0])==#s
    end
    function api.data_access(p,n)
        local info=ffi.new('SickleLaserMemoryInfoV1[1]')
        local cursor=ffi.cast('uint8_t *',p)
        while n>0 do
            if k.VirtualQuery(cursor,ffi.cast('void *',info),ffi.sizeof(info[0]))~=ffi.sizeof(info[0]) then return false end
            if info[0].state~=0x1000 or info[0].kind~=0x20000 or (info[0].protection~=2 and info[0].protection~=4) then return false end
            local remaining=tonumber(info[0].size)-api.distance(cursor,info[0].base)
            if remaining<=0 then return false end
            local take=math.min(n,remaining);cursor=cursor+take;n=n-take
        end
        return true
    end
    function api.write_attachment(p,s)
        assert(#s==4 or #s==8 or #s==12 or #s==16 or #s==32,'Only guarded Sickle data fields may be written')
        local info=ffi.new('SickleLaserMemoryInfoV1[1]')
        if k.VirtualQuery(p,ffi.cast('void *',info),ffi.sizeof(info[0]))~=ffi.sizeof(info[0]) then return false end
        if not api.data_access(p,#s) or api.distance(p,info[0].base)+#s>tonumber(info[0].size) then return false end
        local old=tonumber(info[0].protection)
        if old==4 then return api.write(p,s) end
        if old~=2 then return false end
        local previous,ignored=ffi.new('uint32_t[1]'),ffi.new('uint32_t[1]')
        if k.VirtualProtect(p,#s,4,previous)==0 then return false end
        local ok,result=pcall(api.write,p,s)
        local restored=k.VirtualProtect(p,#s,previous[0],ignored)~=0
        assert(restored,'Attachment page protection restoration failed')
        assert(ok,result)
        return result
    end
    local function digest(feed)
        local algorithm,hash=ffi.new('void *[1]'),ffi.new('void *[1]')
        local ok,result=pcall(function()
            local name=ffi.new('uint16_t[7]',{83,72,65,50,53,54,0})
            assert(b.BCryptOpenAlgorithmProvider(algorithm,name,nil,0)==0,'SHA256 provider failed')
            assert(b.BCryptCreateHash(algorithm[0],hash,nil,0,nil,0,0)==0,'SHA256 initialization failed')
            feed(function(p,n) assert(b.BCryptHashData(hash[0],p,n,0)==0,'SHA256 input failed') end)
            local out=ffi.new('uint8_t[32]')
            assert(b.BCryptFinishHash(hash[0],out,32,0)==0,'SHA256 finalization failed')
            local hex={}; for i=0,31 do hex[#hex+1]=string.format('%02X',out[i]) end
            return table.concat(hex)
        end)
        if hash[0]~=nil then b.BCryptDestroyHash(hash[0]) end
        if algorithm[0]~=nil then b.BCryptCloseAlgorithmProvider(algorithm[0],0) end
        assert(ok,result); return result
    end
    function api.sha256(s) return digest(function(add) add(s,#s) end) end
    function api.module_hash(module)
        local path=ffi.new('uint16_t[32768]')
        local length=k.GetModuleFileNameW(module,path,32768)
        assert(length>0 and length<32768,'Module path unavailable')
        local file=k.CreateFileW(path,0x80000000,7,nil,3,0,nil)
        assert(file~=ffi.cast('void *',-1),'Module file unavailable')
        local ok,result=pcall(digest,function(add)
            local buf,n=ffi.new('uint8_t[65536]'),ffi.new('uint32_t[1]')
            while true do
                assert(k.ReadFile(file,buf,65536,n,nil)~=0,'Module read failed')
                if n[0]==0 then break end
                add(buf,n[0])
                if api.checkpoint then api.checkpoint('Checking game version') end
            end
        end)
        k.CloseHandle(file); assert(ok,result); return result
    end
    -- Retained by the public mod state for the entire process lifetime. No executable allocation.
    function api.owned_copy(source)
        assert(api.kept and (#source==76 or #source==272),'Owned-row lifetime unavailable')
        local block=ffi.new('uint64_t[?]',math.ceil(#source/8))
        ffi.copy(block,source,#source)
        local row=ffi.cast('uint8_t *',block)
        assert(api.writable(row,#source),'Owned data row permissions unsupported')
        api.kept[#api.kept+1]=block
        return row
    end
    function api.pointer_bytes(p)
        local value=ffi.new('uintptr_t[1]',ffi.cast('uintptr_t',p))
        return ffi.string(value,8)
    end
    -- Only the reviewed damage and projectile registry slots; RO/RX pages are refused.
    local registry_slots={
        [0x37c6240]=true,[0x37c6250]=true,[0x37c6258]=true,[0x37c6260]=true,
        [0x37c71b0]=true,[0x37c71b8]=true,[0x37c71c0]=true,
        [0x37c7b18]=true,[0x37c7c38]=true,[0x37c7d48]=true,[0x37c7738]=true
    }
    function api.registry_access(module,p)
        module=ffi.cast('uint8_t *',module)
        if not registry_slots[api.distance(p,module)] then return false end
        local info=ffi.new('SickleLaserMemoryInfoV1[1]')
        if k.VirtualQuery(p,ffi.cast('void *',info),ffi.sizeof(info[0]))~=ffi.sizeof(info[0]) then return false end
        return info[0].state==0x1000 and info[0].protection==4
          and (info[0].kind==0x20000 or info[0].kind==0x1000000)
          and api.distance(p,info[0].base)>=0 and api.distance(p,info[0].base)+8<=tonumber(info[0].size)
          and api.identity(p)%8==0
    end
    function api.publish(module,p,expected,replacement)
        if not api.registry_access(module,p) or api.read(p,8)~=api.pointer_bytes(expected) then return false end
        -- An aligned x64 pointer store publishes a completely initialized retained data row.
        ffi.cast('uintptr_t *',p)[0]=ffi.cast('uintptr_t',replacement)
        return api.read(p,8)==api.pointer_bytes(replacement)
    end

    return api
end

end)()
local profile=(function()
return {["components"]={{["label"]="spread_sickle",["group"]=26151656,["header"]="b1dbe4884c444c4401000000b1dbe488000f07000100000000000000",["count"]=730,["stride"]=1232,["index"]=294,["owner"]="a213c8b367f14586",["sha"]="59811171D7D16C67F2CB9A9870B86A60289E6F86A608466CA3B0A090DD73AF3E",["edits"]={{["offset"]=84,["bytes"]="0000000000000000"}}},{["label"]="spread_double_edge",["group"]=26151656,["header"]="b1dbe4884c444c4401000000b1dbe488000f07000100000000000000",["count"]=730,["stride"]=1232,["index"]=295,["owner"]="f18f4fdc26eb5b29",["sha"]="E480DDC6FCB3981F99F22EBDE8741B120A74585CE3BBF8DA8A43852D2E590727",["edits"]={{["offset"]=84,["bytes"]="0000000000000000"}}},{["label"]="fire_sickle",["group"]=46404840,["header"]="681b17454c444c4401000000681b174560b002000100000000000000",["count"]=542,["stride"]=616,["index"]=213,["owner"]="a213c8b367f14586",["sha"]="B9A380AFAB4E44DB37890FFF00D860790C34C47AEF46FE91360747624A2841D9",["edits"]={{["offset"]=8,["bytes"]="00002f44"}}},{["label"]="fire_double_edge",["group"]=46404840,["header"]="681b17454c444c4401000000681b174560b002000100000000000000",["count"]=542,["stride"]=616,["index"]=214,["owner"]="f18f4fdc26eb5b29",["sha"]="8291AEEAB391D1DB7EB827184818791402F4D6BEDE87B93CE9AB5C5782D38909",["edits"]={{["offset"]=136,["bytes"]="0000a0c0"}}},{["label"]="heat_sickle",["group"]=27493728,["header"]="d91c984c4c444c4401000000d91c984c004900000100000000000000",["count"]=58,["stride"]=592,["index"]=20,["owner"]="a213c8b367f14586",["sha"]="DFF153CFC79F8742A27485BE76FE13FEF40AE74C41CEF5A87BE8F7A0A3B6F50B",["edits"]={{["offset"]=116,["bytes"]="3333133f"}}},{["label"]="heat_double_edge",["group"]=27493728,["header"]="d91c984c4c444c4401000000d91c984c004900000100000000000000",["count"]=58,["stride"]=592,["index"]=21,["owner"]="f18f4fdc26eb5b29",["sha"]="45FDA9AA612DFCF0BEAC3296FE8F30D9CE2D3E6A2164E066107CF7750BE8E7A3",["edits"]={}}},["registries"]={{["label"]="damage_48",["id"]=48,["slot"]=58483264,["size"]=76,["sha"]="587834898D7DB0BF38A81EAD5C12E68017D2A8879FC2CC7E1A43466970CD6787",["offset"]=4,["bytes"]="460000000a000000030000000300000003000000",["original"]="300000003c00000006000000020000000200000002000000000000000a0000000a0000000a000000000000000000000000000000000000000000000000000000000000000000000000000000"},{["label"]="damage_50",["id"]=50,["slot"]=58483280,["size"]=76,["sha"]="8C7C4D21FA94FC63AB3DB196877C0D48663511B10A172A58D2CC7E4CDB82BE02",["offset"]=4,["bytes"]="460000000a000000",["original"]="320000003700000005000000030000000300000003000000000000000a0000000a0000000a000000000000000000000000000000000000000000000000000000000000000000000000000000"},{["label"]="damage_51",["id"]=51,["slot"]=58483288,["size"]=76,["sha"]="91391F156D1DA5838EFB717E1866B0E95D0DE55E2320E4C0C39E429367ED40EF",["offset"]=4,["bytes"]="580000000e000000",["original"]="330000004600000007000000030000000300000003000000000000000a0000000a0000000a000000000000000000000000000000000000000000000000000000000000000000000000000000"},{["label"]="damage_52",["id"]=52,["slot"]=58483296,["size"]=76,["sha"]="D87FB882BDC8BD77FC631CDEAF8D1366329ACD66C2A638AF0975A409F98FEB9E",["offset"]=4,["bytes"]="580000000e000000",["original"]="340000004600000007000000040000000400000004000000000000000a0000000a0000000a000000000000000000000000000000000000000000000000000000000000000000000000000000"},{["label"]="damage_542",["id"]=542,["slot"]=58487216,["size"]=76,["sha"]="145090747DF2F8C5C7C99F6CF5CFAB90D8F5935C05B44360631D7CA9B2885564",["offset"]=4,["bytes"]="0f0000000f000000",["original"]="1e020000140000001400000009000000090000000900000009000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000000000000"},{["label"]="damage_543",["id"]=543,["slot"]=58487224,["size"]=76,["sha"]="7EC01C0AC5F4115AD671FB47CE0BF1730E734938C71C80110E5E1B70A6CAAF72",["offset"]=4,["bytes"]="1e0000001e000000",["original"]="1f020000280000002800000009000000090000000900000009000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000000000000"},{["label"]="damage_544",["id"]=544,["slot"]=58487232,["size"]=76,["sha"]="916F8BF1E06A0CBEA12990C2A86429AA41F2C4B2053009D3FD787563C9F1DE68",["offset"]=4,["bytes"]="4b0000004b000000",["original"]="20020000640000006400000009000000090000000900000009000000000000000000000000000000010000000500000000002041000000000000000000000000000000000000000000000000"},{["label"]="projectile_149",["id"]=149,["slot"]=58489624,["size"]=272,["sha"]="551D3DF069DA84725E0FD7D572390E08B4A64F16D744B953083DBF0A52E12273",["offset"]=32,["bytes"]="0000fa44",["original"]="9500000080147b993e84daef6952cdf1f5c84b7d5898656a00002041010000000080a244000040410000000000000000010000000000c04000000000300000000000803e00000000d4b22a7804127efe0000000000000000cdcc4c3d000000000000000000000000000000000000000092351090ffc124a4000000000000000000000000000000000000a042010000000000000000000000000000000000000000000000cdcccc3d0a0000000a0000000000000000000000000070420000403f000040400000704100000000000000000000000000000000000080bf00000000000000000000000001000000000000007d0000000200000018953d390000000000000000000000000000000000000000"},{["label"]="projectile_185",["id"]=185,["slot"]=58489912,["size"]=272,["sha"]="A6C1BCAA4C954CBD47D7E9C508437DDB84AF814488BCF20E24EF043512894FA4",["offset"]=32,["bytes"]="0000fa44",["original"]="b900000080147b993e84daef6952cdf1f5c84b7d5898656a00002041010000000080a244000040410000000000000000010000000000c04000000000320000000000803e00000000fb966aa327993eaa0000000000000000cdcc4c3d00000000000000000000000000000000000000000e0b9624583266f6000000000000000000000000000000000000a042010000000000000000000000000000000000000000000000cdcccc3d0a0000000a0000000000000000000000000070420000403f000040400000704100000000000000000000000000000000000080bf00000000000000000000000001000000000000007d0000000200000018953d390000000000000000000000000000000000000000"},{["label"]="projectile_219",["id"]=219,["slot"]=58490184,["size"]=272,["sha"]="74D8A556B392E6C631B55AEC231C02D646DFA0A594764FFCCA06574B484C3308",["offset"]=32,["bytes"]="0000fa44",["original"]="db00000080147b993e84daef6952cdf1f5c84b7d5898656a00002041010000000080a244000040410000000000000000010000000000c04000000000330000000000803e00000000fb966aa327993eaa0000000000000000cdcc4c3d00000000000000000000000000000000000000000e0b9624583266f6000000000000000000000000000000000000a042010000000000000000000000000000000000000000000000cdcccc3d0a0000000a0000000000000000000000000070420000403f000040400000704100000000000000000000000000000000000080bf00000000000000000000000001000000000000007d0000000200000018953d390000000000000000000000000000000000000000"},{["label"]="projectile_25",["id"]=25,["slot"]=58488632,["size"]=272,["sha"]="F4C66C413239F7248E49196EBAA45FE8C97A68B97FC0D4A499137378889A0005",["offset"]=32,["bytes"]="0000fa44",["original"]="1900000080147b993e84daef6952cdf1f5c84b7d5898656a00002041010000000080a244000040410000000000000000010000000000c04000000000340000000000803e00000000fb966aa327993eaa0000000000000000cdcc4c3d00000000000000000000000000000000000000000e0b9624583266f6000000000000000000000000000000000000a042010000000000000000000000000000000000000000000000cdcccc3d0a0000000a0000000000000000000000000070420000403f000040400000704100000000000000000000000000000000000080bf00000000000000000000000001000000000000007d0000000200000018953d390000000000000000000000000000000000000000"}},["pins"]={{["rva"]=19538941,["bytes"]="488d0dbc3c5202"},{["rva"]=18845358,["bytes"]="488d0d0bd25c02"},{["rva"]=19538948,["bytes"]="488b0cc1"},{["rva"]=19539065,["bytes"]="660f6e5904"},{["rva"]=19539070,["bytes"]="660f6e4908"},{["rva"]=20618389,["bytes"]="f30f107020"},{["rva"]=20619459,["bytes"]="f3410f11b4376c300000"},{["rva"]=6364120,["bytes"]="e82335f0ff"},{["rva"]=6364133,["bytes"]="4c8be8"},{["rva"]=6365228,["bytes"]="418b8580000000"},{["rva"]=6365235,["bytes"]="894554"},{["rva"]=6365238,["bytes"]="418b8584000000"},{["rva"]=6365245,["bytes"]="894558"},{["rva"]=6365248,["bytes"]="418b8588000000"},{["rva"]=6365255,["bytes"]="89455c"},{["rva"]=6365258,["bytes"]="418b858c000000"},{["rva"]=6365265,["bytes"]="894560"},{["rva"]=20621641,["bytes"]="8b425c"},{["rva"]=20621644,["bytes"]="89472c"},{["rva"]=20621653,["bytes"]="8b4260"},{["rva"]=20621656,["bytes"]="894730"},{["rva"]=19539673,["bytes"]="f30f118b98000000"}}}

end)()
local patch=(function()
local ffi=require('ffi')
local patch={}
local function unhex(s)return(s:gsub('..',function(h)return string.char(tonumber(h,16))end))end
local function u32(s,o)
    if not s or #s<o+4 then return nil end
    local a,b,c,d=s:byte(o+1,o+4);return a+b*256+c*65536+d*16777216
end
local function replace(s,o,b)return s:sub(1,o)..b..s:sub(o+#b+1)end
local function fail(s)error(s..'. No edit applied.',0)end
local function discover(api,game)
    local owner=api.pointer(api.read(game+0x346bf98,8),0)
    local map=owner and api.pointer(api.read(owner+0xf12bd8,8),0)
    if not map then fail('Expected one supported Sickle entity table; found 0')end
    local entities=map-26151684
    local plans={}
    for _,spec in ipairs(profile.components)do
        local group=entities+spec.group
        local owners=api.read(group+28,spec.count*16)
        assert(api.read(group,28)==unhex(spec.header) and owners,'Sickle '..spec.label..' group mismatch')
        local hits,aliases=0,0
        for i=0,spec.count-1 do
            local entry=owners:sub(i*16+1,i*16+16)
            if entry:sub(1,8)==unhex(spec.owner)then
                assert(u32(entry,8)==spec.index and u32(entry,12)==0,'Sickle owner index changed');hits=hits+1
            end
            if entry:sub(1,8)~=string.rep('\0',8) and u32(entry,8)==spec.index then aliases=aliases+1 end
        end
        assert(hits==1 and aliases==1,'Sickle component owner is not unique')
        local row=group+28+spec.count*16+spec.index*spec.stride
        local source=api.read(row,spec.stride)
        assert(api.data_access(row,spec.stride) and source and api.sha256(source)==spec.sha,
            'Sickle '..spec.label..' row mismatch or unsupported permissions')
        local function proof()
            local current_owner=api.pointer(api.read(game+0x346bf98,8),0)
            local current_map=current_owner and api.pointer(api.read(current_owner+0xf12bd8,8),0)
            return current_owner and current_map and api.distance(current_owner,owner)==0 and api.distance(current_map,map)==0
              and api.read(group,28)==unhex(spec.header) and api.read(group+28,spec.count*16)==owners
        end
        plans[#plans+1]={row=row,size=spec.stride,source=source,edits=spec.edits,proof=proof,label=spec.label}
    end
    return plans
end
function patch.apply(api,module)
    local ok,result=pcall(function()
        local game=ffi.cast('uint8_t *',module)
        for _,pin in ipairs(profile.pins)do
            assert(api.read(game+pin.rva,#pin.bytes/2)==unhex(pin.bytes),'Unsupported Sickle native data consumer')
        end
        local plans=discover(api,game)
        local registries={}
        for _,spec in ipairs(profile.registries)do
            local slot=game+spec.slot
            local row=api.pointer(api.read(slot,8),0)
            if not row then fail('Expected one supported Sickle '..spec.label..' row; found 0')end
            local source=api.read(row,spec.size)
            assert(source and api.sha256(source)==spec.sha and u32(source,0)==spec.id,'Sickle '..spec.label..' source mismatch')
            assert(api.registry_access(game,slot),'Sickle '..spec.label..' registry is not writable data')
            local after=replace(source,spec.offset,unhex(spec.bytes))
            local owned=api.owned_copy(after)
            assert(api.read(owned,spec.size)==after,'Sickle owned row initialization failed')
            registries[#registries+1]={slot=slot,row=row,source=source,size=spec.size,owned=owned,after=after}
        end
        local function sources_intact()
            for _,p in ipairs(plans)do
                if api.read(p.row,p.size)~=p.source or not p.proof()then return false end
            end
            for _,p in ipairs(registries)do
                if api.read(p.slot,8)~=api.pointer_bytes(p.row) or api.read(p.row,p.size)~=p.source then return false end
            end
            return true
        end
        assert(sources_intact(),'Data changed before edit. No edit applied.')
        local attempted,published={},{}
        local applied,reason=pcall(function()
            for _,p in ipairs(plans)do
                local expected=p.source
                assert(p.proof() and api.read(p.row,p.size)==expected,'Sickle ownership changed during edit')
                for _,edit in ipairs(p.edits)do
                    local bytes=unhex(edit.bytes)
                    attempted[#attempted+1]={row=p.row,size=p.size,source=expected,offset=edit.offset,bytes=bytes,proof=p.proof}
                    assert(api.write_attachment(p.row+edit.offset,bytes),'Sickle data write incomplete')
                    expected=replace(expected,edit.offset,bytes)
                    assert(api.read(p.row,p.size)==expected,'Sickle component verification failed')
                end
                p.expected=expected
            end
            for _,p in ipairs(registries)do
                assert(api.read(p.row,p.size)==p.source and api.read(p.owned,p.size)==p.after,'Sickle registry source changed')
                published[#published+1]=p
                assert(api.publish(game,p.slot,p.row,p.owned),'Sickle registry publication failed')
            end
            for _,p in ipairs(plans)do assert(p.proof() and api.read(p.row,p.size)==p.expected,'Full table verification failed')end
            for _,p in ipairs(registries)do
                assert(api.read(p.slot,8)==api.pointer_bytes(p.owned) and api.read(p.owned,p.size)==p.after
                  and api.read(p.row,p.size)==p.source,'Full registry verification failed')
            end
        end)
        if not applied then
            local restored=true
            for i=#published,1,-1 do
                local p=published[i]
                local current=api.read(p.slot,8)
                if current==api.pointer_bytes(p.owned)then
                    restored=api.publish(game,p.slot,p.owned,p.row)and restored
                elseif current~=api.pointer_bytes(p.row)then restored=false end
            end
            for i=#attempted,1,-1 do
                local p=attempted[i]
                local before=p.source:sub(p.offset+1,p.offset+#p.bytes)
                local current=api.read(p.row,p.size)
                local owned=p.proof() and current and replace(current,p.offset,before)==p.source
                if owned then
                    for j=1,#p.bytes do
                        local b=current:byte(p.offset+j)
                        if b~=before:byte(j)and b~=p.bytes:byte(j)then owned=false;break end
                    end
                end
                if owned then restored=api.write_attachment(p.row+p.offset,before)and api.read(p.row,p.size)==p.source and restored
                else restored=false end
            end
            error(tostring(reason)..'; restored='..tostring(restored),0)
        end
        return 'Applied: LAS-16 Sickle 70/10 damage, heat per shot 1.15 -> 0.575; Double-Edge cold/warm/hot/maximum heat damage 65/10,65/10,83/14,83/14, direct heat self-damage -25%, heat buildup/cooling/ignition unchanged; base 700 RPM, all projectile tiers 2000 m/s, zero spread and no damage fall-off; minimum AP3 on both weapons, Double-Edge maximum-heat AP4 retained.'
    end)
    return ok,tostring(result)
end
return patch

end)()
local start=(function()
return function(make_api,patch)
    if rawget(_G,'SickleLaserARV1') then return end
    local state={kept={},version='0.1.2',active=false,status='Waiting for first game update',scanned=0,candidates=0,diagnostics={}}
    _G.SickleLaserARV1=state
    local previous=update
    local callback
    local function log()
        pcall(function()
            local logger=rawget(_G,'CowboyBingusModLoader')
            local file=logger and logger.open_log and logger.open_log('SickleLaserAR.log')
            if file then
                file:write('SickleLaserAR v0.1.2 MINIMUM AP3; LAS-16 70/10; DOUBLE-EDGE 65/10,65/10,83/14,83/14; ALL 2000 M/S; HEAT SELF-DAMAGE -25%\nstatus='..state.status..'\nscanned_bytes='..state.scanned..'\ncandidates='..state.candidates..'\n'..table.concat(state.diagnostics,'\n')..'\n')
                file:close()
            end
        end)
    end
    local deadline=0
    local logged_mb=-1
    local worker=coroutine.create(function()
            local api=make_api()
            api.kept=state.kept
            api.note=function(message)
                if #state.diagnostics>=12 then table.remove(state.diagnostics,1) end
                state.diagnostics[#state.diagnostics+1]='validation='..message
            end
            api.checkpoint=function(stage,force)
                state.status=stage
                if force or os.clock()>=deadline then coroutine.yield() end
            end
            api.progress=function(scanned,total,candidates)
                state.scanned,state.candidates=scanned,candidates
                local mb=math.floor(scanned/16777216)
                if mb~=logged_mb then logged_mb=mb;log() end
            end
            local exe,game=api.module(nil),api.module('game.dll')
            assert(exe~=nil and game~=nil,'Game modules unavailable')
            assert(api.module_hash(exe)=='F5FEE03DCFDB2E553A4752C283590950AC13316B376D8196AA556FF0400D5F06','Unsupported executable. No edit applied.')
            assert(api.module_hash(game)=='2E2C3B7C2500646DADD5F2B4C6E0504DBB7E7896139F64CDDC0D1813C718F51E','Unsupported game module. No edit applied.')
            local function wait_for_loading(milliseconds,attempt)
                if not api.now then return end
                state.status='Waiting for game data to load; attempt '..attempt..'/8'
                log()
                local until_time=api.now()+milliseconds
                repeat coroutine.yield() until api.now()>=until_time
            end
            for attempt=1,8 do
                if attempt==1 then wait_for_loading(15000,attempt) end
                local applied,reason=patch.apply(api,game)
                if applied then return reason end
                local not_ready=reason:find('found 0. No edit applied.',1,true)~=nil
                local stale=reason:find('Data changed before edit. No edit applied.',1,true)~=nil
                local restored_conflict=reason:find('Full table verification failed',1,true)~=nil
                    and reason:find('restored=true',1,true)~=nil
                if not not_ready and not stale and not restored_conflict or attempt==8 then error(reason) end
                local retry_conflict=stale or restored_conflict
                state.status=retry_conflict
                    and 'Shared table changed during Sickle setup; retrying safely'
                    or 'Damage table not ready; retrying after loading'
                log()
                wait_for_loading(retry_conflict and 3000 or 10000,attempt)
            end
    end)
    local function init()
        if coroutine.status(worker)=='dead' then return end
        deadline=os.clock()+0.0007
        local ok,message=coroutine.resume(worker)
        if not ok or coroutine.status(worker)=='dead' then
            state.active,state.status=ok,tostring(message)
            print('[SickleLaserAR] '..state.status)
            log()
            if update==callback then update=previous or function() end end
        end
    end
    local function finish(...) init();return ... end
    callback=function(...)
        if previous then return finish(previous(...)) end
        init()
    end
    update=callback
    log()
end

end)()
start(make_api,patch)
