local x = 0
local x2 = 0
for a=1,25 do
	for b=1,25 do
		for c=1,25 do
			for d=1,25 do
				for e=1,25 do
					for f=1,25 do
--io.write(a,",",b,",",c,",",d,",",e,",",f,",",x,"\n")
						x = x + 1
						x2 = x2 + a + b + c + d + e + f
					end
				end
			end
		end
	end
end
--print(a,",",b,",",c,",",d,",",e,",",f,",",x,"\n")
io.write(x,"\n")
io.write(x2,"\n")
