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
