# llvm-lua

# Your platform. See PLATS for possible values.
PLAT= none

CC= gcc
CXX= g++
LLVM_CC= llvm-gcc
LLVM_CXX= llvm-g++
LLVM_LD= llvm-link
LLC= llc
LD= g++
AR= ar rcu
RM= rm -f
RANLIB = ranlib

LIBS= -lm $(MYLIBS)
ARCH=i686
#ARCH=pentium4
#ARCH=athlon64
#LL_FLAGS= -m32 
ifdef DEBUG
CFLAGS= -O0 -ggdb -Wall
#CFLAGS= -ggdb -O3 -fomit-frame-pointer -pipe -Wall
else
CFLAGS= -march=$(ARCH) -O3 -fomit-frame-pointer -pipe -Wall
#CFLAGS= -O3 -fomit-frame-pointer -pipe -Wall
#CFLAGS= -O2 -Wall
endif

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

# disable debugging of compiled lua code.  Comment this line if you want to use debug hooks.
CFLAGS += -DLUA_NODEBUG

# Comment USE_BITCODE_FILE to embed the 'lua_vm_ops.bc' file into llvm-lua & llvm-luac
USE_BITCODE_FILE=1

# == END OF USER SETTINGS. NO NEED TO CHANGE ANYTHING BELOW THIS LINE =========

PLATS= aix ansi bsd freebsd generic linux macosx mingw posix solaris

LUA_SRC= ../src

OPS_CFLAGS= $(CFLAGS) $(MYCFLAGS) $(COCOCFLAGS)
LUA_CFLAGS= -I$(LUA_SRC) $(CFLAGS) $(MYCFLAGS) $(COCOCFLAGS)
LLVM_CFLAGS=`llvm-config --cppflags`
LLVM_LFLAGS=`llvm-config --ldflags --libs core jit native bitreader bitwriter ipo`

COMMON_OBJS=LLVMCompiler.o LLVMDumper.o llvm_compiler.o load_vm_ops.o \
	lua_core.o lua_vm_ops_static.o
INTERPRETER_OBJS=llvm-lua.o lua_interpreter.o $(COMMON_OBJS)
COMPILER_OBJS=llvm-luac.o lua_compiler.o $(COMMON_OBJS)

ifndef USE_BITCODE_FILE
INTERPRETER_OBJS += lua_vm_ops_module.o
COMPILER_OBJS += lua_vm_ops_module.o
else
CFLAGS += -DUSE_BITCODE_FILE
endif

default: $(PLAT)

all: llvm-lua llvm-luac

%.o: %.c
	$(CC) -c $(LUA_CFLAGS) -o $@ $<

%.o: %.cpp
	$(CXX) -c $(LUA_CFLAGS) $(LLVM_CFLAGS) -o $@ $<

%.bc: %.c
	$(LLVM_CC) $(LL_FLAGS) -emit-llvm -c $(LUA_CFLAGS) -o $@ $<

lua_core.o: lua_core.c $(LUA_SRC)/*.c $(LUA_SRC)/*.h
	$(CC) -c $(LUA_CFLAGS) -o $@ lua_core.c

lua_vm_ops.bc: lua_vm_ops.c
	$(LLVM_CC) $(LL_FLAGS) -emit-llvm -c -I$(LUA_SRC) $(OPS_CFLAGS) -o $@ $<

lua_vm_ops_module.inc.cpp: lua_vm_ops.bc
	$(LLC) -march=cpp -f --cppgen=module -o $@ $<

lua_vm_ops_module.o: lua_vm_ops_module.cpp lua_vm_ops_module.inc.cpp
	$(CXX) -O0 -fomit-frame-pointer -c $(LLVM_CFLAGS) -o $@ $<

liblua_main.bc: lua_core.bc lua_main.bc load_jit_proto.bc lua_vm_ops_static.bc
	$(LLVM_LD) -f -o $@ $^

liblua_main.a: lua_core.o lua_main.o load_jit_proto.o lua_vm_ops_static.o
	$(AR) $@ $?
	$(RANLIB) $@

lua_normal.o: lua_normal.c $(LUA_SRC)/*.c $(LUA_SRC)/*.h
	$(CC) -c $(LUA_CFLAGS) -o $@ lua_normal.c

lua: lua_normal.o
	$(LD) -o $@ $^ $(MYLIBS)

llvm-lua: $(INTERPRETER_OBJS) lua_vm_ops.bc
	$(LD) $(MYLDFLAGS) $(MYLIBS) -o $@ $(INTERPRETER_OBJS) $(LLVM_LFLAGS)

llvm-luac: $(COMPILER_OBJS) lua_vm_ops.bc liblua_main.bc liblua_main.a
	$(LD) $(MYLDFLAGS) $(MYLIBS) -o $@ $(COMPILER_OBJS) $(LLVM_LFLAGS)

clean:
	$(RM) *.bc *.o lib*.a lua llvm-lua llvm-luac lua_vm_ops_module.inc.cpp

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
	$(MAKE) "LUA_A=lua51.dll" "LUA_T=lua.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"MYCFLAGS=-DLUA_BUILD_AS_DLL" "MYLIBS=" "MYLDFLAGS=-s" lua.exe
	$(MAKE) "LUAC_T=luac.exe" luac.exe

posix:
	$(MAKE) all MYCFLAGS=-DLUA_USE_POSIX

solaris:
	$(MAKE) all MYCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN" MYLIBS="-ldl"

# list targets that do not create files (but not all makes understand .PHONY)
.PHONY: all $(PLATS) default o a clean depend echo none

