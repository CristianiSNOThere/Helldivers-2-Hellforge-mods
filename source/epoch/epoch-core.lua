local COMPONENTS={['magazine']={['name']='WeaponMagazineComponent',['group']=528664,['header']='a3888dfb4c444c4401000000a3888dfb20cb00000100000000000000',['map']=534708,['owner']='540e78d79af4d5e80001000000000000',['row']=578292,['before']='00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000030000000200000002000000040000000000000000000000'},['reload']={['name']='WeaponReloadComponent',['group']=5737840,['header']='4e451d994c444c44010000004e451d99406d00000100000000000000',['map']=5744476,['owner']='540e78d79af4d5e8ed00000000000000',['row']=5764796,['before']='00000000070b00000100000015fe9ff187712f06000000000000000000000000000000000000000000000000000000000000000000000000000000000000000045bf1295b397435ce86f77b500000000'},['charge']={['name']='WeaponChargeComponent',['group']=7565064,['header']='a135c3ea4c444c4401000000a135c3ea880a00000100000000000000',['map']=7565380,['owner']='540e78d79af4d5e80800000000000000',['row']=7567140,['before']='0000803fa50000006f9b41631ad68a8b000000000000000000002040c100000082a78bdf0a700271000000000000000066662640c100000082a78bdf0a70027100000000000000000000803f0000803f0000803f000000400000803f0000803f3333333f3333b33f3333333f33331340cdcc4c3f0000c03fe5863eae3265f1fbe1ba20e995896fc59832a1fb6052f47d009970c900000000a3a7f8284e8b6c5f523886076781f0ff0fd7d61a96fba2290fd7d61a00000000000000000000000000000000f22b8a6d41010000020000000000504000000000'},['fire']={['name']='ProjectileWeaponComponent',['group']=46404840,['header']='681b17454c444c4401000000681b174560b002000100000000000000',['map']=46408868,['owner']='540e78d79af4d5e80201000000000000',['row']=46572468,['before']='a50000000000000000001644000000000000964200001643000096430000a04100000000000000000000000000000000000034430000803f00000000000020410000a040000000006d72edf78b5280a06d72edf78b5280a00000000000000000faff9f4000002041020000000000000000000000b5e3ae9d000000000000803f00000000000000000000000000000000000000000000000000000000000000000000000000000000637d9b7800000000000000000000000093290adc93290adc93290adc93290adc000000000000000000000000290000000000000000000000000000000000000026000000010100000000000000000000000000000000000000000000d1e30000000000000000000000000000c43df0ae0000000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000000000000bf0a5257adb056d3bd268fe200000000000000000300000000000000000000000000164300000000000000000000000000000000000000000200000000000000000000000000000000000000'}}
local PROJECTILE={['name']='PROJECTILE',['slot']=55108344,['group']=4,['groups']=1,['kind']=3175105218,['size']=95216,['count']=350,['stride']=272,['records']={{['id']=165,['index']=317,['before']='a5000000543084e1891a69fd6952cdf1f5c84b7d5898656a0000a0410100000000007a430000c842cdcccc3d00000040010000009a99193f9a99993e3a0000003333b33e00000000a2f99f6e962364380000000000000000b9fc873c000000000000000000000000f4576281c6d09dda92351090ffc124a4000000000000000000000000000000000000b44301000000e30000000000000000000000e30000000000803fcdcccc3d0000000000000000000000000000a642000070420000403f0000404000007041ca30d0fc00000000239bae1558000259000080bf0000000000000000000000000100000000000000bf00000002000000d8e996ab0000000000000000000000000000000000000000',['mask_pointer']=false},{['id']=193,['index']=318,['before']='c1000000543084e1891a69fd6952cdf1f5c84b7d5898656a0000a0410100000000007a430000c842cdcccc3d00000040010000009a99193f9a99993e3b0000003333b33e00000000a2f99f6e962364380000000000000000b9fc873c000000000000000000000000f4576281c6d09dda92351090ffc124a4000000000000000000000000000000000000b44301000000970100000000000000000000970100000000803fcdcccc3d0000000000000000000000000000a642000070420000403f0000404000007041097b710a00000000239bae1558000259000080bf0000000000000000000000000100000000000000bf00000002000000d8e996ab0000000000000000000000000000000000000000',['mask_pointer']=false}}}
local EXPLOSION={['name']='EXPLOSION',['slot']=55110792,['group']=4,['groups']=1,['kind']=719988114,['size']=67800,['count']=422,['stride']=152,['records']={{['id']=227,['index']=103,['before']='e30000003c000000000000000000000033331340000040400000804000000000000090400000000060fe0000000000000100000000000000b96cd01c9c07330f1dd6b742060000001e0000000000000000000000000000000e000000010000000000000000000000000000000000000000000000050000000000000002000000000000000000000000000042030000000000000000000000',['mask_pointer']=true},{['id']=407,['index']=104,['before']='970100003d000000000000000000000000004040000080400000a040000000000000e0400000000068fe0000000000000100000000000000f1ccba90d6ba620a18dc0d39060000001e0000000000000000000000000000000e000000010000000000000000000000000000000000000000000000050000000000000002000000000000000000000000000042030000000000000000000000',['mask_pointer']=true},{['id']=321,['index']=105,['before']='410100003d000000000000000000000000004040000080400000a040000000000000e0400000000070fe0000000000000100000000000000f1ccba90d6ba620ac3b82fcf060000001e0000000000000000000000000000000e000000010000000000000000000000000000000000000000000000050000000000000002000000000000000000000000000042030000000000000000000000',['mask_pointer']=true}}}
local DAMAGE={['name']='DAMAGE',['slot']=55108088,['group']=60,['groups']=2,['kind']=3769052400,['size']=49340,['count']=649,['stride']=76,['records']={{['id']=58,['index']=63,['before']='3a00000090010000c8000000040000000400000004000000040000000a0000001e0000000f000000000000000000000000000000000000000000000000000000000000000000000000000000',['mask_pointer']=false},{['id']=59,['index']=64,['before']='3b00000090010000c8000000050000000500000005000000050000000a0000001e0000000f000000000000000000000000000000000000000000000000000000000000000000000000000000',['mask_pointer']=false},{['id']=60,['index']=65,['before']='3c000000f4010000f4010000040000000400000004000000000000000a0000001400000007000000000000000000000000000000000000000000000000000000000000000000000000000000',['mask_pointer']=false},{['id']=61,['index']=66,['before']='3d0000002003000020030000050000000500000005000000000000001e000000230000001e000000000000000000000000000000000000000000000000000000000000000000000000000000',['mask_pointer']=false},{['id']=368,['index']=357,['before']='700100005e0100005e010000030000000000000000000000000000001e000000230000000500000000000000050000000000c842000000000000000000000000000000000000000000000000',['mask_pointer']=false},{['id']=278,['index']=269,['before']='16010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000',['mask_pointer']=false}}}
local CHANGES={{['target']='magazine',['offset']=136,['before']='03000000',['after']='04000000',['protected']=true},{['target']='magazine',['offset']=148,['before']='04000000',['after']='05000000',['protected']=true},{['target']='reload',['offset']=1,['before']='00',['after']='01',['protected']=true},{['target']='damage_59',['offset']=4,['before']='90010000',['after']='08020000',['protected']=false},{['target']='damage_59',['offset']=8,['before']='c8000000',['after']='04010000',['protected']=false},{['target']='damage_278',['offset']=4,['before']='00000000',['after']='10040000',['protected']=false},{['target']='damage_278',['offset']=8,['before']='00000000',['after']='10040000',['protected']=false},{['target']='damage_278',['offset']=12,['before']='00000000',['after']='05000000',['protected']=false},{['target']='damage_278',['offset']=16,['before']='00000000',['after']='05000000',['protected']=false},{['target']='damage_278',['offset']=20,['before']='00000000',['after']='05000000',['protected']=false},{['target']='damage_278',['offset']=28,['before']='00000000',['after']='1e000000',['protected']=false},{['target']='damage_278',['offset']=32,['before']='00000000',['after']='23000000',['protected']=false},{['target']='damage_278',['offset']=36,['before']='00000000',['after']='1e000000',['protected']=false},{['target']='damage_278',['offset']=44,['before']='00000000',['after']='05000000',['protected']=false},{['target']='damage_278',['offset']=48,['before']='00000000',['after']='0000c842',['protected']=false},{['target']='explosion_407',['offset']=4,['before']='3d000000',['after']='16010000',['protected']=false},{['target']='explosion_407',['offset']=16,['before']='00004040',['after']='0000a040',['protected']=false},{['target']='explosion_407',['offset']=20,['before']='00008040',['after']='0000c040',['protected']=false},{['target']='explosion_407',['offset']=24,['before']='0000a040',['after']='0000e040',['protected']=false}}

local function unhex(s) return (s:gsub('..',function(h) return string.char(tonumber(h,16)) end)) end
local function u32(s,o)
    if not s or o<0 or o+4>#s then return nil end
    local a,b,c,d=s:byte(o+1,o+4)
    return a+b*256+c*65536+d*16777216
end
local function fail(label) error('Epoch validation failed: '..label..'. No edit applied.') end
local patch={}

local function component_row(api,entities,spec)
    local header=api.read(entities+spec.group,28)
    if not header or header~=unhex(spec.header) then fail(spec.name..' group header') end
    local owner=api.read(entities+spec.map,16)
    if not owner or owner~=unhex(spec.owner) then fail(spec.name..' owner map') end
    local ptr=entities+spec.row
    local actual=api.read(ptr,#spec.before/2)
    if not actual or actual~=unhex(spec.before) then fail(spec.name..' exact owner row') end
    return ptr
end

local function settings_rows(api,game,spec)
    local root=api.pointer(api.read(game+spec.slot,8),0)
    if not root or u32(api.read(root,4),0)~=spec.groups then fail(spec.name..' table pointer') end
    local group=root+spec.group
    local head=api.read(group,24)
    if not head or head:sub(1,4)~='LDLD' or u32(head,4)~=1
       or u32(head,8)~=spec.kind or u32(head,12)~=spec.size then
        fail(spec.name..' table header')
    end
    local payload=api.read(group+24,16)
    local rows=payload and api.pointer(payload,0)
    if not rows or u32(payload,8)~=spec.count or api.distance(rows,group+24)~=16 then
        fail(spec.name..' table rows')
    end
    local all=api.read(rows,spec.count*spec.stride)
    if not all then fail(spec.name..' table read') end
    local found,by_id={},{}
    for i=0,spec.count-1 do
        local id=u32(all,i*spec.stride)
        if not id or by_id[id] then fail(spec.name..' duplicate or invalid row') end
        by_id[id]=i
    end
    for _,expect in ipairs(spec.records) do
        local i=by_id[expect.id]
        if not i or i~=expect.index then fail(spec.name..' row '..expect.id..' index') end
        local actual=all:sub(i*spec.stride+1,(i+1)*spec.stride)
        local baseline=unhex(expect.before)
        if expect.mask_pointer then
            actual=actual:sub(1,40)..actual:sub(49)
            baseline=baseline:sub(1,40)..baseline:sub(49)
        end
        if actual~=baseline then fail(spec.name..' row '..expect.id..' exact baseline') end
        found[expect.id]=rows+i*spec.stride
    end
    return found,by_id,all
end

local function consumers(projectiles,explosions)
    local function references(rows,stride,offsets,value)
        local ids={}
        for id,i in pairs(rows.by_id) do
            local row=rows.all:sub(i*stride+1,(i+1)*stride)
            for _,o in ipairs(offsets) do if u32(row,o)==value then ids[#ids+1]=id;break end end
        end
        table.sort(ids);return table.concat(ids,',')
    end
    assert(references(projectiles,272,{144,156},407)=='193','Epoch explosion consumer changed')
    assert(references(projectiles,272,{60},59)=='193','Epoch direct consumer changed')
    assert(references(explosions,152,{4},278)=='','Private blast donor in use')
end

function patch.apply(api,game)
    local owner=api.pointer(api.read(game+0x346bf98,8),0)
    local map=owner and api.pointer(api.read(owner+0xf12bd8,8),0)
    if not map then fail('entity manager pointer') end
    local entities=map-26151684
    local ptrs={}
    for key,spec in pairs(COMPONENTS) do ptrs[key]=component_row(api,entities,spec) end
    local projectile,pi,pa=settings_rows(api,game,PROJECTILE)
    local explosion,ei,ea=settings_rows(api,game,EXPLOSION)
    local damage=settings_rows(api,game,DAMAGE)
    consumers({by_id=pi,all=pa},{by_id=ei,all=ea})
    for id,p in pairs(projectile) do ptrs['projectile_'..id]=p end
    for id,p in pairs(explosion) do ptrs['explosion_'..id]=p end
    for id,p in pairs(damage) do ptrs['damage_'..id]=p end

    local writes={}
    for _,change in ipairs(CHANGES) do
        local base=ptrs[change.target]
        if not base then fail('missing target '..change.target) end
        local at=base+change.offset
        local before,after
        if change.copy_target then
            before=api.read(at,change.size)
            after=api.read(assert(ptrs[change.copy_target])+change.copy_offset,change.size)
            assert(before and after,'Epoch resident pointer unavailable')
        else before,after=unhex(change.before),unhex(change.after) end
        if api.read(at,#before)~=before then fail(change.target..' +'..change.offset) end
        local protected=change.protected
        if protected then
            if not api.data_access(at,#after) then fail(change.target..' is not accessible') end
        elseif not api.writable(at,#after) then fail(change.target..' is not writable') end
        writes[#writes+1]={at=at,before=before,after=after,label=change.target,protected=protected}
    end
    local applied={}
    for _,item in ipairs(writes) do
        applied[#applied+1]=item
        local function write(change,bytes)
            if change.protected then return api.write_protected(change.at,bytes) end
            return api.write(change.at,bytes)
        end
        if not write(item,item.after) or api.read(item.at,#item.after)~=item.after then
            local rollback_ok=true
            for i=#applied,1,-1 do
                local old=applied[i]
                if not write(old,old.before) or api.read(old.at,#old.before)~=old.before then
                    rollback_ok=false
                end
            end
            error('Epoch transaction failed at '..item.label..'; rollback '..(rollback_ok and 'verified' or 'FAILED'))
        end
    end
    return 'Applied: Epoch final-stage 30% damage, 5/6/7 m burning blast; mobile reload, 4 rounds, 5 max spare magazines; '..#writes..' guarded writes.'
end

local function start(make_api)
    if rawget(_G,'EpochChargedIncendiaryV1') then return end
    local state={version='0.1.2-test',active=false,status='Waiting for first game update',diagnostics={}}
    _G.EpochChargedIncendiaryV1=state
    local previous=update
    local callback
    local function log()
        pcall(function()
            local loader=rawget(_G,'CowboyBingusModLoader')
            local file=loader and loader.open_log and loader.open_log('EpochChargedIncendiary.log')
            if file then
                file:write('EpochChargedIncendiary v0.1.2-test\nstatus='..state.status..'\n'..table.concat(state.diagnostics,'\n')..'\n')
                file:close()
            end
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
        state.status='Waiting for game tables';log()
        repeat coroutine.yield() until api.now()>=ready_at
        local peer_deadline=api.now()+120000
        while true do
            local blocker
            for _,peer in ipairs({
                {'FlagDamagePrototypeV1','Flag'},
                {'SaiFocusModV1','SAI'},
                {'AR11ArbitratorModV1','AR-11'},
                {'SterilizerArmorControlV1','Gas Overhaul'},
                {'RapidArcThrowerV1','ARC-3'},
                {'AC8BackpacklessV1','AC-8'},
                {'AR32PacifierLuaV1','Pacifier'},
            }) do
                local state=rawget(_G,peer[1])
                if state and not state.active then blocker=peer[2];break end
            end
            local profiles=rawget(_G,'ArsenalPreset01Runtime')
            if not blocker and profiles and profiles.phase~='ready' then blocker='Arsenal profiles' end
            local scorcher=rawget(_G,'ScorcherARRuntime')
            if not blocker and scorcher and scorcher.phase~='ready' then blocker='Scorcher runtime' end
            if not blocker then break end
            assert(api.now()<peer_deadline,'Companion not ready: '..blocker..'; no edit applied')
            state.status='Waiting for '..blocker;log()
            coroutine.yield()
        end
        state.status='Validating Epoch highest-tier owners, rows and consumers';log()
        return patch.apply(api,game)
    end)
    local function init()
        if coroutine.status(worker)=='dead' then return end
        deadline=os.clock()+0.0007
        local ok,message=coroutine.resume(worker)
        if not ok or coroutine.status(worker)=='dead' then
            state.active,state.status=ok,tostring(message)
            print('[EpochChargedIncendiary] '..state.status)
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

start(make_api)
