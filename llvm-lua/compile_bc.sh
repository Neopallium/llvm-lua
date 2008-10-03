#!/bin/sh
#
FILE=${1/.lua/}
DEBUG="0"
if [[ $DEBUG == "1" ]]; then
	CFLAGS=" -O0 -ggdb "
	OPT_FLAGS=" -disable-opt "
	LLC_FLAGS=" "
else
	CFLAGS=" -march=athlon64 -O3 -fomit-frame-pointer -pipe -Wall "
	OPT_FLAGS=" -O3 -std-compile-opts -tailcallelim -tailduplicate "
	#CFLAGS=" -march=athlon64 -O2 -pipe -Wall "
	#OPT_FLAGS=" -disable-opt -tailcallelim "
	LLC_FLAGS=" -tailcallopt "
fi

./llvm-luac -bc -o ${FILE}.bc ${FILE}.lua && \
	llvm-link -f -o ${FILE}_run.bc liblua_main.bc ${FILE}.bc && \
	opt $OPT_FLAGS -f -o ${FILE}_opt.bc ${FILE}_run.bc && \
	llc $LLC_FLAGS -f -filetype=asm -o ${FILE}_run.s ${FILE}_opt.bc && \
	g++ $CFLAGS -o ${FILE} ${FILE}_run.s -lm -ldl

rm -f ${FILE}_opt.bc ${FILE}_run.bc
rm -f ${FILE}.bc ${FILE}_run.s
#mv ${FILE}_opt.bc ${FILE}_run.bc

