# llvm-lua

LUA_SRC= ../src

CC= gcc
CXX= g++
LLVM_CC= llvm-gcc
LLVM_CXX= llvm-g++
LLVM_LD= llvm-link
LLC= llc
LD= g++
COCOCFLAGS=
#COCOCFLAGS= -DCOCO_DEFAULT_CSTACKSIZE=524288 -DUSE_VALGRIND

ifdef DEBUG
MYCFLAGS= -O0 -ggdb -Wall
OPS_CFLAGS= -O0 -DLUA_USE_LINUX $(COCOCFLAGS)
else
#MYCFLAGS= -march=athlon64 -O3 -fomit-frame-pointer -pipe -Wall
MYCFLAGS= -O3 -fomit-frame-pointer -pipe -Wall
OPS_CFLAGS= $(MYCFLAGS) -DLUA_USE_LINUX $(COCOCFLAGS)
endif

LUA_CFLAGS= -I$(LUA_SRC) $(MYCFLAGS) -DLUA_USE_LINUX $(COCOCFLAGS)
LLVM_CFLAGS=`llvm-config --cppflags`
LLVM_LFLAGS=`llvm-config --ldflags --libs core jit native bitreader bitwriter ipo`
CFLAGS= $(MYCFLAGS)
MYLDFLAGS= -rdynamic -Wl,-E
MYLIBS= -lm -ldl
RM= rm -f

# disable debugging of compiled lua code.
MYCFLAGS += -DLUA_NODEBUG
OPS_CFLAGS += -DLUA_NODEBUG

COMMON_OBJS=LLVMCompiler.o LLVMDumper.o llvm_compiler.o load_vm_ops.o \
	lua_core.o lua_vm_ops_static.o
INTERPRETER_OBJS=llvm-lua.o lua_interpreter.o $(COMMON_OBJS)
COMPILER_OBJS=llvm-luac.o lua_compiler.o $(COMMON_OBJS)

###
### comment USE_BITCODE_FILE to embed the 'lua_vm_ops.bc' file into llvm-lua & llvm-luac
###
USE_BITCODE_FILE=1
ifndef USE_BITCODE_FILE
INTERPRETER_OBJS += lua_vm_ops_module.o
COMPILER_OBJS += lua_vm_ops_module.o
else
MYCFLAGS += -DUSE_BITCODE_FILE
endif

all: default

macosx:
	$(MAKE) default MYLDFLAGS="" MYLIBS="-lm"

default: llvm-lua llvm-luac liblua_main.bc

%.o: %.c
	$(CC) -c $(LUA_CFLAGS) -o $@ $<

%.o: %.cpp
	$(CXX) -c $(LUA_CFLAGS) $(LLVM_CFLAGS) -o $@ $<

%.bc: %.c
	$(LLVM_CC) -emit-llvm -c $(LUA_CFLAGS) -o $@ $<

lua_core.o: lua_core.c $(LUA_SRC)/*.c $(LUA_SRC)/*.h
	$(CC) -c $(LUA_CFLAGS) -o $@ lua_core.c

lua_vm_ops.bc: lua_vm_ops.c
	$(LLVM_CC) -emit-llvm -c -I$(LUA_SRC) $(OPS_CFLAGS) -o $@ $<

lua_vm_ops_module.inc.cpp: lua_vm_ops.bc
	$(LLC) -march=cpp -f --cppgen=module -o $@ $<

lua_vm_ops_module.o: lua_vm_ops_module.cpp lua_vm_ops_module.inc.cpp
	$(CXX) -O0 -fomit-frame-pointer -c $(LLVM_CFLAGS) -o $@ $<

liblua_main.bc: lua_core.bc lua_main.bc load_jit_proto.bc lua_vm_ops_static.bc
	$(LLVM_LD) -f -o $@ $^

lua_normal.o: lua_normal.c $(LUA_SRC)/*.c $(LUA_SRC)/*.h
	$(CC) -c $(LUA_CFLAGS) -o $@ lua_normal.c

lua: lua_normal.o
	$(LD) -o $@ $^ $(MYLIBS)

llvm-lua: $(INTERPRETER_OBJS) lua_vm_ops.bc
	$(LD) $(MYLDFLAGS) -o $@ $(INTERPRETER_OBJS) -L$(LUA_SRC) -llua $(LLVM_LFLAGS)

llvm-luac: $(COMPILER_OBJS) lua_vm_ops.bc
	$(LD) $(MYLDFLAGS) -o $@ $(COMPILER_OBJS) -L$(LUA_SRC) -llua $(LLVM_LFLAGS)

clean:
	$(RM) *.bc *.o lua llvm-lua llvm-luac lua_vm_ops_module.inc.cpp

.PHONY:	default clean
