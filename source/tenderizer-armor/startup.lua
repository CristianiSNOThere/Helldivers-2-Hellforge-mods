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
