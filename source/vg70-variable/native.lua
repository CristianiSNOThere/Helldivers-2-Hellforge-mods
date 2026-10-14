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
