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
