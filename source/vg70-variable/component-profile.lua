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
