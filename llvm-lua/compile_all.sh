#!/bin/sh
#

for script in $*; do
	echo "Compiling script: $script"
	./compile.sh $script
done

