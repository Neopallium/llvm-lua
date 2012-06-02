
local patch = arg[1]
if not patch then
	print("run:", arg[0] .. "<hg_patch_set>")
	return
end

local prefix, ext = patch:match("^(.*)%.(.*)$")

print("split HG patch set:", prefix .. '.' .. ext)

local input = io.open(patch, "rb")

local part = 0
local output

local function append_line(line)
	if output then
		output:write(line, '\n')
	end
end

local function open_next_output()
	if output then
		-- close previous patch.
		output:close()
		output = nil
	end
	part = part + 1
	local name = string.format("%s_%d.%s", prefix, part, ext)
	print("Start patch file:", name)
	output = io.open(name, "w+b")
end

for line in input:lines() do
	if line == "# HG changeset patch" then
		open_next_output()
	end
	append_line(line)
end

if output then
	output:close()
end

