function tail_cfunc(...)
	local s = string.format(...)
	return print(s)
end

print(tail_cfunc("this is a test: %s %s %s %s","1","2","3","4"))

