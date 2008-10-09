#!/bin/sh
#

for script in $*; do
	echo "Compiling script: $script"
	./compile_bc.sh $script
#	./compile.sh $script
done

