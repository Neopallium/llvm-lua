local function tail(depth)
	if(depth > 0) then
		return tail(depth - 1)
	end
	return 0
end

local N = tonumber(arg and arg[1]) or 1
print(tail(N))

local function tail_math()
  -- tail call
	return math.sin("a")
end

tail_math()

