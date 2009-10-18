#!/bin/sh
#

for script in `ls tests/*.lua`; do
	echo "run test: $script"
	llvm-lua -g -O0 $script >/dev/null
	#llvm-lua -g -O0 $script
	#lua $script
done

