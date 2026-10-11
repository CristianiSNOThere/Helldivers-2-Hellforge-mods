-- HD2-Addon: mods/crife/ar32_pacifier_lua
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
        } SaiFocusMemoryInfoV1;
    ]]
    local k, b = ffi.load('kernel32'), ffi.load('bcrypt')
    local self = k.GetCurrentProcess()
    local api = {}
    -- Canonical user addresses are below 2^47 and exactly representable as doubles.
    -- The game may hide every cdata value behind the same tostring result.
    function api.identity(p) return tonumber(ffi.cast('uintptr_t',p)) end
    function api.now() return tonumber(k.GetTickCount64()) end
    function api.module(name) return ffi.cast('uint8_t *',k.GetModuleHandleA(name)) end
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
    function api.record_candidates(module)
        local function u32(s,o)
            if not s or #s < o+4 then return nil end
            local a,b,c,d=s:byte(o+1,o+4)
            return a+b*256+c*65536+d*16777216
        end
        local base=ffi.cast('uint8_t *',module)
        local owner=assert(api.pointer(api.read(base+0x346bf98,8),0),'Entity owner pointer unavailable')
        local map=assert(api.pointer(api.read(owner+0xf12bd8,8),0),'Entity source table unavailable')
        local entities=map-26151684
        local magazine,health=entities+576052,entities+9520532
        local status={}
        local seen={}
        for _,region in ipairs(api.private_regions(true)) do
            local head=api.read(region.base,56)
            if head then
                for offset=0,12,4 do
                    if head:sub(offset+1,offset+4)=='LDLD' and u32(head,offset+4)==1
                      and u32(head,offset+8)==0xc63e0b22 and u32(head,offset+12)==11888 then
                        local address=region.base+offset+8248
                        if u32(api.read(address,4),0)==55 and not seen[api.identity(address)] then
                            seen[api.identity(address)]=true;status[#status+1]=address
                        end
                    end
                end
            end
            if api.checkpoint then api.checkpoint('Resolving status allocation header') end
        end
        return {{magazine},status,{health}}
    end
    function api.writable(p,n)
        local info=ffi.new('SaiFocusMemoryInfoV1[1]')
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
        local info=ffi.new('SaiFocusMemoryInfoV1[1]')
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
    function api.write_protected(p,s)
        assert(#s==1 or #s==4 or #s==8,'Only validated 1-, 4-, or 8-byte data edits may be written')
        local info=ffi.new('SaiFocusMemoryInfoV1[1]')
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
    local function u16(s,o) local a,c=s:byte(o+1,o+2);return a+c*256 end
    local function u32(s,o) local a,c,d,e=s:byte(o+1,o+4);return a+c*256+d*65536+e*16777216 end
    function api.private_regions(include_readonly)
        local regions={}
        local allocations={}
        local info=ffi.new('SaiFocusMemoryInfoV1[1]')
        local cursor=ffi.cast('uint8_t *',65536)
        while ffi.cast('uintptr_t',cursor)<0x800000000000 do
            if k.VirtualQuery(cursor,ffi.cast('void *',info),ffi.sizeof(info[0]))~=ffi.sizeof(info[0]) then break end
            local size=tonumber(info[0].size)
            local next_address=ffi.cast('uint8_t *',info[0].base)+size
            assert(size>0 and next_address>cursor,'Invalid memory-region layout')
            if info[0].state==0x1000 and info[0].kind==0x20000 and (info[0].protection==4 or (include_readonly and info[0].protection==2)) then
                local allocation=ffi.cast('uint8_t *',info[0].allocation)
                local identity=api.identity(allocation)
                if allocations[identity]==nil then
                    local head=identity>=0x80000000 and api.read(allocation,32) or nil
                    local eligible=false
                    if head then
                        for offset=0,12,4 do
                            if head:sub(offset+1,offset+4)=='LDLD' and u32(head,offset+4)==1 then eligible=true;break end
                        end
                    end
                    allocations[identity]=eligible
                end
                if allocations[identity] then
                    regions[#regions+1]={base=ffi.cast('uint8_t *',info[0].base),size=size}
                end
            end
            cursor=next_address
            if api.checkpoint then api.checkpoint('Enumerating writable private data') end
        end
        return regions
    end
    function api.scan_private(signature,row_offset,include_readonly)
        assert(type(signature)=='string' and #signature>=8 and #signature<=256,'Invalid data signature')
        local regions=api.private_regions(include_readonly)
        local total,scanned,matches=0,0,0
        local result,seen={},{}
        for _,region in ipairs(regions) do total=total+region.size end
        assert(total<=8589934592,'Private-data scan exceeds 8 GiB bounds. No edit applied.')
        for _,region in ipairs(regions) do
            local tail=''
            for offset=0,region.size-1,65536 do
                local length=math.min(65536,region.size-offset)
                local data=api.read(region.base+offset,length)
                if data then
                    local block=tail..data
                    local from=1
                    while true do
                        local at=block:find(signature,from,true)
                        if not at then break end
                        matches=matches+1
                        local address=region.base+offset-#tail+at-1-row_offset
                        local key=api.identity(address)
                        if not seen[key] then seen[key]=true;result[#result+1]=address end
                        from=at+1
                    end
                    tail=block:sub(-(#signature-1))
                else tail='' end
                scanned=scanned+length
                if api.progress then api.progress(scanned,total,matches) end
                if api.checkpoint then api.checkpoint('Searching private data for exact flag record') end
            end
        end
        return result
    end
    function api.scan_private_many(patterns,include_readonly)
        assert(type(patterns)=='table' and #patterns>0 and #patterns<=8,'Invalid multi-signature scan')
        local regions=api.private_regions(include_readonly)
        local total,scanned=0,0
        local results,seen={},{}
        local max_signature=0
        for i,pattern in ipairs(patterns) do
            assert(type(pattern.signature)=='string' and #pattern.signature>=8 and #pattern.signature<=256,'Invalid data signature')
            assert(type(pattern.row_offset)=='number' and pattern.row_offset>=0,'Invalid record offset')
            results[i],seen[i]={},{}
            max_signature=math.max(max_signature,#pattern.signature)
        end
        for _,region in ipairs(regions) do total=total+region.size end
        assert(total<=8589934592,'Private-data scan exceeds 8 GiB bounds. No edit applied.')
        for _,region in ipairs(regions) do
            local tail=''
            for offset=0,region.size-1,65536 do
                local length=math.min(65536,region.size-offset)
                local data=api.read(region.base+offset,length)
                if data then
                    local block=tail..data
                    for i,pattern in ipairs(patterns) do
                        local from=1
                        while true do
                            local at=block:find(pattern.signature,from,true)
                            if not at then break end
                            local address=region.base+offset-#tail+at-1-pattern.row_offset
                            local key=api.identity(address)
                            if not seen[i][key] then
                                seen[i][key]=true
                                results[i][#results[i]+1]=address
                            end
                            from=at+1
                        end
                    end
                    tail=block:sub(-(max_signature-1))
                else tail='' end
                scanned=scanned+length
                local matches=0
                for i=1,#results do matches=matches+#results[i] end
                if api.progress then api.progress(scanned,total,matches) end
                if api.checkpoint then api.checkpoint('Searching private data for Sterilizer records') end
            end
        end
        return results
    end
    function api.candidates(module,signature,row_offset,header)
        if not header then
            local slot=ffi.cast('uint8_t *',module)+0x348e1f8
            local target=api.pointer(api.read(slot,8),0)
            local head=target and api.read(target,16)
            if head and u32(head,0)==2 and head:sub(5,8)=='LDLD' and u32(head,12)==3943969754 then
                return {target}
            end
        end
        local base=ffi.cast('uint8_t *',module)
        local dos=assert(api.read(base,64),'Cannot read module header')
        assert(dos:sub(1,2)=='MZ','Unexpected module header')
        local ntOffset=u32(dos,60); assert(ntOffset<1048576,'Invalid PE offset')
        local nt=assert(api.read(base+ntOffset,264),'Cannot read PE header')
        assert(nt:sub(1,4)=='PE\0\0' and u16(nt,24)==0x20b,'Requires PE64')
        local count,optional=u16(nt,6),u16(nt,20)
        assert(count>0 and count<=96 and optional>=112 and optional<=512,'Invalid PE layout')
        local imageSize=u32(nt,80)
        local sections=assert(api.read(base+ntOffset+24+optional,count*40),'Cannot read module sections')
        local result,seen,total={}, {},0
        for i=0,count-1 do
            local o=i*40
            local size,rva,flags=u32(sections,o+8),u32(sections,o+12),u32(sections,o+36)
            if bit.band(flags,0x40000000)~=0 and bit.band(flags,0x80000000)~=0
              and bit.band(flags,0x20000000)==0 then
                -- The supported game.dll has a 17.61 MiB writable section.
                -- Read small blocks, including initialized globals in its zero-fill tail.
                assert(size<=33554432 and rva+size<=imageSize,'Data section exceeds scan bounds')
                total=total+size; assert(total<=33554432,'Module data exceeds scan bounds')
                for offset=0,size-1,65536 do
                    local length=math.min(65536,size-offset)
                    local data=assert(api.read(base+rva+offset,length),'Module data unavailable')
                    -- Filter zero and noncanonical slots without allocating a cdata per slot.
                    local words=ffi.cast('const uintptr_t *',ffi.cast('const char *',data))
                    for index=0,math.floor(length/8)-1 do
                        local v=words[index]
                        if v>=65536 and v<0x800000000000 then
                            local key=tonumber(v)
                            if not seen[key] then
                                seen[key]=true
                                local p=ffi.cast('uint8_t *',v)
                                -- Read only a header at a module-owned pointer target.
                                local h=api.read(p,16)
                                if h and ((header and h==header) or (not header and u32(h,0)==2 and h:sub(5,8)=='LDLD' and u32(h,12)==3943969754)) then
                                    result[#result+1]=p
                                end
                            end
                        end
                        if api.checkpoint and index%256==255 then api.checkpoint('Scanning damage table') end
                    end
                    if api.progress then api.progress(total-size+offset+length,total,#result) end
                    if api.checkpoint then api.checkpoint('Scanning damage table',true) end
                end
            end
        end
        if #result==0 and signature then
            if api.checkpoint then api.checkpoint('No module pointer found; searching private data',true) end
            return api.scan_private(signature,row_offset)
        end
        return result
    end
    -- Keep external memory-region state and pointer arithmetic interpreted.
    -- Hot-loop optimization can otherwise skip the high-address read-only cache.
    local runtime_jit=rawget(_G,'jit')
    if runtime_jit and runtime_jit.off then
        runtime_jit.off(api.private_regions,true)
        runtime_jit.off(api.scan_private,true)
        runtime_jit.off(api.scan_private_many,true)
    end
    return api
end

end)()
local function bytes(s) return (s:gsub('..',function(h) return string.char(tonumber(h,16)) end)) end
local function u32(s,o)
    if not s or o<0 or o+4>#s then return nil end
    local a,b,c,d=s:byte(o+1,o+4); return a+b*256+c*65536+d*16777216
end
local function f32(n) local ffi=require('ffi');local v=ffi.new('float[1]',n);return ffi.string(v,4) end
local function reject(why) error('Pacifier validation failed: '..why..'; no edit applied') end
local function expect(api,p,data,label)
    if api.read(p,#data)~=data then reject(label) end
end
local function apply(api,game)
    local owner=api.pointer(api.read(game+0x346bf98,8),0)
    local map=owner and api.pointer(api.read(owner+0xf12bd8,8),0)
    if not map then reject('entity manager') end
    local entities=map-26151684
    local weapon_group=entities+26151656
    expect(api,weapon_group,bytes('b1dbe4884c444c4401000000b1dbe488000f07000100000000000000'),'WeaponData header')
    local weapon=weapon_group+11680+275*1232
    local owner_hits=0
    for i=0,729 do
        local entry=api.read(weapon_group+28+i*16,16)
        if entry and entry:sub(1,8)==bytes('6B69DF66366129BC') then
            owner_hits=owner_hits+1
            if u32(entry,8)~=275 then reject('WeaponData owner row') end
        end
    end
    if owner_hits~=1 then reject('unique WeaponData owner') end
    local damage_root=api.pointer(api.read(game+55108088,8),0)
    if not damage_root or u32(api.read(damage_root,4),0)~=2 then reject('damage root') end
    local damage_group=damage_root+60
    expect(api,damage_group,bytes('4c444c4401000000f02ca7e0bcc000000100000000000000'),'damage group header')
    local payload=api.read(damage_group+24,16)
    local rows=payload and api.pointer(payload,0)
    if not rows or api.distance(rows,damage_group+24)~=16 or u32(payload,8)~=649 then reject('damage rows') end
    local damage=rows+121*76
    if u32(api.read(damage,4),0)~=116 then reject('Pacifier damage row') end
    -- The attachment delta is a private read-only allocation. Identify it by its
    -- exact versioned header, then re-prove its option and component chain.
    local deltas={}
    for _,region in ipairs(api.private_regions(true)) do
        local head=api.read(region.base,28)
        if region.size>=349570 and region.size<=349570+65536
          and head==bytes('4f603e684c444c44010000004f603e68665505000100000000000000') then
            deltas[#deltas+1]=region.base
        end
        if api.checkpoint then api.checkpoint('Locating magazine data') end
    end
    if #deltas~=1 then reject('unique magazine allocation ('..#deltas..')') end
    local delta=deltas[1]+28
    local offsets={80,16464,25440,37572,110052}
    local counts={1024,1122,1011,6040,239490}
    local header=api.read(delta,80)
    for i=1,5 do
        local p=api.pointer(header,(i-1)*16)
        if not p or api.distance(p,delta)~=offsets[i] or u32(header,(i-1)*16+8)~=counts[i] then reject('magazine table '..i) end
    end
    local slot=api.read(delta+80+919*16,16)
    if not slot or slot:sub(1,8)~=bytes('964F3BC03B26AB16') or u32(slot,8)~=153 then reject('magazine option owner') end
    local setting=api.read(delta+16464+153*8,8)
    local count,first=u32(setting,0),u32(setting,4)
    if not count or count<1 or count>64 or first+count>1011 then reject('magazine setting bounds') end
    local matches=0
    for i=0,count-1 do
        local c=api.read(delta+25440+(first+i)*12,12)
        local component,start,n=u32(c,0),u32(c,4),u32(c,8)
        if not n or n<1 or n>64 or start+n>6040 then reject('magazine component bounds') end
        if component==5 then
            matches=matches+1
            local hit=0
            for j=0,n-1 do
                local d=api.read(delta+37572+(start+j)*12,12)
                local offset,size,raw=u32(d,0),u32(d,4),u32(d,8)
                if not raw or raw+size>239490 then reject('magazine delta bounds') end
                if offset==136 then
                    hit=hit+1
                    if size~=4 or 28+110052+raw~=170592 then reject('magazine delta target') end
                end
            end
            if hit~=1 then reject('unique magazine capacity field') end
        end
    end
    if matches~=1 then reject('unique magazine component') end
    local changes={
        {damage+4,bytes('37000000'),bytes('41000000'),'normal damage'},
        {damage+8,bytes('0a000000'),bytes('0f000000'),'durable damage'},
        {damage+48,f32(2),f32(3),'stun buildup'},
        {weapon+104,f32(1),f32(0.6),'sway'},
        {deltas[1]+170592,bytes('28000000'),bytes('41000000'),'magazine capacity'},
    }
    for _,c in ipairs(changes) do
        expect(api,c[1],c[2],c[4])
        if not api.data_access(c[1],#c[2]) then reject(c[4]..' access') end
    end
    local applied={}
    for _,c in ipairs(changes) do
        applied[#applied+1]=c
        if not api.write_protected(c[1],c[3]) or api.read(c[1],#c[3])~=c[3] then
            local restored=true
            for i=#applied,1,-1 do
                local old=applied[i]
                if not api.write_protected(old[1],old[2]) or api.read(old[1],#old[2])~=old[2] then restored=false end
            end
            error('Pacifier transaction failed at '..c[4]..'; rollback '..(restored and 'verified' or 'FAILED'))
        end
    end
    return 'Applied: AR-32 Pacifier five guarded field writes'
end
local function start(make_api)
    if rawget(_G,'AR32PacifierLuaV1') then return end
    local state={version='0.1.3-test',active=false,status='Waiting for game update'}
    _G.AR32PacifierLuaV1=state
    local previous=update
    local callback
    local function log()
        pcall(function()
            local loader=rawget(_G,'CowboyBingusModLoader')
            local file=loader and loader.open_log and loader.open_log('AR32PacifierLua.log')
            if file then file:write('AR32PacifierLua '..state.version..'\nstatus='..state.status..'\n');file:close() end
        end)
    end
    local deadline=0
    local worker=coroutine.create(function()
        local api=make_api()
        api.checkpoint=function(stage,force)
            state.status=stage
            if force or os.clock()>=deadline then coroutine.yield() end
        end
        local exe,game=api.module(nil),api.module('game.dll')
        assert(exe and game,'Game modules unavailable')
        assert(api.module_hash(exe)=='F5FEE03DCFDB2E553A4752C283590950AC13316B376D8196AA556FF0400D5F06','Unsupported executable; no edit applied')
        assert(api.module_hash(game)=='2E2C3B7C2500646DADD5F2B4C6E0504DBB7E7896139F64CDDC0D1813C718F51E','Unsupported game.dll; no edit applied')
        local ready_at=api.now()+30000
        repeat coroutine.yield() until api.now()>=ready_at
        local peer_deadline=api.now()+120000
        while true do
            local blocker
            for _,peer in ipairs({
                {'FlagDamagePrototypeV1','Flag'}, {'SaiFocusModV1','SAI'},
                {'SterilizerArmorControlV1','Gas Overhaul'}, {'AC8BackpacklessV1','AC-8'},
            }) do
                local peer_state=rawget(_G,peer[1])
                if peer_state and not peer_state.active then blocker=peer[2];break end
            end
            local profiles=rawget(_G,'ArsenalPreset01Runtime')
            if not blocker and profiles and profiles.phase~='ready' then blocker='Arsenal profiles' end
            local scorcher=rawget(_G,'ScorcherARRuntime')
            if not blocker and scorcher and scorcher.phase~='ready' then blocker='Scorcher runtime' end
            if not blocker then break end
            assert(api.now()<peer_deadline,'Companion not ready: '..blocker..'; no edit applied')
            state.status='Waiting for '..blocker;log();coroutine.yield()
        end
        state.status='Validating Pacifier data';log()
        return apply(api,game)
    end)
    local function tick()
        if coroutine.status(worker)=='dead' then return end
        deadline=os.clock()+0.0007
        local ok,message=coroutine.resume(worker)
        if not ok or coroutine.status(worker)=='dead' then
            state.active,state.status=ok,tostring(message)
            print('[AR32PacifierLua] '..state.status);log()
            if update==callback then update=previous or function() end end
        end
    end
    callback=function(...) if previous then local result=previous(...);tick();return result end;tick() end
    update=callback;log()
end
start(make_api)
