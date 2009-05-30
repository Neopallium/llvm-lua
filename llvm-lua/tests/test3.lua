debug.sethook(function(e,l,x) print(e,l,x) end, "l")

-- very inefficient fibonacci function
function fib(n)
	N=N+1
	if n<2 then
		return n
	else
		return fib(n-1)+fib(n-2)
	end
end

-- run and time it
function test(s,f)
	N=0
	local c=os.clock()
	local v=f(n)
	local t=os.clock()-c
	print(s,n,v,t,N)
end

n=arg[1] or 24		-- for other values, do lua fib.lua XX
n=tonumber(n)
print("","n","value","time","evals")
test("plain",fib)

