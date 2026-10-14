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
