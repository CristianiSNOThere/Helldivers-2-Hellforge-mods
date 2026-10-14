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
