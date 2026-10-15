-- HD2-Addon: mods/codex/tenderizer_armor
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
        } TenderizerMemoryInfoV1;
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
        local info=ffi.new('TenderizerMemoryInfoV1[1]')
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
        local info=ffi.new('TenderizerMemoryInfoV1[1]')
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
        assert(#s==4 or #s==8 or #s==12 or #s==16 or #s==32,'Only guarded Tenderizer data fields may be written')
        local info=ffi.new('TenderizerMemoryInfoV1[1]')
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
    local registry_slots={[0x37c64f8]=true,[0x37c77d0]=true
    }
    function api.registry_access(module,p)
        module=ffi.cast('uint8_t *',module)
        if not registry_slots[api.distance(p,module)] then return false end
        local info=ffi.new('TenderizerMemoryInfoV1[1]')
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
return {["components"]={{["label"]="magazine",["group"]=528664,["header"]="a3888dfb4c444c4401000000a3888dfb20cb00000100000000000000",["count"]=540,["stride"]=160,["record_count"]=271,["index"]=210,["owner"]="12a8953da33a06ce",["sha"]="C5737C68E2DCBFDBBC1EC37BF77EEF9769C9D642967374AE478D42F0BC8F2C47",["edits"]={{["offset"]=136,["bytes"]="46000000"}}},{["label"]="fire",["group"]=46404840,["header"]="681b17454c444c4401000000681b174560b002000100000000000000",["count"]=542,["stride"]=616,["record_count"]=272,["index"]=201,["owner"]="12a8953da33a06ce",["sha"]="F35E6A6BBCEAC5595C5D9AD1349604837777106AF6353AE3935563EDD748681A",["edits"]={}},{["label"]="sway",["group"]=26151656,["header"]="b1dbe4884c444c4401000000b1dbe488000f07000100000000000000",["count"]=730,["stride"]=1232,["record_count"]=366,["index"]=279,["owner"]="12a8953da33a06ce",["sha"]="6FE8A8703A00BB9BEA0E6DB42F79BADA6EF64E3257E076D9781772224CE9FBC4",["edits"]={{["offset"]=104,["bytes"]="6666663f"}}}},["registries"]={{["label"]="damage",["id"]=135,["slot"]=58483960,["size"]=76,["sha"]="960A2DC29CAC151AD632FE13ADB2292980E233CB7DF988F5D076D5BC4D311AA5",["offset"]=4,["bytes"]="5a0000001e000000030000000300000002000000000000000a0000000f0000000a00000000000000370000000000803f",["original"]="87000000690000001e000000020000000200000002000000000000000a0000000f0000000a000000000000000000000000000000000000000000000000000000000000000000000000000000"},{["label"]="projectile",["id"]=44,["slot"]=58488784,["size"]=272,["sha"]="A43D78C901F97C3F92949F5AA06EA54F815EAB1089FDDE5A924BAE9E9A24D77B",["offset"]=32,["bytes"]="00007a44",["original"]="2c000000cc2b165228c155196952cdf1f5c84b7d5898656a000000410100000000004d44000030419a99993e0000803f010000000000000000000000870000000000803e000000009c7b34f4ac78e462cbd59ebe71f73f3f0ad7233c00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000a042010000000000000000000000000000000000000000000000cdcccc3d0300000003000000010000000000a642000070420000403f000000400000404150af124d8790e63a0000000000000000000080bf00000000000000000000000001000000000000007d0000000000000018953d390000000000000000000000000000000000000000"}},["pins"]={{["rva"]=19538941,["bytes"]="488d0dbc3c5202"},{["rva"]=19538948,["bytes"]="488b0cc1"},{["rva"]=19539065,["bytes"]="660f6e5904"},{["rva"]=19539070,["bytes"]="660f6e4908"},{["rva"]=20618389,["bytes"]="f30f107020"},{["rva"]=20619459,["bytes"]="f3410f11b4376c300000"},{["rva"]=18813392,["bytes"]="4863c1"},{["rva"]=18813395,["bytes"]="488d0d764a5d02"},{["rva"]=18813402,["bytes"]="488b04c1"},{["rva"]=6922270,["bytes"]="488bb4c1505c7c03"},{["rva"]=6922568,["bytes"]="f30f106628"}},["status_slot"]=58482184,["acid_signature"]="00000000000000009db958bdd7e7bc960000c03f0000803f",["gas_applied"]="Applied: TX-41 Sterilizer gas 25 -> 45 DPS; Helldiver incoming Gas multiplier 1.3 -> 0.7333; magazine 125 -> 175; Gas MKII / confusion buildup 0.5 -> 0.75; Gas Overhaul: Acid Storm armor reduction added to player gas hit records; duration 1 -> 15 seconds."}

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
    if not map then fail('Expected one supported Tenderizer entity table; found 0')end
    local entities=map-26151684
    local plans={}
    for _,spec in ipairs(profile.components)do
        local group=entities+spec.group
        local owners=api.read(group+28,spec.count*16)
        assert(api.read(group,28)==unhex(spec.header) and owners,'Tenderizer '..spec.label..' group mismatch')
        local hits,aliases=0,0
        for i=0,spec.count-1 do
            local entry=owners:sub(i*16+1,i*16+16)
            if entry:sub(1,8)==unhex(spec.owner)then
                assert(u32(entry,8)==spec.index and u32(entry,12)==0,'Tenderizer owner index changed');hits=hits+1
            end
            if entry:sub(1,8)~=string.rep('\0',8) and u32(entry,8)==spec.index then aliases=aliases+1 end
        end
        assert(hits==1 and aliases==1,'Tenderizer component owner is not unique')
        local row=group+28+spec.count*16+spec.index*spec.stride
        local source=api.read(row,spec.stride)
        assert(api.data_access(row,spec.stride) and source and api.sha256(source)==spec.sha,
            'Tenderizer '..spec.label..' row mismatch or unsupported permissions')
        local function proof()
            local current_owner=api.pointer(api.read(game+0x346bf98,8),0)
            local current_map=current_owner and api.pointer(api.read(current_owner+0xf12bd8,8),0)
            return current_owner and current_map and api.distance(current_owner,owner)==0 and api.distance(current_map,map)==0
              and api.read(group,28)==unhex(spec.header) and api.read(group+28,spec.count*16)==owners
        end
        plans[#plans+1]={row=row,size=spec.stride,source=source,edits=spec.edits,proof=proof,label=spec.label}
    end
    local slot=game+profile.status_slot
    local row=api.pointer(api.read(slot,8),0)
    if not row then fail('Expected one supported Acid Storm row; found 0')end
    local s=api.read(row,152)
    assert(api.data_access(row,152) and s and #s==152 and u32(s,0)==55,'Acid Storm ownership or permissions mismatch')
    assert(s:sub(17,40)==unhex(profile.acid_signature),'Acid Storm status fields mismatch')
    assert(u32(s,44)==0 and s:byte(49)==0 and s:sub(57,97)==string.rep('\0',41),'Acid Storm status payload mismatch')
    local seconds=s:sub(41,44)
    assert(seconds==unhex('0000803f')or seconds==unhex('00007041'),'Unknown Acid Storm duration')
    if api.gas_required then assert(seconds==unhex('00007041'),'Gas Overhaul reported Applied without its 15-second duration')end
    local edits=seconds==unhex('0000803f')and{{offset=40,bytes='00007041'}}or{}
    plans[#plans+1]={row=row,size=152,source=s,edits=edits,label='acid_duration',proof=function()return api.read(slot,8)==api.pointer_bytes(row)end}
    return plans
end
function patch.apply(api,module)
    local ok,result=pcall(function()
        local game=ffi.cast('uint8_t *',module)
        for _,pin in ipairs(profile.pins)do
            assert(api.read(game+pin.rva,#pin.bytes/2)==unhex(pin.bytes),'Unsupported Tenderizer native data consumer')
        end
        local plans=discover(api,game)
        local registries={}
        for _,spec in ipairs(profile.registries)do
            local slot=game+spec.slot
            local row=api.pointer(api.read(slot,8),0)
            if not row then fail('Expected one supported Tenderizer '..spec.label..' row; found 0')end
            local source=api.read(row,spec.size)
            assert(source and api.sha256(source)==spec.sha and u32(source,0)==spec.id,'Tenderizer '..spec.label..' source mismatch')
            assert(api.registry_access(game,slot),'Tenderizer '..spec.label..' registry is not writable data')
            local after=replace(source,spec.offset,unhex(spec.bytes))
            local owned=api.owned_copy(after)
            assert(api.read(owned,spec.size)==after,'Tenderizer owned row initialization failed')
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
                assert(p.proof() and api.read(p.row,p.size)==expected,'Tenderizer ownership changed during edit')
                for _,edit in ipairs(p.edits)do
                    local bytes=unhex(edit.bytes)
                    attempted[#attempted+1]={row=p.row,size=p.size,source=expected,offset=edit.offset,bytes=bytes,proof=p.proof}
                    assert(api.write_attachment(p.row+edit.offset,bytes),'Tenderizer data write incomplete')
                    expected=replace(expected,edit.offset,bytes)
                    assert(api.read(p.row,p.size)==expected,'Tenderizer component verification failed')
                end
                p.expected=expected
            end
            for _,p in ipairs(registries)do
                assert(api.read(p.row,p.size)==p.source and api.read(p.owned,p.size)==p.after,'Tenderizer registry source changed')
                published[#published+1]=p
                assert(api.publish(game,p.slot,p.row,p.owned),'Tenderizer registry publication failed')
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
        return "Applied: AR-61 Tenderizer 70-round magazine, 1000 m/s projectile speed, direct/slight AP3 (other angles 2/0), Acid Storm on bullet hits with strength 1 and shared duration 15 seconds; base sway 0.9; 90/30 damage, 600/850 RPM, recoil, spread, spare magazines and drag unchanged."
    end)
    return ok,tostring(result)
end
return patch

end)()
local start=(function()
return function(make_api,patch)
    if rawget(_G,'TenderizerArmorV1') then return end
    local state={kept={},version='0.1.2',active=false,status='Waiting for first game update',scanned=0,candidates=0,diagnostics={}}
    _G.TenderizerArmorV1=state
    local previous=update
    local callback
    local function log()
        pcall(function()
            local logger=rawget(_G,'CowboyBingusModLoader')
            local file=logger and logger.open_log and logger.open_log('TenderizerArmor.log')
            if file then
                file:write('TenderizerArmor v0.1.2 70 ROUNDS / 1000 M/S / DIRECT-SLIGHT AP3 / ACID STORM 15S / SWAY 0.9\nstatus='..state.status..'\nscanned_bytes='..state.scanned..'\ncandidates='..state.candidates..'\n'..table.concat(state.diagnostics,'\n')..'\n')
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
            local function wait_for_gas()
                local loader=rawget(_G,'CowboyBingusModLoader')
                local module_status=loader and loader.modules and loader.modules['mods/codex/sterilizer_armor_control']
                local peer=rawget(_G,'SterilizerArmorControlV1')
                local detected=peer~=nil or(module_status~=nil and module_status~='not installed')
                api.gas_required=detected
                if not detected then return end
                local started=assert(api.now,'Clock unavailable for Gas Overhaul readiness')()
                repeat
                    peer=rawget(_G,'SterilizerArmorControlV1')
                    if peer and type(peer.status)=='string'and peer.status:sub(1,8)=='Applied:'then
                        assert(peer.status==profile.gas_applied,'Unsupported Gas Overhaul Applied state. No edit applied.')
                        return
                    end
                    assert(api.now()-started<120000,'Installed companion did not become ready within 120 seconds: Gas Overhaul; no Tenderizer edits applied')
                    state.status='Waiting for installed Gas Overhaul to report Applied';log();coroutine.yield()
                until false
            end
            for attempt=1,8 do
                if attempt==1 then wait_for_loading(15000,attempt);wait_for_gas() end
                local applied,reason=patch.apply(api,game)
                if applied then return reason end
                local not_ready=reason:find('found 0. No edit applied.',1,true)~=nil
                local stale=reason:find('Data changed before edit. No edit applied.',1,true)~=nil
                local restored_conflict=reason:find('Full table verification failed',1,true)~=nil
                    and reason:find('restored=true',1,true)~=nil
                if not not_ready and not stale and not restored_conflict or attempt==8 then error(reason) end
                local retry_conflict=stale or restored_conflict
                state.status=retry_conflict
                    and 'Shared table changed during Tenderizer setup; retrying safely'
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
            print('[TenderizerArmor] '..state.status)
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
