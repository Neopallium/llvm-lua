#!/bin/sh
#
FILE=${1/.lua/}
DEBUG="0"
if [[ $DEBUG == "1" ]]; then
	#CFLAGS=" -O0 -ggdb "
	CFLAGS=" -ggdb -O3 -foptimize-sibling-calls -fomit-frame-pointer -pipe -Wall "
	LLC_FLAGS=" "
else
	#CFLAGS=" -ggdb -march=athlon64 -O3 -fomit-frame-pointer -pipe -Wall "
	CFLAGS=" -march=athlon64 -O3 -foptimize-sibling-calls -fomit-frame-pointer -pipe -Wall "
	LLC_FLAGS=" -tailcallopt "
fi

./llvm-luac -bc -o ${FILE}.bc ${FILE}.lua && \
	llc $LLC_FLAGS --march=c -f -o ${FILE}_mod.c ${FILE}.bc && \
	gcc $CFLAGS -o ${FILE} ${FILE}_mod.c liblua_main.a -lm -ldl

if [[ $DEBUG == "0" ]]; then
	rm -f ${FILE}.bc ${FILE}_mod.c
fi

