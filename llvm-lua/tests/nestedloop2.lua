local x = 0
for a=25,1,-1 do
	for b=25,1,-1 do
		for c=25,1,-1 do
			for d=25,1,-1 do
				for e=25,1,-1 do
					for f=25,1,-1 do
--io.write(a,",",b,",",c,",",d,",",e,",",f,",",x,"\n")
						x = x + 1
					end
				end
			end
		end
	end
end
print(a,",",b,",",c,",",d,",",e,",",f,",",x,"\n")
io.write(x,"\n")
