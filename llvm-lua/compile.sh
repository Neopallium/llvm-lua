#!/bin/sh
#
FILE=${1/.lua/}

./llvm-luac -bc -o ${FILE}.bc ${FILE}.lua && \
	llvm-link -f -o ${FILE}_run.bc liblua_main.bc ${FILE}.bc && \
	llc -f -filetype=asm -o ${FILE}_run.s ${FILE}_run.bc && \
	g++ -ggdb -o ${FILE} ${FILE}_run.s -lm -ldl

rm -f ${FILE}.bc ${FILE}.ll ${FILE}_run.bc ${FILE}_run.s

