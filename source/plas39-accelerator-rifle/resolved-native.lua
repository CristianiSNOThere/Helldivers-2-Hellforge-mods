local native = {}
local OWNER = '5e7f47af911f0630'
local GUARDS = {{rva=5281136,bytes='4885c97471488b051c2af602448bc14c8b90d82bf10048b8379f711660f3196748f7e1488bc1482bc248d1e84803c248'},{rva=5262720,bytes='4885c9746c488b050c72f602448bc14c8b90d82af10048b8cdcccccccccccccc48f7e148c1ea048d0492c1e002442bc0'},{rva=7694280,bytes='8bc8498b4260488d14498b049048'},{rva=7592314,bytes='498bcde83e79dcff488bf88b88bc00000083f901762e418b44ee143bc173208b8fc000000041894cee188d480141894cee14eb103bc87582438b44c304eb80418974ee14418b4508'},{rva=7695008,bytes='443ba88c0000000f92c04883c420415e415d'},{rva=5327888,bytes='4885c97471488b057c73f502448bc14c8b90802ef10048b8fb99d3e0ce8bd4f148f7e148c1ea0969c21e020000442bc0'},{rva=5329152,bytes='4883ec284c8bd14885c9750733c04883c428c38b41083b0508ebf6020f84a30000004c8b1daf15e1024533c048895c24'},{rva=6376742,bytes='8b45a80f57ffffc8f20f5af8a9fdffffff0f84c0000000'},{rva=6376525,bytes='4180b8ed000000000f84f5030000'},{rva=7588168,bytes='3953100f862c0f00004889b4'},{rva=7597496,bytes='488b05f975be02ba001b0000488b0dad75be0241b808000000c74304200000004c8b485841ffd18b4b0433d24c69c1d8000000488bc848898390000000e8a6bc9401'},{rva=7599818,bytes='8bab880000004c8d7350ffcd4c897c2458498bce89ab88000000e8b701ff004c8d7b708bd5498bcfe8a901ff003bee0f8439'},{rva=6395126,bytes='488b05bbced002ba00340100488b0d6fced00241b808000000c74330800000004c8b485841ffd18b4b3033d24c69c168020000488bc8488983d0'},{rva=6398105,bytes='8babc80000004c89742450ffcd4c897c24584c8dbb90000000498bcf89abc8000000e8e05711018bd5488d8bb0000000e8d25711013bef0f84620100004c'},{rva=5264064,bytes='4883ec284c8bd14885c9750733c04883c428c38b41083b0548e9f7020f84930000004c8b1d3719e2024533c048895c243048896c24384889742440458b4b58418b5b600fafd848897c2420418d69ff4585c9742c498b7b50418b735c0f1f40008bcd418d14184823d1488d0cd78b14d73bd6740e3bd0740a41ffc0453bc172e033c9488b7c2420488b742440488b6c2438488b5c24303901751b8b410483f8ff74134869c0d8000000490383900000004883c428c3498b0a4883c428e9'},{rva=24311696,bytes='48895c2408488974241048897c2418448b510833c0448b5910418bf0440fafda448bca418d7aff4585d274274c8b018b590c8bcf428d14184823d1498d0cd0418b14d03bd3740e413bd17409ffc0413bc272df33c9488b5c2408488bc1488b7c2418897104488b742410448909c3'},{rva=24311968,bytes='48895c2418488974242057448b59088bda0faf59104533c0448bd2418d73ff4585db7427488b39660f1f840000000000418d14188bc64823d0443914d74c8d0cd7741641ffc0453bc372e533c0488b5c2420488b7424285fc38b410c418901440faf51108b510848896c2410418b69044c897424188d72ff478d340241ba01000000418bfe23fe413bd2765f0f1f40004c8b19478d0c164423ce4a8d1ccd00000000468b041b443b410c743f440faf41108bc22bd74103d123d64423c6412bc04103c123c63bd077174a8b041b8bd7418bf9498904d34c8b018b510c428914038b510841ffc2443bd272a54c8b7424188bc5488b6c2410488b5c2420488b7424285fc3'},{rva=6364083,bytes='498b6f78498bce8bc34869f8a800000048894424304803ef4889bc24d000000048896c2440'},{rva=6364236,bytes='f3410f594d08488b05af46d102f30f1005df5adb014489651cf30f5ec1f30f11450c'},{rva=6368504,bytes='488b442478f30f10400cf30f584008f30f114008'},{rva=7589458,bytes='438b44c30483f8ff741a4869d0a8000000488b056e98be02488b4878440f2f6c0a08720d488b4c2458488bd6e83d0a0000'},{rva=37514608,bytes='0000803f'},{rva=37514444,bytes='6666663f'},{rva=37517120,bytes='00007042'}}
local function unhex(s) return (s:gsub('..', function(p) return string.char(tonumber(p,16)) end)) end
OWNER = unhex(OWNER)
local function u32(s,o)
    assert(s and o >= 0 and o+4 <= #s, 'Native field unreadable')
    local a,b,c,d=s:byte(o+1,o+4)
    return a+b*256+c*65536+d*16777216
end
local function read(api,p,n) return assert(api.read(p,n), 'Native data unavailable') end
local function pointer(api,s,o) return assert(api.pointer(s,o), 'Native pointer unavailable') end
local function lookup(api,h,o,key)
    local cap,empty,mult=u32(h,o+8),u32(h,o+12),u32(h,o+16)
    assert(cap <= 4096 and (cap == 0 or bit.band(cap,cap-1) == 0), 'Native map capacity changed')
    if cap == 0 then return nil end
    local array=pointer(api,h,o)
    -- Split multiplication to preserve exact 32-bit wraparound in Lua numbers.
    local product=((key%65536)*mult + math.floor(key/65536)*(mult%65536)*65536)%4294967296
    for probe=0,math.min(cap,64)-1 do
        local row=read(api,array+bit.band(product+probe,cap-1)*8,8)
        local identity,index=u32(row,0),u32(row,4)
        if identity == key then return index ~= 4294967295 and index or nil end
        if identity == empty then return nil end
    end
    error('Native map probe bound reached')
end

function native.prepare(api)
    local game=api.byte_pointer(api.module('game.dll'))
    for _,g in ipairs(GUARDS) do
        local bytes=unhex(g.bytes)
        assert(read(api,game+g.rva,#bytes)==bytes,'Unsupported native weapon code; no edit applied')
    end
    local owner=pointer(api,read(api,game+0x346bf98,8),0)
    local weapon_map=pointer(api,read(api,owner+0xf12bd8,8),0)
    local charge_map=pointer(api,read(api,owner+0xf12ad8,8),0)
    local projectile_map=pointer(api,read(api,owner+0xf12e80,8),0)
    local mapping=read(api,projectile_map+290*16,16)
    assert(mapping:sub(1,8)==OWNER and u32(mapping,8)==136,'Projectile resource owner/index changed')
    return { game=game, owner=owner, weapon_group=weapon_map-28, charge_group=charge_map-28,
        projectile_map=projectile_map, projectile_address=projectile_map+542*16+136*616 }
end

function native.instances(api,plan)
    assert(api.pointer(read(api,plan.native.game+0x346bf98,8),0)==plan.native.owner,
        'Native entity tables changed; restart required')
    assert(api.pointer(read(api,plan.native.owner+0xf12bd8,8),0)==plan.native.weapon_group+28
        and api.pointer(read(api,plan.native.owner+0xf12ad8,8),0)==plan.native.charge_group+28,
        'Native component tables changed; restart required')
    local manager=pointer(api,read(api,plan.native.game+0x3326ce0,8),0)
    local h=read(api,manager,192)
    local count=u32(h,0x1c)
    assert(count <= 512 and count <= u32(h,0x10), 'Native weapon instance count changed')
    local array=pointer(api,h,0x48)
    local modes=pointer(api,h,0x60)
    local charge_manager=pointer(api,read(api,plan.native.game+0x3326c20,8),0)
    local ch=read(api,charge_manager,160)
    local projectile_manager=pointer(api,read(api,plan.native.game+0x33266d8,8),0)
    local ph=read(api,projectile_manager,224)
    assert(pointer(api,read(api,plan.native.owner+0xf12e80,8),0)==plan.native.projectile_map,
        'Native projectile table changed')
    local owned=u32(ch,0x10)
    assert(owned<=u32(ch,0xc) and u32(ch,0xc)<=512, 'Native owned charge count changed')
    local references=owned>0 and read(api,pointer(api,ch,0x38),owned*8) or ''
    local result={}
    for slot=0,owned-1 do
        local entity=pointer(api,references,slot*8)
        local identity=read(api,entity,24)
        if identity:sub(1,8)==OWNER and bit.band(u32(identity,20),1)==1 then
            local id=u32(identity,8)
            local i=lookup(api,h,0x30,id)
            assert(i and i<count,'Native weapon slot changed')
            assert(read(api,array+i*8,8)==references:sub(slot*8+1,slot*8+8),'Native firing owner changed')
            local mode_address=modes+i*12
            local mode=u32(read(api,mode_address,4),0)
            if mode==2 or mode==3 then
                local weapon=plan.weapon_address
                local wi=lookup(api,h,0x70,id)
                if wi then
                    assert(wi < u32(h,0xa8),'Native weapon settings index changed')
                    weapon=pointer(api,h,0xb0)+wi*1232
                end
                local charge=plan.charge_address
                local ci=lookup(api,ch,0x50,id)
                if ci then
                    assert(ci < u32(ch,0x88),'Native charge settings index changed')
                    charge=pointer(api,ch,0x90)+ci*216
                end
                local projectile=plan.projectile_address
                local pi=lookup(api,ph,0x90,id)
                if pi then
                    assert(pi < u32(ph,0xc8) and pi<512,'Native projectile settings index changed')
                    projectile=pointer(api,ph,0xd0)+pi*616
                end
                local charge_state
                local psi=lookup(api,ph,0x50,id)
                assert(psi and psi<u32(ph,0x38) and u32(ph,0x38)<=512,
                    'Native projectile state index changed')
                assert(read(api,pointer(api,ph,0x68)+psi*8,8)==references:sub(slot*8+1,slot*8+8),
                    'Native projectile state owner changed')
                local projectile_state=pointer(api,ph,0x78)+psi*168
                local si=lookup(api,ch,0x20,id)
                if si then
                    assert(si == slot,'Native charge state index changed')
                    assert(read(api,pointer(api,ch,0x38)+si*8,8)==references:sub(slot*8+1,slot*8+8),
                        'Native charge instance owner changed')
                    charge_state=pointer(api,ch,0x40)+si*40
                end
                -- Only the owned charge prefix is advanced by the native firing loop.
                -- Dropped/preview WeaponData instances must never select shared firing settings.
                if si and si<u32(ch,0x10) then
                result[#result+1]={ id=id, mode=mode, mode_address=mode_address,
                    entity=entity, identity=identity, weapon=weapon, charge=charge,
                    charge_state=charge_state, projectile=projectile, projectile_state=projectile_state,
                    charge_cached=ci~=nil, projectile_cached=pi~=nil }
                end
            end
        end
    end
    -- A game update may swap arrays; reject stale snapshots before any writes.
    assert(read(api,manager+0x48,8)==h:sub(0x49,0x50)
        and read(api,manager+0x60,8)==h:sub(0x61,0x68),'Native weapon arrays changed')
    plan.instance_snapshot={data_manager=manager,data_header=h,charge_manager=charge_manager,
        charge_header=ch,projectile_manager=projectile_manager,projectile_header=ph}
    return result
end


local function word(v)
    return string.char(v%256,math.floor(v/256)%256,math.floor(v/65536)%256,math.floor(v/16777216)%256)
end

-- Use the engine's already allocated override rows and both index maps.
-- No new allocation, source-table edit, native call or code patch is required.
-- Native destructors remove these entries and compact the rows on entity removal.
function native.isolate(api,plan,instances,edits,seen)
    local snapshot=assert(plan.instance_snapshot,'Native instance snapshot unavailable')
    local function stage(address,after,clone)
        local key=api.identity(address)
        assert(not seen[key],'Override reservation overlaps another edit')
        local entry={address=address,before=read(api,address,4),after=after,clone=clone}
        seen[key]=entry
        edits[#edits+1]=entry
    end
    local function reserve(group,key,index,reserved)
        local cap,empty,mult=u32(group.header,group.offset+8),u32(group.header,group.offset+12),u32(group.header,group.offset+16)
        assert(cap>0 and cap<=4096 and bit.band(cap,cap-1)==0,'Override map capacity changed')
        local array=pointer(api,group.header,group.offset)
        local product=((key%65536)*mult+math.floor(key/65536)*(mult%65536)*65536)%4294967296
        local tombstone
        for probe=0,math.min(cap,64)-1 do
            local address=array+bit.band(product+probe,cap-1)*8
            local identity=api.identity(address)
            local row=reserved[identity] or read(api,address,8)
            if u32(row,0)==key then
                assert(u32(row,4)==4294967295,'Override key already registered')
            end
            if u32(row,4)==4294967295 and not tombstone then tombstone=address end
            if u32(row,0)==empty or u32(row,0)==key then
                local target=tombstone or address
                reserved[api.identity(target)]=word(key)..word(index)
                return target
            end
        end
        if cap<=64 and tombstone then
            reserved[api.identity(tombstone)]=word(key)..word(index)
            return tombstone
        end
        error('No bounded override map slot available; no edit applied')
    end
    local function isolate(manager,header,capacity_offset,count_offset,forward_offset,reverse_offset,array_offset,stride,field,cached)
        local count,capacity=u32(header,count_offset),u32(header,capacity_offset)
        assert(count<=capacity and capacity<=512,'Override row capacity changed')
        local rows=pointer(api,header,array_offset)
        local next_index=count
        local reserved,publications={},{}
        for _,item in ipairs(instances) do
            if not item[cached] then
                assert(next_index<capacity,'Preallocated override rows exhausted; no edit applied')
                local forward=reserve({header=header,offset=forward_offset},item.id,next_index,reserved)
                local reverse=reserve({header=header,offset=reverse_offset},next_index,item.id,reserved)
                local source=read(api,item[field],stride)
                local target=rows+next_index*stride
                assert(api.data_access(target,stride),'Override row is not writable private data')
                for offset=0,stride-4,4 do stage(target+offset,source:sub(offset+1,offset+4),true) end
                -- Reverse map and count precede the forward key publication.
                stage(reverse+4,word(item.id));stage(reverse,word(next_index))
                publications[#publications+1]={address=forward,index=next_index,id=item.id}
                item[field]=target
                item[field..'_row']=source
                next_index=next_index+1
            end
        end
        if next_index~=count then
            stage(manager+count_offset,word(next_index))
            for _,entry in ipairs(publications) do
                stage(entry.address+4,word(entry.index));stage(entry.address,word(entry.id))
            end
        end
    end
    isolate(snapshot.charge_manager,snapshot.charge_header,4,0x88,0x50,0x70,0x90,216,'charge','charge_cached')
    isolate(snapshot.projectile_manager,snapshot.projectile_header,0x30,0xc8,0x90,0xb0,0xd0,616,'projectile','projectile_cached')
end

function native.validate(api,plan,instances)
    local s=assert(plan.instance_snapshot)
    assert(read(api,s.data_manager,192)==s.data_header
        and read(api,s.charge_manager,160)==s.charge_header
        and read(api,s.projectile_manager,224)==s.projectile_header,
        'Native manager changed before override publication; no edit applied')
    for _,item in ipairs(instances) do
        assert(read(api,item.entity,24)==item.identity
            and u32(read(api,item.mode_address,4),0)==item.mode,'Native weapon changed before override publication')
    end
end

return native
