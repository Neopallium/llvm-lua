local width = 100
local xb = 50
local a = 0

for x=0, xb < width and 10 or 20 do
	a = a + 1
end

for x=xb < width and 10 or 20,0,-1 do
	a = a + 1
end

print("a=", a)

