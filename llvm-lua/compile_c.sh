#!/bin/sh
#
FILE=${1/.lua/}
DEBUG="1"
if [[ $DEBUG == "1" ]]; then
	LUA_FLAGS=" -O0 "
	#LUA_FLAGS=" -O3 -do-not-inline-opcodes -dump-functions "
	CFLAGS=" -O0 "
	OPT_FLAGS=" -disable-opt "
	LLC_FLAGS=" "
else
	LUA_FLAGS=" -O3 "
	CFLAGS=" -O3 -fomit-frame-pointer -pipe -Wall "
	#CFLAGS=" -O3 -fomit-frame-pointer -pipe -Wall "
	OPT_FLAGS=" -O3 -std-compile-opts -tailcallelim -tailduplicate "
	#OPT_FLAGS=" -disable-opt -tailcallelim "
	LLC_FLAGS=" -tailcallopt "
fi

./llvm-luac -O3 -bc -o ${FILE}.bc ${FILE}.lua && \
	llc $LLC_FLAGS -f -march=c -o ${FILE}.c ${FILE}.bc

if [[ $DEBUG == "0" ]]; then
	rm -f ${FILE}.bc
	#mv ${FILE}_opt.bc ${FILE}_run.bc
fi

