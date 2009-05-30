local function test()
	local v="test"
	local function test2()
		print(v)
	end
	test2()
	return
end

local d=string.dump(test)
io.write(d)
test()
