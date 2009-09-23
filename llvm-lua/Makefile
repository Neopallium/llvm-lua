# llvm-lua

# Your platform. See PLATS for possible values.
PLAT= none

CC= gcc
CXX= g++
LLVM_CC= clang
#LLVM_CC= llvm-gcc
LLVM_LD= llvm-link
LLC= llc
LD= g++
AR= ar rcu
RM= rm -f
RANLIB = ranlib

LIBTOOL = libtool --quiet --tag=CC
LIB_VERSION = 6:4:1

LIBS= -lm $(MYLIBS)
BC_CFLAGS= -c -emit-llvm
#ARCH=i686
#ARCH=pentium4
#ARCH=athlon64
#BC_CFLAGS += -m32 
ifdef DEBUG
CFLAGS= -O0 -ggdb -Wall
#CFLAGS= -ggdb -O3 -fomit-frame-pointer -pipe -Wall
else
CFLAGS= -O3 -fomit-frame-pointer -pipe -Wall -DNDEBUG
#CFLAGS= -O3 -fomit-frame-pointer -pipe -Wall
#CFLAGS= -O2 -Wall
endif
ifdef ARCH
CFLAGS += -march=$(ARCH)
endif

ifdef LUA_CPP_SUPPORT
CC= g++
LLVM_CC= llvm-g++
CFLAGS += -fexceptions -DLUA_CPP_SUPPORT=1
endif

BC_CFLAGS += $(CFLAGS)

MYCFLAGS=
MYLDFLAGS=
MYLIBS=
#MYLDFLAGS= -rdynamic -Wl,-E
#MYLIBS= -lm -ldl

# ++ Coco =========
# Default: autodetect gccasm/setjmp/ucontext/fibers context switch method.
COCOCFLAGS=
# Force use of setjmp (instead of gccasm).
#COCOCFLAGS= -DCOCO_USE_SETJMP
# Force use of ucontext (instead of gccasm or setjmp).
#COCOCFLAGS= -DCOCO_USE_UCONTEXT
# Uncomment this if you want to disable support for Coco.
#COCOCFLAGS= -DCOCO_DISABLE
# Uncomment if you want to use lazy-jit-compilation
#COCOCFLAGS= -DCOCO_DEFAULT_CSTACKSIZE=524288
# -- Coco =========

#
# Cross compile support
#
#### iPhone cross-compile
#LLVM_CC=/path/to/llvm/libexec/clang-cc
#CROSS_FLAGS= -nostdinc -isystem=/path/to/iphone/sdk/include -triple=arm-apple-darwin9 -mcpu=arm
#BC_CFLAGS= -emit-llvm-bc $(CROSS_FLAGS) -O2 -DFORCE_ALIGNMENT=4

# == END OF USER SETTINGS. NO NEED TO CHANGE ANYTHING BELOW THIS LINE =========

PLATS= aix ansi bsd freebsd generic linux macosx mingw posix solaris

LUA_SRC= ../src

LUA_CFLAGS= -I$(LUA_SRC) $(CFLAGS) $(MYCFLAGS) $(COCOCFLAGS)
BC_CFLAGS += -I$(LUA_SRC) $(MYCFLAGS) $(COCOCFLAGS)
LLVM_CFLAGS=`llvm-config --cppflags`
LLVM_LFLAGS=`llvm-config --ldflags --libs core jit native linker bitreader bitwriter ipo`
LLVM_ALL_LFLAGS=`llvm-config --ldflags --libs all`

NON_LLVM_OBJS=lua_core.o lua_vm_ops_static.o load_jit_proto.o
COMMON_OBJS=LLVMCompiler.o llvm_compiler.o load_vm_ops.o load_embedded_bc.o $(NON_LLVM_OBJS)
NO_JIT_OBJS=no_jit.o $(NON_LLVM_OBJS)

INTERPRETER_OBJS=llvm-lua.o lua_interpreter.o $(COMMON_OBJS)
COMPILER_OBJS=llvm-luac.o lua_compiler.o LLVMDumper.o llvm_dumper.o load_liblua_main.o $(COMMON_OBJS)

BIN_EXT=

LIB_NAME=liblua.la
LIB_OBJS=$(NO_JIT_OBJS:.o=.lo)

LLVM_LIB_NAME=liblua-llvm.la
LLVM_LIB_OBJS=lua_interpreter.lo $(COMMON_OBJS:.o=.lo)

LUA_T=lua$(BIN_EXT)
BIN2C_T=bin2c$(BIN_EXT)
LLVM_LUA_T=llvm-lua$(BIN_EXT)
LLVM_LUAC_T=llvm-luac$(BIN_EXT)

BINS = $(LUA_T) $(LLVM_LUA_T) $(LLVM_LUAC_T) $(BIN2C_T)
EMB_BCS = lua_vm_ops_bc.h liblua_main_bc.h

default: $(PLAT)

all: $(LLVM_LUA_T) $(LLVM_LUAC_T) $(LIB_NAME) $(LIB_NAME:.la=.a) $(LLVM_LIB_NAME) $(LLVM_LIB_NAME:.la=.a)

%.lo %.o: %.c
	$(LIBTOOL) --mode=compile $(CC) -c $(LUA_CFLAGS) -o $@ $<

%.lo %.o: %.cpp
	$(LIBTOOL) --mode=compile $(CXX) -c $(LUA_CFLAGS) $(LLVM_CFLAGS) -o $@ $<

%.bc: %.c
	$(LLVM_CC) $(BC_CFLAGS) -o $@ $<

lua_core.o: lua_core.c $(LUA_SRC)/*.c $(LUA_SRC)/*.h
	$(CC) -c $(LUA_CFLAGS) -o $@ lua_core.c

liblua_main.bc: lua_main.bc $(NO_JIT_OBJS:.o=.bc)
	$(LLVM_LD) -f -o $@ $^

liblua_main.a: lua_main.o $(NO_JIT_OBJS)
	$(AR) $@ $?
	$(RANLIB) $@

load_vm_ops.lo load_vm_ops.o: load_vm_ops.cpp lua_vm_ops_bc.h
	$(LIBTOOL) --mode=compile $(CXX) -c $(LUA_CFLAGS) $(LLVM_CFLAGS) -o $@ $<

load_liblua_main.lo load_liblua_main.o: load_liblua_main.cpp liblua_main_bc.h
	$(LIBTOOL) --mode=compile $(CXX) -c $(LUA_CFLAGS) $(LLVM_CFLAGS) -o $@ $<

%_bc.h: %.bc $(BIN2C_T)
	./$(BIN2C_T) -z -c $< $@

lua_normal.o: lua_normal.c $(LUA_SRC)/*.c $(LUA_SRC)/*.h
	$(CC) -c $(LUA_CFLAGS) -o $@ lua_normal.c

lua: lua_normal.o
	$(LIBTOOL) --mode=link $(CC) -Wl,-E -o $@ $^ $(MYLIBS) -llua

$(LIB_NAME): $(LIB_OBJS)
	$(LIBTOOL) --mode=link $(CC) -version-info $(LIB_VERSION) \
            -rpath $(RPATH) -o $(LIB_NAME) $(LIB_OBJS)

$(LIB_NAME:.la=.a): $(LIB_OBJS)
	$(LIBTOOL) --mode=link $(CC) -static -o $(LIB_NAME:.la=.a) $(LIB_OBJS)

$(LLVM_LIB_NAME): $(LLVM_LIB_OBJS)
	$(LIBTOOL) --mode=link $(CC) -version-info $(LIB_VERSION) \
            -rpath $(RPATH) -o $(LLVM_LIB_NAME) $(LLVM_LIB_OBJS)

$(LLVM_LIB_NAME:.la=.a): $(LLVM_LIB_OBJS)
	$(LIBTOOL) --mode=link $(CC) -static -o $(LLVM_LIB_NAME:.la=.a) $(LLVM_LIB_OBJS)

$(LLVM_LUA_T): $(INTERPRETER_OBJS) lua_vm_ops_static.o
	$(LD) $(MYLDFLAGS) $(MYLIBS) -o $@ $(INTERPRETER_OBJS) $(LLVM_LFLAGS)

$(LLVM_LUAC_T): $(COMPILER_OBJS) liblua_main.bc liblua_main.a
	$(LD) $(MYLDFLAGS) $(MYLIBS) -o $@ $(COMPILER_OBJS) $(LLVM_ALL_LFLAGS)

$(BIN2C_T): bin2c.c
	$(CC) -O3 -o $@ $<

clean:
	$(LIBTOOL) --mode=clean $(RM) *.lo *.bc *.o lib*.a $(BINS) $(EMB_BCS) $(LIB_NAME) $(LLVM_LIB_NAME)

depend:
	@$(CC) $(LUA_CFLAGS) -MM lua_*.c load_*.c
	@$(CXX) $(LUA_CFLAGS) $(LLVM_CFLAGS) -MM LLVM*.cpp llvm-*.cpp load_*.cpp

echo:
	@echo "PLAT = $(PLAT)"
	@echo "CC = $(CC)"
	@echo "LUA_CFLAGS = $(LUA_CFLAGS)"
	@echo "LLVM_CFLAGS = $(LLVM_CFLAGS)"
	@echo "AR = $(AR)"
	@echo "RANLIB = $(RANLIB)"
	@echo "RM = $(RM)"
	@echo "MYCFLAGS = $(MYCFLAGS)"
	@echo "MYLDFLAGS = $(MYLDFLAGS)"
	@echo "MYLIBS = $(MYLIBS)"

# convenience targets for popular platforms

none:
	@echo "Please choose a platform:"
	@echo "   $(PLATS)"

aix:
	$(MAKE) all CC="xlc" CFLAGS="-O2 -DLUA_USE_POSIX -DLUA_USE_DLOPEN" MYLIBS="-ldl" MYLDFLAGS="-brtl -bexpall"

ansi:
	$(MAKE) all MYCFLAGS=-DLUA_ANSI

bsd:
	$(MAKE) all MYCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN" MYLIBS="-Wl,-E"

freebsd:
	$(MAKE) all MYCFLAGS="-DLUA_USE_LINUX" MYLIBS="-Wl,-E -lreadline"

generic:
	$(MAKE) all MYCFLAGS=

linux:
	$(MAKE) all MYCFLAGS=-DLUA_USE_LINUX MYLIBS="-rdynamic -Wl,-E -ldl "
#	$(MAKE) all MYCFLAGS=-DLUA_USE_LINUX MYLIBS="-rdynamic -Wl,-E -ldl -lreadline -lhistory -lncurses"

macosx:
	$(MAKE) all MYCFLAGS=-DLUA_USE_LINUX MYLDFLAGS="" MYLIBS="-lreadline"
# use this on Mac OS X 10.3-
#	$(MAKE) all MYCFLAGS=-DLUA_USE_MACOSX

mingw:
	$(MAKE) "BIN_EXT=.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"MYCFLAGS=-DLUA_BUILD_AS_DLL" "MYLIBS=-rdynamic -Wl,-E -ldl" "MYLDFLAGS=-s" llvm-lua.exe
	$(MAKE) "BIN_EXT=.exe" llvm-luac.exe

posix:
	$(MAKE) all MYCFLAGS=-DLUA_USE_POSIX

solaris:
	$(MAKE) all MYCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN" MYLIBS="-ldl"

# list targets that do not create files (but not all makes understand .PHONY)
.PHONY: all $(PLATS) default o a clean depend echo none

