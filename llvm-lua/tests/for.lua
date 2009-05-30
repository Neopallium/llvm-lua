local n = tonumber((arg and arg[1]) or 1)
local hash1={}
for i=0,10000 do
    hash1["foo_"..i] = i
    hash1[i] = i
end
for i=0,10000 do
print(i,hash1[i])
end
print(hash1["foo_1"], hash1["foo_9999"])
