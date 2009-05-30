--[[
local function min_stack()
end
local function min_stack_params(a,b,c,d,e)
end
local function tail_recursive_nil(depth)
	local a,b,c,d,e
	print("should be 5 nil values:",a,b,c,d,e)
	a,b,c,d,e = 1,2,3,4,5
	if(depth == 0) then return a,b,c,d,e end
	return tail_recursive_nil(depth - 1)
end
print(tail_recursive_nil(2))
]]
local function test_nil()
	local a,b,c,d,e
	return a,b,c,d,e
end
print(test_nil())
