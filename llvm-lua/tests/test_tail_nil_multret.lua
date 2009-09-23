do
	local f = function(n)
		local x = {}
		for i=1,n do
			x[i] = i
		end
		return unpack(x)
	end

	local a,b,c
	a,b,c = 0,5,f(0)
	print("a,b,c", a, b, c)
	assert(a==0 and b==5 and c==nil)
	a={f(0)}
	print("#a = ", #a)
	print("a = ", unpack(a))
end
