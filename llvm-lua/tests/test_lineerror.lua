-- testing line error

function lineerror (s)
  local err,msg = pcall(loadstring(s))
  local line = string.match(msg, ":(%d+):")
  print(err,msg,line,X)
  return line and line+0
end

assert(lineerror"local a\n for i=1,'a' do \n print(i) \n end" == 2)
assert(lineerror"\n local a \n for k,v in 3 \n do \n print(k) \n end" == 3)
assert(lineerror"\n\n for k,v in \n 3 \n do \n print(k) \n end" == 4)
assert(lineerror"function a.x.y ()\na=a+1\nend" == 1)

local p = [[
function g() f() end
function f(x) error('a', X) end
g()
]]
X=3;assert(lineerror(p) == 3)
X=0;assert(lineerror(p) == nil)
X=1;assert(lineerror(p) == 2)
X=2;assert(lineerror(p) == 1)

