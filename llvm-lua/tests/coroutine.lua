-- coroutine tests

local f

assert(coroutine.running() == nil)


-- tests for global environment

local function foo (a)
  setfenv(0, a)
	print('yield')
  coroutine.yield(getfenv())
	print('getfenv(0)')
  assert(getfenv(0) == a)
	print('getfenv(1)')
  assert(getfenv(1) == _G)
	print('getfenv(loadstring)')
  assert(getfenv(loadstring"") == a)
	print('getfenv()')
  return getfenv()
end

f = coroutine.wrap(foo)
local a = {print = print, tostring=tostring}
assert(f(a) == _G)
local a,b = pcall(f)
print(a,b)
assert(a and b == _G)

