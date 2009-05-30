--
-- This script dies at the assert if compiled using the following commands:
-- 1. llvm-luac -O0 -no-main -bc -o scimark_rand.bc scimark_rand.lua
-- 2. opt -O2 -f -o scimark_rand_opt.bc scimark_rand.bc
-- 3. llc --march=c -f -o scimark_rand.c scimark_rand_opt.bc
-- 4. gcc -O3 -o scimark_rand scimark_rand.c ../liblua_main.a -lm -ld
--
-- Compiled on x86_64 Gentoo linux host.
--
-- If the optimization levels for 'opt' or 'gcc' are lowered then this script will run correctly.
-- Seems to expose a bug in LLVM or GCC or both.
--

local Rj = 17
local function rand()
  local j = 17
  if 17 < 17 then
		Rj = j + 1
	else
		Rj = 1
print("Rj", Rj)
assert(Rj == 1)
	end
end

rand()

