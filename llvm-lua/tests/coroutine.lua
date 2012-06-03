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
	----------------------------------------------------------
	-- NOTE: The call to 'loadstring' from a coroutine caused
	-- a stack overflow when the LLVM JIT tried to codegen
	-- the Lua code (in this case an empty function).
	-- 
	-- For now we will disable the JIT compiler from running
	-- on a coroutine.
	-- TODO: Switch to a separate large cstack when needed.
	----------------------------------------------------------
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

