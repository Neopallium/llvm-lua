local n = tonumber((arg and arg[1]) or 1)

local hash1={}
for i=0,10000 do
    hash1["foo_"..i] = i
end
local hash2={}
for i=1,n do
    for k,v in pairs(hash1) do
	hash2[k] = v + (hash2[k] or 0)
    end
end

print(hash1["foo_1"], hash1["foo_9999"],
	     hash2["foo_1"], hash2["foo_9999"])
--io.write(string.format("%d %d %d %d\n", hash1["foo_1"], hash1["foo_9999"],
--	     hash2["foo_1"], hash2["foo_9999"]))
