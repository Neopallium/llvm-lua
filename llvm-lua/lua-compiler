#!/bin/sh
#

CC=gcc
LIBTOOL="libtool --tag=CC --silent"
RPATH=`pwd`
LLVM_LUAC="./llvm-luac"
if [[ ! -x "$LLVM_LUAC" ]]; then
	LLVM_LUAC=`which llvm-luac`
fi

#ARCH=i686
#ARCH=pentium4
ARCH=athlon64
FILE=${1/.lua/}
DEBUG="0"
MODE="full_bc"
LUA_MODULE="0"
LUA_ARGS=

# parse command line parameters.
for arg in "$@" ; do
	case "$arg" in
	-lua-module*)  LUA_MODULE="1"; MODE="lua_mod"; LUA_ARGS="$LUA_ARGS $arg" ;;
	-debug)  DEBUG="1" ;;
	mode=*)  MODE=` echo "$arg" | sed -e 's/mod=//'` ;;
	arch=*)  ARCH=` echo "$arg" | sed -e 's/arch=//'` ;;
	*) FILE=${arg/.lua/} ;;
	esac
done

FPATH=`dirname ${FILE}`
FNAME=`basename ${FILE}`

# select debug/optimize parameters.
if [[ $DEBUG == "1" ]]; then
	CFLAGS=" -O0 -ggdb "
	LUA_FLAGS=" -O0 -g "
	#LUA_FLAGS=" -O3 -do-not-inline-opcodes -dump-functions "
	#CFLAGS=" -ggdb -O3 -fomit-frame-pointer -pipe -Wall "
	OPT_FLAGS=" -disable-opt "
	LLC_FLAGS=" "
else
	LUA_FLAGS=" -O3 -s "
	#LUA_FLAGS=" -O3 -g "
	#CFLAGS=" -ggdb -march=$ARCH -O3 -fomit-frame-pointer -pipe -Wall "
	CFLAGS=" -march=$ARCH -O3 -fomit-frame-pointer -pipe "
	OPT_FLAGS=" -O3 -std-compile-opts -tailcallelim -tailduplicate "
	LLC_FLAGS=" -tailcallopt "
fi

$LLVM_LUAC $LUA_ARGS $LUA_FLAGS -bc -o ${FILE}.bc ${FILE}.lua || {
	echo "llvm-luac: failed to compile Lua code into LLVM bitcode."
	exit 1;
}
TMPS="${FILE}.bc"

# use one of the compile modes.
case "$MODE" in
	cbe)
		TMPS="$TMPS ${FILE}_opt.bc ${FILE}_run.c"
		opt $OPT_FLAGS -f -o ${FILE}_opt.bc ${FILE}.bc && \
		llc $LLC_FLAGS --march=c -f -o ${FILE}_run.c ${FILE}_opt.bc && \
		gcc $CFLAGS -o ${FILE} ${FILE}_run.c -rdynamic -Wl,-E -lm -ldl
		;;
	c)
		llc $LLC_FLAGS -f -march=c -o ${FILE}.c ${FILE}.bc
		;;
	ll)
		llvm-dis -f -o ${FILE}.ll ${FILE}.bc
		;;
	full_bc)
		TMPS="$TMPS ${FILE}_opt.bc ${FILE}_run.bc ${FILE}_run.s"
		opt $OPT_FLAGS -f -o ${FILE}_run.bc ${FILE}.bc && \
		llc $LLC_FLAGS -f -filetype=asm -o ${FILE}_run.s ${FILE}_run.bc && \
		g++ $CFLAGS -o ${FILE} ${FILE}_run.s -rdynamic -Wl,-E -lm -ldl
		;;
	lua_mod)
		TMPS="$TMPS ${FILE}_opt.bc ${FILE}_mod.c ${FILE}_mod_tmp.c ${FPATH}/${FNAME}.lo ${FPATH}/lib${FNAME}.la"
		opt $OPT_FLAGS -f -o ${FILE}_opt.bc ${FILE}.bc && \
		llc $LLC_FLAGS --march=c -f -o ${FILE}_mod_tmp.c ${FILE}_opt.bc && \
		cat ${FILE}_mod_tmp.c | grep -v "^#line " >${FILE}_mod.c && \
		$LIBTOOL --mode=compile $CC $CFLAGS -c -o ${FILE}.lo ${FILE}_mod.c && \
		$LIBTOOL --mode=link $CC -rpath ${RPATH} -o ${FPATH}/lib${FNAME}.la ${FILE}.lo && \
		cp -p ${FPATH}/.libs/lib${FNAME}.so ${RPATH}/${FILE}.so
		if [[ $DEBUG == "0" ]]; then
			$LIBTOOL --mode=clean rm -f $TMPS
		fi
		;;
	*)
		echo "Invalid compile mode: $MODE"
		;;
esac

if [[ $DEBUG == "0" ]]; then
	rm -f $TMPS
fi

