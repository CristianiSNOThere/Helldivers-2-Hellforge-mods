local make_api=(function()
return function()
    local ffi = require('ffi')
    assert(ffi.abi('64bit'), 'Requires Windows x64 LuaJIT')
    ffi.cdef [[
        void *GetModuleHandleA(const char *);
        void *GetCurrentProcess(void);
        uint32_t GetCurrentProcessId(void);
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
        } Vg70VariableMemoryInfoV1;
    ]]
    local k, b = ffi.load('kernel32'), ffi.load('bcrypt')
    local self = k.GetCurrentProcess()
    local api = {}
    function api.byte_pointer(value) return ffi.cast('uint8_t *',value) end
    -- Canonical user addresses are below 2^47 and exactly representable as doubles.
    -- The game may hide every cdata value behind the same tostring result.
    function api.identity(p) return tonumber(ffi.cast('uintptr_t',p)) end
    function api.now() return tonumber(k.GetTickCount64()) end
    function api.module(name)
        local module=k.GetModuleHandleA(name)
        if not module or ffi.cast('uintptr_t',module)==0 then return nil end
        return ffi.cast('uint8_t *',module)
    end
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
        local info=ffi.new('Vg70VariableMemoryInfoV1[1]')
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
        local info=ffi.new('Vg70VariableMemoryInfoV1[1]')
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
        assert(#s==4 or #s==8,'Only validated 4-byte or 8-byte data edits may be written')
        local info=ffi.new('Vg70VariableMemoryInfoV1[1]')
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
        module=ffi.cast('void *',module)
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
        local info=ffi.new('Vg70VariableMemoryInfoV1[1]')
        local cursor=ffi.cast('uint8_t *',65536)
        while ffi.cast('uintptr_t',cursor)<0x800000000000 do
            if k.VirtualQuery(cursor,ffi.cast('void *',info),ffi.sizeof(info[0]))~=ffi.sizeof(info[0]) then break end
            local size=tonumber(info[0].size)
            local next_address=ffi.cast('uint8_t *',info[0].base)+size
            assert(size>0 and next_address>cursor,'Invalid memory-region layout')
            if info[0].state==0x1000 and info[0].kind==0x20000 and (info[0].protection==4 or (include_readonly and info[0].protection==2)) then
                regions[#regions+1]={base=ffi.cast('uint8_t *',info[0].base),size=size,protection=tonumber(info[0].protection)}
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
                if api.checkpoint then api.checkpoint('Searching private data for VG-70 Variable records') end
            end
        end
        return results
    end
    function api.candidates(module,signature,row_offset,header)
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
local component_profile=(function()
-- Pure byte transformations for the guarded VG-70 profile fields.
-- The caller stages the returned bytes in one transaction; this module never writes memory.
local ffi=require('ffi')
local M={}

local PROJECTILES={[94]=true,[333]=true,[335]=true,[336]=true,[337]=true,[338]=true}
local AUTO_RATES={550,1000,750}
local FIRE_NODES={0x51c7f869,0x305559a7,0x1f6cba2f,0xfa44604f,0x152c80cb,0xf5c2ebc6,0xb193e9e2}
local CACHED_NODES={24,22,21,20,19,18,17}

local function u32(s,o)
    assert(type(s)=='string' and o>=0 and o+4<=#s,'Unreadable 32-bit component field')
    local a,b,c,d=s:byte(o+1,o+4)
    return a+b*256+c*65536+d*16777216
end
local function word(v)
    return string.char(v%256,math.floor(v/256)%256,math.floor(v/65536)%256,math.floor(v/16777216)%256)
end
local function f32(s,o)
    assert(type(s)=='string' and o>=0 and o+4<=#s,'Unreadable float component field')
    local v=ffi.new('float[1]')
    ffi.copy(v,s:sub(o+1,o+4),4)
    return tonumber(v[0])
end
local function fbytes(n)
    assert(type(n)=='number' and n==n and math.abs(n)<1e7,'Invalid float component value')
    local v=ffi.new('float[1]',n)
    return ffi.string(v,4)
end
local function replace(s,o,v)
    return s:sub(1,o)..v..s:sub(o+#v+1)
end
local function close(a,b)
    return math.abs(a-b)<=0.0001
end

function M.weapon_data(row,mode)
    assert(#row==1232,'Variable WeaponData row size changed')
    assert(u32(row,140)==7 or u32(row,140)==15,'Variable burst-round field changed')
    assert(close(f32(row,84),5) and close(f32(row,88),5),'Variable source spread changed')
    assert(close(f32(row,104),1) or close(f32(row,104),0.9),'Variable source sway changed')
    assert(close(f32(row,356),25) or close(f32(row,356),40),'Variable base ergonomics changed')
    assert(mode==nil or mode==1 or mode==4 or mode==7,'Unsupported Variable component mode')
    local count=mode==4 and 15 or 7
    local out=replace(row,140,word(count))
    for index=0,23 do
        local offset=204+index*4
        local known=index<7 and FIRE_NODES[index+1] or 0
        local repeated=index<15 and FIRE_NODES[index%7+1] or 0
        local actual=u32(row,offset)
        assert(actual==known or actual==repeated,'Variable source fire-node mapping changed')
        local target=index<count and FIRE_NODES[index%7+1] or 0
        out=replace(out,offset,word(target))
    end
    for _,item in ipairs({{0,5},{4,10},{28,4},{32,8}})do
        local actual=f32(row,item[1])
        assert(close(actual,item[2]) or close(actual,item[2]*0.85),'Variable recoil source changed')
        out=replace(out,item[1],fbytes(item[2]*0.85))
    end
    out=replace(out,104,fbytes(0.9))
    return replace(out,356,fbytes(40))
end

function M.projectile_weapon(row,projectile_id,rates)
    assert(#row==616,'Variable ProjectileWeapon row size changed')
    assert(PROJECTILES[u32(row,0)],'Variable projectile identity changed')
    assert(PROJECTILES[projectile_id],'Unsupported Variable projectile profile')
    local before={f32(row,4),f32(row,8),f32(row,12)}
    local stock=close(before[1],300) and close(before[2],550) and close(before[3],750)
    local auto=close(before[1],550) and close(before[2],1000) and close(before[3],750)
    local volley=close(before[1],550) and close(before[2],550) and close(before[3],550)
    assert(stock or auto or volley,'Variable RPM component vector changed')
    assert(type(rates)=='table' and #rates==3,'Variable RPM profile is missing')
    local auto=rates[1]==550 and rates[2]==1000 and rates[3]==750
    assert(auto,'Unsupported Variable RPM profile')
    local selected=AUTO_RATES
    local out=replace(row,0,word(projectile_id))
    for i,offset in ipairs({4,8,12}) do out=replace(out,offset,fbytes(selected[i])) end
    return out
end

function M.rates(row)
    assert(#row==616,'Variable ProjectileWeapon row size changed')
    assert(PROJECTILES[u32(row,0)],'Variable projectile identity changed')
    local rates={f32(row,4),f32(row,8),f32(row,12)}
    local stock=close(rates[1],300) and close(rates[2],550) and close(rates[3],750)
    local auto=close(rates[1],550) and close(rates[2],1000) and close(rates[3],750)
    local volley=close(rates[1],550) and close(rates[2],550) and close(rates[3],550)
    assert(stock or auto or volley,'Variable RPM component vector changed')
    return rates
end

function M.magazine(row)
    assert(#row==160,'Variable WeaponMagazine row size changed')
    assert(u32(row,136)==49 or u32(row,136)==150,'Variable magazine capacity changed')
    assert(u32(row,140)==6 and u32(row,144)==7 and u32(row,148)==7,
        'Variable reserve-magazine settings changed')
    return replace(row,136,word(150))
end

function M.weapon_state(row,mode)
    assert(#row==1008,'Variable WeaponDataState row size changed')
    assert(mode==1 or mode==4 or mode==7,'Unsupported Variable state mode')
    local count=u32(row,0xd8)
    assert(count==7 or count==15,'Variable derived fire-node count changed')
    local counter=u32(row,0xdc)
    assert(counter<count,'Variable fire-node counter changed')
    local mx,my=f32(row,0x3e0),f32(row,0x3e8)
    assert(mx>0 and mx<=100 and my>0 and my<=100,'Variable spread multiplier changed')
    local x,y=f32(row,0x58),f32(row,0x5c)
    local base_x,base_y=5*mx,5*my
    assert(close(x,base_x) or close(x,0.75),'Variable horizontal spread changed')
    assert(close(y,base_y) or close(y,0.75),'Variable vertical spread changed')
    local target_count=mode==4 and 15 or 7
    local out=row
    for index=0,23 do
        local offset=0x78+index*4
        local known=index<7 and CACHED_NODES[index+1] or 0
        local repeated=index<15 and CACHED_NODES[index%7+1] or 0
        local actual=u32(row,offset)
        assert(actual==known or actual==repeated,'Variable cached fire-node mapping changed')
        local target=index<target_count and CACHED_NODES[index%7+1] or 0
        out=replace(out,offset,word(target))
    end
    -- Exact constructor 7525AB..752638 derives these four fields from the
    -- source recoil and the native global/per-axis handling modifiers.
    for _,item in ipairs({{0x1c,5,0x3b0,0x3c0},{0x20,10,0x3b8,0x3c8},
                           {0x38,4,0x3b0,0x3d0},{0x3c,8,0x3b8,0x3d8}})do
        local first,second=f32(row,item[3]),f32(row,item[4])
        assert(first>=0 and first<=10 and second>=0 and second<=10,'Variable recoil modifiers unsupported')
        local stock=item[2]*first*second
        local actual=f32(row,item[1])
        assert(close(actual,stock) or close(actual,stock*0.85),'Variable derived recoil changed')
        out=replace(out,item[1],fbytes(stock*0.85))
    end
    out=replace(out,0xd8,word(target_count))
    -- Preserve the physical barrel phase when leaving the expanded Volley map.
    out=replace(out,0xdc,word(counter%target_count))
    out=replace(out,0x58,fbytes(mode==7 and 0.75 or base_x))
    out=replace(out,0x5c,fbytes(mode==7 and 0.75 or base_y))
    return out
end

function M.rate_state(row,slot,rates)
    assert(#row==32,'Variable rate-of-fire state size changed')
    assert(slot==0 or slot==1 or slot==2,'Unsupported Variable RPM slot')
    assert(u32(row,28)<=2,'Variable selected RPM slot changed')
    local before={f32(row,16),f32(row,20),f32(row,24)}
    local stock=close(before[1],300) and close(before[2],550) and close(before[3],750)
    local auto=close(before[1],550) and close(before[2],1000) and close(before[3],750)
    local volley=close(before[1],550) and close(before[2],550) and close(before[3],550)
    assert(stock or auto or volley,'Variable rate-state vector changed')
    assert(type(rates)=='table' and #rates==3,'Variable rate-state profile missing')
    local out=row
    for i,offset in ipairs({16,20,24}) do out=replace(out,offset,fbytes(rates[i])) end
    return replace(out,28,word(slot))
end

function M.current_rpm_state(row,rpm)
    assert(#row==12,'Variable current-RPM state size changed')
    assert(rpm==550 or rpm==750 or rpm==1000,'Unsupported Variable current RPM')
    local current=f32(row,4)
    assert(current==300 or current==550 or current==750 or current==1000,
        'Variable current RPM changed externally')
    return replace(row,4,fbytes(rpm))
end

function M.rpm_vector(row)
    assert(#row==616,'Variable ProjectileWeapon row size changed')
    return {f32(row,4),f32(row,8),f32(row,12)}
end

function M.current_rpm(row)
    assert(#row==12,'Variable current-RPM state size changed')
    return f32(row,4)
end

function M.selected_slot(row)
    assert(#row==32,'Variable rate-of-fire state size changed')
    local slot=u32(row,28)
    assert(slot==0 or slot==1 or slot==2,'Unsupported Variable RPM slot')
    return slot
end

function M.rate_vector(row)
    assert(#row==32,'Variable rate-of-fire state size changed')
    local rates={f32(row,16),f32(row,20),f32(row,24)}
    local stock=rates[1]==300 and rates[2]==550 and rates[3]==750
    local current=rates[1]==550 and rates[2]==1000 and rates[3]==750
    assert(stock or current,'Variable native selector vector changed')
    return rates
end

function M.cadence(state,current_rpm,target_rpm)
    assert(#state==168,'Variable ProjectileState row size changed')
    assert(current_rpm==300 or current_rpm==550 or current_rpm==750 or current_rpm==1000,
        'Variable current RPM changed externally')
    assert(target_rpm==550 or target_rpm==750 or target_rpm==1000,
        'Unsupported Variable target RPM')
    local interval=f32(state,12)
    assert(interval>0 and interval<2,'Variable firing interval changed externally')
    local multiplier=60/(current_rpm*interval)
    assert(multiplier>=0.5 and multiplier<=2,'Variable native cadence modifier is unsupported')
    local out=replace(state,12,fbytes(60/(target_rpm*multiplier)))
    return out,multiplier
end

return M

end)()
local mode_profile=(function()
local M={}
local SLOT_BY_RATE={[550]=0,[1000]=1,[750]=2}
local RATES={550,1000,750}
local PROFILE={auto={[550]=333,[750]=335,[1000]=336},volley=337,total=338}

function M.step(memory,identity,mode,selected_slot,current_rpm,current_vector,force_default)
    assert(mode==1 or mode==4 or mode==7,'Unsupported Variable firing mode')
    assert(selected_slot==0 or selected_slot==1 or selected_slot==2,'Unsupported native rate slot')
    current_vector=current_vector or RATES
    assert(type(current_vector)=='table' and #current_vector==3,'Native RPM vector missing')
    local stock=current_vector[1]==300 and current_vector[2]==550 and current_vector[3]==750
    local current=current_vector[1]==550 and current_vector[2]==1000 and current_vector[3]==750
    assert(stock or current,'Unsupported Variable RPM vector')
    assert(current_vector[selected_slot+1]==current_rpm,'Native RPM and selector slot disagree')
    local prior=memory[identity]
    if prior then assert(prior.identity==identity,'Variable identity changed; stale mode state') end
    local rpm=force_default and 1000 or (current_rpm==300 and 1000 or current_rpm)
    assert(SLOT_BY_RATE[rpm],'Unsupported native Variable rate')
    -- Native selection stays shared across all modes. Only Auto damage scales
    -- with RPM; Volley and Total keep their fixed damage profiles.
    memory[identity]={identity=identity,auto_rpm=rpm,last_mode=mode}
    return {rpm=rpm,slot=SLOT_BY_RATE[rpm],
        projectile=mode==4 and PROFILE.volley or (mode==7 and PROFILE.total or PROFILE.auto[rpm]),
        rpm_vector=RATES,locked=false,remembered_auto_rpm=rpm}
end

function M.forget_missing(memory,live)
    for identity in pairs(memory) do
        if not live[identity] then memory[identity]=nil end
    end
end

return M

end)()
local compatibility=(function()
return function(globals)
    local peers={
        {'FlagDamagePrototypeV1','One True Flag'},
        {'SaiFocusModV1','SAI Focus Precision'},
        {'AR11ArbitratorModV1','AR-11 Arbitrator'},
        {'SterilizerArmorControlV1','Gas Overhaul'},
        {'RapidArcThrowerV1','ARC-3 Rapid/Supercharge'},
        {'AC8BackpacklessV1','AC-8 Autocannon'},
        {'AR32PacifierLuaV1','AR-32 Pacifier'},
        {'EpochChargedIncendiaryV1','PLAS-45 Epoch'},
    }
    for _,peer in ipairs(peers) do
        local other=rawget(globals,peer[1])
        if other and other.active~=true then return peer[2] end
    end
    local profiles=rawget(globals,'ArsenalPreset01Runtime')
    if profiles and profiles.phase~='ready' then return 'Arsenal profiles' end
    local scorcher=rawget(globals,'ScorcherARRuntime')
    if scorcher and scorcher.phase~='ready' then return 'Scorcher runtime' end
    return nil
end

end)()
local transaction_profile=(function()
return function(api,edits,validate)
    if validate then validate() end
    table.sort(edits,function(a,b)
        if a.priority~=b.priority then return a.priority<b.priority end
        return a.order<b.order
    end)
    for _,item in ipairs(edits) do
        assert(api.read(item.address,#item.before)==item.before,item.label..' changed before commit')
        local accessible=item.protected and api.data_access(item.address,#item.before)
            or (not item.protected and api.writable(item.address,#item.before))
        assert(accessible,item.label..' memory permissions changed')
    end
    local attempted={}
    local ok,reason=pcall(function()
        for _,item in ipairs(edits) do
            attempted[#attempted+1]=item
            local write=item.protected and api.write_protected or api.write
            assert(write(item.address,item.after),'short write at '..item.label)
            assert(api.read(item.address,#item.after)==item.after,'readback failed at '..item.label)
        end
    end)
    if not ok then
        local restored=true
        for i=#attempted,1,-1 do
            local item=attempted[i]
            local current=api.read(item.address,#item.after)
            if current==item.after then
                local write=item.protected and api.write_protected or api.write
                if not write(item.address,item.before) or api.read(item.address,#item.before)~=item.before then
                    restored=false
                end
            elseif current~=item.before then restored=false end
        end
        error(tostring(reason)..'; rollback='..tostring(restored))
    end
    return #edits
end

end)()
local native=(function()
-- Exact-build VG-70 instance discovery and preallocated cache isolation.
-- This module only describes guarded reads/staged edits; caller commits atomically.
local native={}
local bit=require('bit')
local OWNER='7f8a7c5797ce92f9'
local function unhex(s)return(s:gsub('..',function(p)return string.char(tonumber(p,16))end))end
OWNER=unhex(OWNER)
local function u32(s,o)
 assert(type(s)=='string' and o>=0 and o+4<=#s,'Native field unreadable')
 local a,b,c,d=s:byte(o+1,o+4);return a+b*256+c*65536+d*16777216
end
local function word(v)return string.char(v%256,math.floor(v/256)%256,math.floor(v/65536)%256,math.floor(v/16777216)%256)end
local function read(api,p,n)return assert(api.read(p,n),'Native data unavailable')end
local function pointer(api,s,o)return assert(api.pointer(s,o),'Native pointer unavailable')end
local function lookup(api,h,o,key)
 local cap,empty,mult=u32(h,o+8),u32(h,o+12),u32(h,o+16)
 assert(cap<=4096 and (cap==0 or bit.band(cap,cap-1)==0),'Native map capacity changed')
 if cap==0 then return nil end
 local array=pointer(api,h,o)
 local product=((key%65536)*mult+math.floor(key/65536)*(mult%65536)*65536)%4294967296
 for probe=0,math.min(cap,64)-1 do
  local row=read(api,array+bit.band(product+probe,cap-1)*8,8)
  local identity,index=u32(row,0),u32(row,4)
  if identity==key then return index~=4294967295 and index or nil end
  if identity==empty then return nil end
 end
 error('Native map probe bound reached')
end

function native.prepare(api,components)
 local game=api.module('game.dll')
 assert(game,'Game module unavailable')
 local owner=pointer(api,read(api,game+0x346bf98,8),0)
 local map=pointer(api,read(api,owner+0xf12bd8,8),0)
 local entities=map -- component offsets are relative to this ownership-table anchor
 local function component(name)
  local spec=assert(components[name],'Missing component specification '..name)
  local group=entities+spec.group
  assert(read(api,group,28)==unhex(spec.header),name..' group header changed')
  assert(read(api,entities+spec.map,16)==unhex(spec.owner),name..' owner mapping changed')
  local row=entities+spec.row
  local actual=read(api,row,#spec.before/2)
  assert(actual==unhex(spec.before) or (spec.after and actual==unhex(spec.after)),
   name..' exact source row changed')
  assert(api.data_access(row,#spec.before/2),name..' source row is inaccessible')
  return row
 end
 local projectile_map=pointer(api,read(api,owner+0xf12e80,8),0)
 assert(projectile_map==entities+components.projectile.table_map,'ProjectileWeapon owner table changed')
 assert(map==entities+components.weapon.table_map,'WeaponData owner table changed')
 return {game=game,owner=owner,entities=entities,weapon_map=map,projectile_map=projectile_map,
  weapon_address=component('weapon'),projectile_address=component('projectile'),
  magazine_address=component('magazine')}
end

function native.instances(api,plan)
 assert(pointer(api,read(api,plan.native.game+0x346bf98,8),0)==plan.native.owner,
  'Native entity manager changed; restart required')
 assert(pointer(api,read(api,plan.native.owner+0xf12bd8,8),0)==plan.native.weapon_map
  and pointer(api,read(api,plan.native.owner+0xf12e80,8),0)==plan.native.projectile_map,
  'Native entity component tables changed; restart required')
 local wm=pointer(api,read(api,plan.native.game+0x3326ce0,8),0)
 local wh=read(api,wm,192)
 local count=u32(wh,0x1c)
 assert(count<=512 and count<=u32(wh,0x10),'Native Variable owner count changed')
 local refs=pointer(api,wh,0x48)
 local states=pointer(api,wh,0x58)
 local selections=pointer(api,wh,0x60)
 local pm=pointer(api,read(api,plan.native.game+0x33266d8,8),0)
 local ph=read(api,pm,224)
 local psi_count=u32(ph,0x38)
 assert(psi_count<=512,'Native projectile-state count changed')
 local psi_refs=pointer(api,ph,0x68)
 local projectile_states=pointer(api,ph,0x78)
 local rof_states=pointer(api,ph,0x70)
 local rpm_states=pointer(api,ph,0x80)
 local mm=pointer(api,read(api,plan.native.game+0x3326648,8),0)
 local mh=read(api,mm,256)
 local mag_count=u32(mh,0x10)
 assert(mag_count<=512 and mag_count<=u32(mh,0x0),'Native magazine owner count changed')
 local magazine_refs=pointer(api,mh,0x38)
 local ammo_rows=pointer(api,mh,0x50)
 local effective_rows=pointer(api,mh,0x48)
 local weapon_rows=pointer(api,wh,0xb0)
 local projectile_rows=pointer(api,ph,0xd0)
 local magazine_rows=pointer(api,mh,0xa0)
 local result,live={},{}
 for slot=0,count-1 do
  local reference=read(api,refs+slot*8,8)
  local entity=reference~=string.rep('\0',8) and pointer(api,reference,0) or nil
  if entity then
   local identity=read(api,entity,24)
   if identity:sub(1,8)==OWNER then
    assert(bit.band(u32(identity,20),1)==1,'VG-70 entity is not player-owned')
    local id=u32(identity,8)
    local wi=lookup(api,wh,0x30,id)
    assert(wi and wi<count,'VG-70 WeaponData index unavailable')
    assert(pointer(api,read(api,refs+wi*8,8),0)==entity,'VG-70 WeaponData owner reference changed')
  local mode_address=selections+wi*12
    local mode=u32(read(api,mode_address,4),0)
    assert(mode==1 or mode==4 or mode==7,'Unsupported VG-70 native mode '..mode)
    local weapon_private=lookup(api,wh,0x70,id)
    if weapon_private then assert(weapon_private<u32(wh,0xa8) and lookup(api,wh,0x90,weapon_private)==id,'WeaponData cache owner/index changed') end
    local projectile_private=lookup(api,ph,0x90,id)
    if projectile_private then assert(projectile_private<u32(ph,0xc8) and lookup(api,ph,0xb0,projectile_private)==id,'ProjectileWeapon cache owner/index changed') end
    local psi=lookup(api,ph,0x50,id)
    assert(psi and psi<psi_count,'VG-70 projectile state unavailable')
    assert(pointer(api,read(api,psi_refs+psi*8,8),0)==entity,'Projectile state owner mismatch')
    local mi=lookup(api,mh,0x20,id)
    assert(mi and mi<mag_count,'VG-70 magazine state unavailable')
    local reference=read(api,magazine_refs+mi*8,8)
    assert(pointer(api,reference,0)==entity,'Magazine owner reference mismatch')
    local magazine_private=lookup(api,mh,0x60,id)
    if magazine_private then assert(magazine_private<u32(mh,0x98) and lookup(api,mh,0x80,magazine_private)==id,'WeaponMagazine cache owner/index changed') end
    local weapon=weapon_private and weapon_rows+weapon_private*1232 or plan.native.weapon_address
    local projectile=projectile_private and projectile_rows+projectile_private*616 or plan.native.projectile_address
    local magazine=magazine_private and magazine_rows+magazine_private*160 or plan.native.magazine_address
    local state=states+wi*1008
    local key=tostring(id)..':'..tostring(api.identity(entity))
    live[key]=true
    result[#result+1]={id=id,key=key,entity=entity,identity=identity,slot=slot,mode=mode,
     mode_address=mode_address,weapon_index=wi,weapon=weapon,weapon_state=state,
     weapon_cached=weapon_private~=nil,projectile=projectile,projectile_cached=projectile_private~=nil,
     projectile_index=psi,projectile_state=projectile_states+psi*168,
     rate_state=rof_states+psi*32,current_rpm_state=rpm_states+psi*12,
     magazine_index=mi,magazine=magazine,magazine_cached=magazine_private~=nil,
     magazine_ammo=ammo_rows+mi*12,magazine_effective=effective_rows+mi*16}
    assert(read(api,entity,24)==identity,'VG-70 owner changed during discovery')
   end
  end
 end
 plan.snapshot={weapon_manager=wm,weapon_header=wh,projectile_manager=pm,projectile_header=ph,
  magazine_manager=mm,magazine_header=mh}
 return result,live
end

-- Reserve an instance-owned preallocated row and transactionally publish both maps.
function native.isolate(api,plan,instances,edits,seen)
 local snap=assert(plan.snapshot,'Instance snapshot missing')
 local function stage(address,before,after,clone)
  local key=api.identity(address)
  if seen[key] then
   if seen[key].clone then
    assert(seen[key].after==before,'Cloned row changed while configuring')
    seen[key].after=after;return
   end
   assert(seen[key].after==after,'Conflicting instance field edits')
   return
  end
  local item={address=address,before=before,after=after,clone=clone,
   label='VG-70 private cache field',protected=false,
   priority=clone and 1 or 3,order=#edits+1}
  seen[key]=item;edits[#edits+1]=item
 end
 local function reserve(header,offset,key,index,reserved)
  local cap,empty,mult=u32(header,offset+8),u32(header,offset+12),u32(header,offset+16)
  assert(cap>0 and cap<=4096 and bit.band(cap,cap-1)==0,'Private-map capacity changed')
  local array=pointer(api,header,offset);local product=((key%65536)*mult+math.floor(key/65536)*(mult%65536)*65536)%4294967296
  local tombstone
  for probe=0,math.min(cap,64)-1 do
   local at=array+bit.band(product+probe,cap-1)*8
   local address=api.identity(at)
   local row=reserved[address] or read(api,at,8)
   local stored,indexed=u32(row,0),u32(row,4)
   if stored==key then assert(indexed==4294967295,'Private-map key already exists') end
   if indexed==4294967295 and not tombstone then tombstone=at end
   if stored==empty or stored==key then
    local target=tombstone or at;reserved[api.identity(target)]=word(key)..word(index);return target
   end
  end
  if tombstone then reserved[api.identity(tombstone)]=word(key)..word(index);return tombstone end
  error('No private-map reservation available')
 end
 local function isolate(manager,header,capacity_off,count_off,forward_off,reverse_off,rows_off,stride,field,cached)
  local capacity,count=u32(header,capacity_off),u32(header,count_off)
  assert(count<=capacity and capacity<=512,'Private component cache capacity changed')
  local rows=pointer(api,header,rows_off);local next_index=count;local reserved,pub={},{}
  for _,item in ipairs(instances) do
   if not item[cached] then
    assert(next_index<capacity,'Preallocated private component cache exhausted')
    local forward=reserve(header,forward_off,item.id,next_index,reserved)
    local reverse=reserve(header,reverse_off,next_index,item.id,reserved)
    local source=read(api,item[field],stride)
    local target=rows+next_index*stride
    assert(api.writable(target,stride),'Private component row is not writable game data')
    local target_before=read(api,target,stride)
    for offset=0,stride-4,4 do
     stage(target+offset,target_before:sub(offset+1,offset+4),source:sub(offset+1,offset+4),true)
    end
    local reverse_before=read(api,reverse,8)
    local reverse_after=assert(reserved[api.identity(reverse)],'Private reverse-map reservation missing')
    stage(reverse+4,reverse_before:sub(5,8),reverse_after:sub(5,8))
    stage(reverse,reverse_before:sub(1,4),reverse_after:sub(1,4))
    pub[#pub+1]={address=forward,index=next_index,id=item.id}
    item[field]=target;item[field..'_row']=source;item[cached]=true
    next_index=next_index+1
   else item[field..'_row']=read(api,item[field],stride) end
  end
  if next_index~=count then
   stage(manager+count_off,word(count),word(next_index))
   for _,p in ipairs(pub) do
    local before=read(api,p.address,8);local empty=u32(header,forward_off+12)
    assert(u32(before,0)==empty or u32(before,4)==4294967295 or u32(before,0)==p.id,
     'Private map slot changed')
    stage(p.address+4,before:sub(5,8),word(p.index));stage(p.address,before:sub(1,4),word(p.id))
   end
  end
 end
 isolate(snap.weapon_manager,snap.weapon_header,0x14,0xa8,0x70,0x90,0xb0,1232,'weapon','weapon_cached')
 isolate(snap.projectile_manager,snap.projectile_header,0x30,0xc8,0x90,0xb0,0xd0,616,'projectile','projectile_cached')
 isolate(snap.magazine_manager,snap.magazine_header,4,0x98,0x60,0x80,0xa0,160,'magazine','magazine_cached')
end

function native.validate(api,plan,instances)
 local s=assert(plan.snapshot)
 assert(read(api,s.weapon_manager,192)==s.weapon_header
  and read(api,s.projectile_manager,224)==s.projectile_header
  and read(api,s.magazine_manager,256)==s.magazine_header,'Native component manager changed during transaction')
 for _,item in ipairs(instances) do
  assert(read(api,item.entity,24)==item.identity and u32(read(api,item.mode_address,4),0)==item.mode,
   'VG-70 owner or mode changed before transaction')
 end
end

return native

end)()
local start=(function()
return function(make_api,components,mode_profile,native,component_profile,compatibility,transaction_profile,component_specs,profile_specs)
    if rawget(_G,'Vg70VariableV1') then return end
    local state={version='0.1.0',active=false,status='Waiting for game data',instances=0,diagnostics={}}
    _G.Vg70VariableV1=state
    local previous=update
    local callback,api,plan
    local mode_memory={}
    local boot_sweep_done=false
    local deadline=0
    local function log(message)
        if message then
            state.diagnostics[#state.diagnostics+1]=tostring(message)
            while #state.diagnostics>12 do table.remove(state.diagnostics,1) end
        end
        pcall(function()
            local loader=rawget(_G,'CowboyBingusModLoader')
            local file=loader and loader.open_log and loader.open_log('Vg70Variable.log')
            if file then
                file:write('VG-70 Variable v0.1.0\nstatus='..tostring(state.status)..'\n'..table.concat(state.diagnostics,'\n')..'\n')
                file:close()
            end
        end)
    end
    local function checkpoint(label,force)
        state.status=label
        if force or os.clock()>=deadline then coroutine.yield() end
    end
    local function u32(s,o)
        assert(type(s)=='string' and o>=0 and o+4<=#s,'Unreadable VG-70 data field')
        local a,b,c,d=s:byte(o+1,o+4)
        return a+b*256+c*65536+d*16777216
    end
    local function pointer_at(api,s,o)
        return assert(api.pointer(s,o),'VG-70 data pointer unavailable')
    end
    local function unhex(s)
        return (s:gsub('..',function(p)return string.char(tonumber(p,16))end))
    end
    local function list_copy(source)
        local out={};for i,v in ipairs(source) do out[i]=v end;return out
    end
    local function add_row_edits(api,edits,seen,address,before,after,label,protected,priority)
        assert(#before==#after and #before%4==0,label..' row size changed')
        for offset=0,#before-4,4 do
            local old,new=before:sub(offset+1,offset+4),after:sub(offset+1,offset+4)
            if old~=new then
                local at=address+offset;local key=api.identity(at);local prior=seen[key]
                if prior then
                    assert(prior.after==old,label..' overlaps a conflicting edit')
                    prior.after=new
                else
                    assert(api.read(at,4)==old,label..' changed before transaction at +'..offset)
                    local item={address=at,before=old,after=new,label=label,protected=protected,
                        priority=priority or 2,order=#edits+1}
                    seen[key]=item;edits[#edits+1]=item
                end
            end
        end
    end
    local function table_rows(api,game,spec)
        local slot=game+spec.slot
        local root_bytes=assert(api.read(slot,8),'VG-70 table root unavailable')
        local root=pointer_at(api,root_bytes,0)
        assert(u32(assert(api.read(root,4),'VG-70 table root header unavailable'),0)==spec.groups,
            spec.name..' group count changed')
        local group=root+spec.group
        local header=assert(api.read(group,24),spec.name..' group header unavailable')
        assert(header:sub(1,4)=='LDLD' and u32(header,4)==1 and u32(header,8)==spec.kind
            and u32(header,12)==spec.size,spec.name..' group header changed')
        local payload=assert(api.read(group+24,16),spec.name..' rows descriptor unavailable')
        local rows=pointer_at(api,payload,0)
        assert(u32(payload,8)==spec.count and u32(payload,12)==0
            and api.distance(rows,group+24)==16,spec.name..' row array descriptor changed')
        local all=assert(api.read(rows,spec.count*spec.stride),spec.name..' table rows unavailable')
        local by_id={}
        for i=0,spec.count-1 do
            local id=u32(all,i*spec.stride)
            assert(not by_id[id],spec.name..' duplicate row identity')
            by_id[id]=i
        end
        local targets={}
        for _,record in ipairs(spec.records) do
            local index=by_id[record.id]
            assert(index==record.index,spec.name..' row '..record.id..' index changed')
            local actual=all:sub(index*spec.stride+1,(index+1)*spec.stride)
            local before,after=unhex(record.before),unhex(record.after)
            assert(actual==before or actual==after,spec.name..' row '..record.id..' exact baseline changed')
            targets[#targets+1]={address=rows+index*spec.stride,before=actual,after=after,
                label=spec.name..' row '..record.id,protected=true}
        end
        return {root_slot=slot,root_bytes=root_bytes,root=root,group=group,header=header,
            rows=rows,all=all,targets=targets}
    end
    local function global_profiles(api,game,native_plan)
        checkpoint('Validating VG-70 profile tables',true)
        local projectile=table_rows(api,game,profile_specs.projectile)
        local damage=table_rows(api,game,profile_specs.damage)
        local edits,seen={},{}
        for _,entry in ipairs(damage.targets) do
            add_row_edits(api,edits,seen,entry.address,entry.before,entry.after,entry.label,true,1)
        end
        for _,entry in ipairs(projectile.targets) do
            add_row_edits(api,edits,seen,entry.address,entry.before,entry.after,entry.label,true,1)
        end
        local component_rows={}
        for key,spec in pairs(component_specs) do
            local row=native_plan[key..'_address']
            local before=assert(api.read(row,#spec.before/2),key..' source component unavailable')
            local after
            if key=='weapon' then after=component_profile.weapon_data(before)
            elseif key=='projectile' then after=component_profile.projectile_weapon(before,336,{550,1000,750})
            elseif key=='magazine' then after=component_profile.magazine(before)
            else error('Unsupported VG-70 component profile '..key) end
            component_rows[#component_rows+1]={address=row,before=before,after=after,label=key..' source component',protected=true}
        end
        for _,entry in ipairs(component_rows) do
            add_row_edits(api,edits,seen,entry.address,entry.before,entry.after,entry.label,true,1)
        end
        local function validate()
            assert(api.read(projectile.root_slot,8)==projectile.root_bytes
                and api.read(damage.root_slot,8)==damage.root_bytes,'VG-70 table root changed before commit')
            assert(api.read(projectile.group,24)==projectile.header
                and api.read(damage.group,24)==damage.header,'VG-70 table header changed before commit')
            assert(api.read(projectile.rows,#projectile.all)==projectile.all
                and api.read(damage.rows,#damage.all)==damage.all,'VG-70 profile rows changed before commit')
            for _,entry in ipairs(component_rows) do
                assert(api.read(entry.address,#entry.before)==entry.before,entry.label..' changed before commit')
            end
        end
        local count=transaction_profile(api,edits,validate)
        for _,entry in ipairs(projectile.targets) do
            assert(api.read(entry.address,#entry.after)==entry.after,'Projectile profile readback failed')
        end
        for _,entry in ipairs(damage.targets) do
            assert(api.read(entry.address,#entry.after)==entry.after,'Damage profile readback failed')
        end
        return count,edits
    end
    local function peer_gate(api)
        local deadline_at=api.now()+120000
        while true do
            local blocker=compatibility(_G)
            if not blocker then return end
            assert(api.now()<deadline_at,'Companion not ready within 120 seconds: '..blocker..'; no VG-70 edits applied')
            state.status='Waiting for '..blocker
            checkpoint(state.status,false)
        end
    end
    local function configure_instances(api,native_plan,instances,live)
        local working={}
        for key,value in pairs(mode_memory) do
            working[key]={identity=value.identity,auto_rpm=value.auto_rpm,last_mode=value.last_mode}
        end
        local edits,seen={},{}
        native.isolate(api,native_plan,instances,edits,seen)
        for _,item in ipairs(instances) do
            local current_rpm=component_profile.current_rpm(assert(api.read(item.current_rpm_state,12)))
            local rate_row=assert(api.read(item.rate_state,32))
            local selected_slot=component_profile.selected_slot(rate_row)
            local vector=component_profile.rate_vector(rate_row)
            local force_default=boot_sweep_done and working[item.key]==nil
            local selection=mode_profile.step(working,item.key,item.mode,selected_slot,current_rpm,vector,force_default)
            local w_before=item.weapon_row
            local w_after=component_profile.weapon_data(w_before,item.mode)
            add_row_edits(api,edits,seen,item.weapon,w_before,w_after,'VG-70 WeaponData instance',false,1)
            local p_before=item.projectile_row
            local p_after=component_profile.projectile_weapon(p_before,selection.projectile,selection.rpm_vector)
            add_row_edits(api,edits,seen,item.projectile,p_before,p_after,'VG-70 ProjectileWeapon instance',false,1)
            local m_before=item.magazine_row
            local m_after=component_profile.magazine(m_before)
            add_row_edits(api,edits,seen,item.magazine,m_before,m_after,'VG-70 magazine instance',false,1)
            local state_before=assert(api.read(item.weapon_state,1008))
            local state_after=component_profile.weapon_state(state_before,item.mode)
            add_row_edits(api,edits,seen,item.weapon_state,state_before,state_after,'VG-70 WeaponDataState',false,2)
            local rate_after=component_profile.rate_state(rate_row,selection.slot,selection.rpm_vector)
            add_row_edits(api,edits,seen,item.rate_state,rate_row,rate_after,'VG-70 RPM selector state',false,2)
            local rpm_before=assert(api.read(item.current_rpm_state,12))
            local rpm_after=component_profile.current_rpm_state(rpm_before,selection.rpm)
            add_row_edits(api,edits,seen,item.current_rpm_state,rpm_before,rpm_after,'VG-70 current RPM',false,2)
            local projectile_before=assert(api.read(item.projectile_state,168))
            local projectile_after=component_profile.cadence(projectile_before,current_rpm,selection.rpm)
            add_row_edits(api,edits,seen,item.projectile_state,projectile_before,projectile_after,'VG-70 native cadence',false,2)
        end
        local changed=transaction_profile(api,edits,function() native.validate(api,native_plan,instances) end)
        mode_memory=working
        mode_profile.forget_missing(mode_memory,live)
        boot_sweep_done=true
        state.instances=#instances
        local previous_status=state.status
        state.status='Applied: VG-70 Variable 150-round magazine; Auto 550/750/1000 RPM; 15-round Volley follows native RPM; guarded muzzle mapping; ergonomics 40; sway 0.9; recoil -15%; stock Total self-injury and knockdown; Total 28/8 damage, AP4 and 0.75 spread.'
        if state.status~=previous_status then log() end
        if changed>0 then
            state.diagnostics[#state.diagnostics+1]='Updated '..changed..' guarded instance fields'
            while #state.diagnostics>12 do table.remove(state.diagnostics,1) end
        end
    end
    local worker=coroutine.create(function()
        api=make_api()
        api.note=function(message) state.diagnostics[#state.diagnostics+1]=tostring(message) end
        api.checkpoint=checkpoint
        local exe,game=api.module(nil),api.module('game.dll')
        assert(exe and game,'Game modules unavailable')
        local executable_hash=api.module_hash(exe)
        assert(executable_hash=='F5FEE03DCFDB2E553A4752C83E290950AC13316B376D8196AA556FF0400D5F06'
            or executable_hash=='F5FEE03DCFDB2E553A4752C283590950AC13316B376D8196AA556FF0400D5F06',
            'Unsupported executable; no edit applied')
        assert(api.module_hash(game)=='2E2C3B7C2500646DADD5F2B4C6E0504DBB7E7896139F64CDDC0D1813C718F51E',
            'Unsupported game.dll; no edit applied')
        local settle=api.now()+30000
        state.status='Waiting for game data and compatible weapon profiles';log()
        repeat coroutine.yield() until api.now()>=settle
        peer_gate(api)
        plan={native=native.prepare(api,component_specs)}
        state.status='Applying isolated VG-70 profiles';log()
        local global_writes=global_profiles(api,game,plan.native)
        state.active=true
        state.status='Applied: VG-70 Variable profiles ready; configuring owned instances.'
        log('Global profiles committed: '..global_writes..' guarded fields')
        while true do
            local ok,result=pcall(function()
                local instances,live=native.instances(api,plan)
                configure_instances(api,plan,instances,live)
            end)
            if not ok then
                state.status='Applied profiles; instance update refused safely: '..tostring(result)
                if state.diagnostics[#state.diagnostics]~=state.status then log(state.status) end
            end
            coroutine.yield()
        end
    end)
    local function init()
        if coroutine.status(worker)=='dead' then return end
        deadline=os.clock()+0.0007
        local ok,message=coroutine.resume(worker)
        if not ok then
            state.active=false
            state.status='Refused: '..tostring(message)
            log(state.status)
            if update==callback then update=previous or function() end end
        end
    end
    local function finish(...)
        init();return ...
    end
    callback=function(...)
        if previous then return finish(previous(...)) end
        init()
    end
    update=callback
    log()
end

end)()
local COMPONENTS={["weapon"]={["name"]="WeaponDataComponent",["group"]=-28,["header"]="b1dbe4884c444c4401000000b1dbe488000f07000100000000000000",["table_map"]=0,["map"]=2768,["owner"]="7f8a7c5797ce92f9ba00000000000000",["row"]=240832,["before"]="0000a040000020410000b4420000b44200000000cdcccc3d0000803f00008040000000410000b4420000b44200000000cdcccc3d0000803f0000003f0000803f0000803f0000803f0000803f0000803f0000803f0000a0400000a040000000000000803f0000803f0000803f0000803f000000000000803f0000803f0000803f0000803f070000000000803f0700000001000000040000000700000000000000000000000000000000000000000000000000000000000000030000000200000030eb953efde64a2d0000000069f8c751a75955302fba6c1f4f6044facb802c15c6ebc2f5e2e993b10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009a99193f3333333f6666663fec51b8bd5c8f42be00000000000000000000000000000000000000000000c8413333b33e000000000000c8c10000a04000000000000000000100010000000000000000000000000004000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000739c7c525a68254d0000000001000000827ef5c5508883125a68254de0dfb582ca1b91e1000000007d2319e60000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000803f0000803f0000803f000000000000803f0000803f0000803fd8cfeaf50000000071c36caec75d53eb42518be6840a00003200000000000000000000001200000000000000120000000000000012000000000000001200000000000000120000000000000012000000000000001200000000000000120000000000000000000000a0c7c3d5291e9a0855f346f280b6fbb20000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009d053fdb0f27b5a7d258dc2d000000000000803f00000000",["after"]="00008840000008410000b4420000b44200000000cdcccc3d0000803f9a9959409a99d9400000b4420000b44200000000cdcccc3d0000803f0000003f0000803f0000803f0000803f0000803f0000803f0000803f0000a0400000a040000000000000803f0000803f6666663f0000803f000000000000803f0000803f0000803f0000803f070000000000803f0700000001000000040000000700000000000000000000000000000000000000000000000000000000000000030000000200000030eb953efde64a2d0000000069f8c751a75955302fba6c1f4f6044facb802c15c6ebc2f5e2e993b10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009a99193f3333333f6666663fec51b8bd5c8f42be0000000000000000000000000000000000000000000020423333b33e000000000000c8c10000a04000000000000000000100010000000000000000000000000004000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000739c7c525a68254d0000000001000000827ef5c5508883125a68254de0dfb582ca1b91e1000000007d2319e60000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000000000000000803f0000803f0000803f000000000000803f0000803f0000803fd8cfeaf50000000071c36caec75d53eb42518be6840a00003200000000000000000000001200000000000000120000000000000012000000000000001200000000000000120000000000000012000000000000001200000000000000120000000000000000000000a0c7c3d5291e9a0855f346f280b6fbb20000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009d053fdb0f27b5a7d258dc2d000000000000803f00000000"},["projectile"]={["name"]="ProjectileWeaponComponent",["group"]=20253156,["header"]="681b17454c444c4401000000681b174560b002000100000000000000",["table_map"]=20253184,["map"]=20256176,["owner"]="7f8a7c5797ce92f98900000000000000",["row"]=20346248,["before"]="5e000000000096430080094400803b440000964200001643000096430000a04100000000000000000000000000000000000034430000803f00000000000020410000a040000000000e1a4a503e7843e40e1a4a503e7843e400000000000000000000a0400000204104000000d2720c5600000000b5e3ae9d000000000000803f00000000000000000000000000000000000000000000000000000000000000000000000000000000637d9b7800000000000000000000000093290adc93290adc93290adc93290adc000000000000000000000000290000000000000000000000d5f02ca0e96ad039260000000101000000000000000000000000000000000000000000002c0a6d15000000001413105d4f880cf1d9c307ff0000000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000000000000bf0a52570ea085241c8b534000000000000000000300000000000000000000000000164300000000000000000000000000000000000000000200000000000000000000000000000000000000",["after"]="500100000080094400007a4400803b440000964200001643000096430000a04100000000000000000000000000000000000034430000803f00000000000020410000a040000000000e1a4a503e7843e40e1a4a503e7843e400000000000000000000a0400000204104000000d2720c5600000000b5e3ae9d000000000000803f00000000000000000000000000000000000000000000000000000000000000000000000000000000637d9b7800000000000000000000000093290adc93290adc93290adc93290adc000000000000000000000000290000000000000000000000d5f02ca0e96ad039260000000101000000000000000000000000000000000000000000002c0a6d15000000001413105d4f880cf1d9c307ff0000000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000000000000bf0a52570ea085241c8b534000000000000000000300000000000000000000000000164300000000000000000000000000000000000000000200000000000000000000000000000000000000"},["magazine"]={["name"]="WeaponMagazineComponent",["group"]=-25623020,["header"]="a3888dfb4c444c4401000000a3888dfb20cb00000100000000000000",["table_map"]=-25622992,["map"]=-25619424,["owner"]="7f8a7c5797ce92f98e00000000000000",["row"]=-25591632,["before"]="00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000310000000600000007000000070000000000000000000000",["after"]="00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000960000000600000007000000070000000000000000000000"}}
local PROFILE_SPECS={["projectile"]={["name"]="ProjectileSettings",["slot"]=55108344,["group"]=4,["groups"]=1,["kind"]=3175105218,["size"]=95216,["count"]=350,["stride"]=272,["records"]={{["id"]=333,["index"]=217,["before"]="4d01000027a014db78e66a3a6952cdf1f5c84b7d5898656a000000410100000000004844000060419a99993e0000803f010000000000000000000000780000000000803e0000000092fa8c7d5ee90622cbd59ebe71f73f3f0ad7233c00000000000000000000000000000000000000004a2c7593b65104be000000000000000000000000000000000000a042010000000000000000000000000000000000000000000000cdcccc3d0300000003000000010000000000a642000070420000403f000000400000404163e348f78790e63a0000000000000000000080bf00000000000000000000000002000000000000007d0000000000000018953d390000000000000000000000000000000000000000",["after"]="4d010000ad3b87f88bde7ff36952cdf1f5c84b7d5898656a00001041010000000000a0430000e0409a99193f0000803f0100000000000000000000001c0200000000803e000000009c7b34f4ac78e462cbd59ebe71f73f3f0ad7233c000000000000000000000000000000000000000000000000000000000000000000000000767e7a9fdfb2adc60000a042010000000000000000000000000000000000000000000000cdcccc3d0300000003000000010000000000a642000070420000403f000000000000000050af124d8790e63a0000000000000000000080bf00000000000000000000000001000000000000007d0000000000000018953d390000000000000000000000000000000000000000"},{["id"]=335,["index"]=115,["before"]="4f01000000000000000000006952cdf1f5c84b7d5898656a00002041010000000000803f000020410000000000000000010000000000803f00000000400000000000803e00000000000000000000000000000000000000009565883d00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000a0420100000000000000000000000000000000000000000000000000003f1e00000007000000010000000000a642000070420000403f00000000000000004b49ecc48790e63a6de0063ce1c6f889000080bf00000000000000000000000001000000000000003d0000000000000018953d390000000000000000000000000000000000000000",["after"]="4f010000ad3b87f88bde7ff36952cdf1f5c84b7d5898656a00001041010000000000a0430000e0409a99193f0000803f0100000000000000000000001d0200000000803e000000009c7b34f4ac78e462cbd59ebe71f73f3f0ad7233c000000000000000000000000000000000000000000000000000000000000000000000000767e7a9fdfb2adc60000a042010000000000000000000000000000000000000000000000cdcccc3d0300000003000000010000000000a642000070420000403f000000000000000050af124d8790e63a0000000000000000000080bf00000000000000000000000001000000000000007d0000000000000018953d390000000000000000000000000000000000000000"},{["id"]=336,["index"]=124,["before"]="50010000211c8fa5bab01ca36952cdf1f5c84b7d5898656a000084420100000000008c420000af439a99993e0000803f010000000000704100000000f70000000000803e00000000b2acadb00a2417d500000000000000000ad7233c00000000a380ca4d15a65c5500000000000000000000000000000000000000000000000000000000000000000000a042010000007300000000002041000000000000000000000000cdcccc3d1100000004000000010000000000a642000070420000403f000000000000000000000000b138e1ccc28001b30d2996a0000080bf0000000000000000000000000400000000000000a70000000000000018953d390000000000000000000000000000000000000000",["after"]="50010000ad3b87f88bde7ff36952cdf1f5c84b7d5898656a00001041010000000000a0430000e0409a99193f0000803f0100000000000000000000001e0200000000803e000000009c7b34f4ac78e462cbd59ebe71f73f3f0ad7233c000000000000000000000000000000000000000000000000000000000000000000000000767e7a9fdfb2adc60000a042010000000000000000000000000000000000000000000000cdcccc3d0300000003000000010000000000a642000070420000403f000000000000000050af124d8790e63a0000000000000000000080bf00000000000000000000000001000000000000007d0000000000000018953d390000000000000000000000000000000000000000"},{["id"]=337,["index"]=166,["before"]="510100000df73db3a10665236952cdf1f5c84b7d5898656a00001041010000000000a0430000e0409a99993f0000803f0100000000000000000000004b0000000000803e000000009c7b34f4ac78e462cbd59ebe71f73f3f0ad7233c000000000000000000000000000000000000000000000000000000000000000000000000767e7a9fdfb2adc60000a042010000000000000000000000000000000000000000000000cdcccc3d0300000003000000010000000000a642000070420000403f00000000000000002dfde1ac8790e63a0000000000000000000080bf00000000000000000000000001000000000000007d0000000000000018953d390000000000000000000000000000000000000000",["after"]="51010000ad3b87f88bde7ff36952cdf1f5c84b7d5898656a00001041010000000000a0430000e0409a99193f0000803f0100000000000000000000001f0200000000803e000000009c7b34f4ac78e462cbd59ebe71f73f3f0ad7233c000000000000000000000000000000000000000000000000000000000000000000000000767e7a9fdfb2adc60000a042010000000000000000000000000000000000000000000000cdcccc3d0300000003000000010000000000a642000070420000403f000000000000000050af124d8790e63a0000000000000000000080bf00000000000000000000000001000000000000007d0000000000000018953d390000000000000000000000000000000000000000"},{["id"]=338,["index"]=3,["before"]="520100008d8f96a589762e8a6952cdf1f5c84b7d5898656a0000c841010000000000fa430000c8420000003f0000803f010000000000000000000000ea0000000000803e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000036b33606802fe5710000b44201000000a501000000000000000000000000000000000000cdcccc3d0000000000000000010000000000b442000070420000403f0000a0400000a041000000000000000012398fa100000000000080bf0000000000000000000000000300000000000000bc0000000000000018953d390000000000000000000000000000000000000000",["after"]="52010000ad3b87f88bde7ff36952cdf1f5c84b7d5898656a00001041010000000000a0430000e0409a99193f0000803f010000000000000000000000200200000000803e000000009c7b34f4ac78e462cbd59ebe71f73f3f0ad7233c000000000000000000000000000000000000000000000000000000000000000000000000767e7a9fdfb2adc60000a042010000000000000000000000000000000000000000000000cdcccc3d0300000003000000010000000000a642000070420000403f000000000000000050af124d8790e63a0000000000000000000080bf00000000000000000000000001000000000000007d0000000000000018953d390000000000000000000000000000000000000000"}}},["damage"]={["name"]="DamageSettings",["slot"]=55108088,["group"]=60,["groups"]=2,["kind"]=3769052400,["size"]=49340,["count"]=649,["stride"]=76,["records"]={{["id"]=540,["index"]=535,["before"]="1c0200000a0000000a00000009000000090000000900000009000000000000000000000000000000040000000000000000000000000000000000000000000000000000000000000000000000",["after"]="1c0200006e0000001d000000030000000300000003000000000000000a0000000a00000004000000000000000000000000000000000000000000000000000000000000000000000000000000"},{["id"]=541,["index"]=536,["before"]="1d020000050000000500000009000000090000000900000009000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",["after"]="1d0200005a00000018000000030000000300000003000000000000000a0000000a00000004000000000000000000000000000000000000000000000000000000000000000000000000000000"},{["id"]=542,["index"]=537,["before"]="1e020000140000001400000009000000090000000900000009000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000000000000",["after"]="1e0200004b00000014000000030000000300000003000000000000000a0000000a00000004000000000000000000000000000000000000000000000000000000000000000000000000000000"},{["id"]=543,["index"]=538,["before"]="1f020000280000002800000009000000090000000900000009000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000000000000",["after"]="1f020000280000000b000000030000000300000003000000000000000a0000000a00000004000000000000000000000000000000000000000000000000000000000000000000000000000000"},{["id"]=544,["index"]=539,["before"]="20020000640000006400000009000000090000000900000009000000000000000000000000000000010000000500000000002041000000000000000000000000000000000000000000000000",["after"]="200200001c00000008000000040000000400000004000000000000000a0000000a00000004000000000000000000000000000000000000000000000000000000000000000000000000000000"}}}}
start(make_api,COMPONENTS,mode_profile,native,component_profile,compatibility,transaction_profile,COMPONENTS,PROFILE_SPECS)
