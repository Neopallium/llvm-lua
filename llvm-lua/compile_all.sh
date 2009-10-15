#!/usr/bin/env bash
#

OPTS=""
FILES=""
# parse command line parameters.
for arg in "$@" ; do
	case "$arg" in
	-*) OPTS="$OPTS $arg" ;;
	*) FILES="$FILES $arg" ;;
	esac
done

for script in $FILES; do
	echo "Compiling script: $script"
	lua-compiler $OPTS $script
	#./lua-compiler $OPTS $script
done

