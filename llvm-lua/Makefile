# llvm-lua

LUA_SRC= ../src

CC= gcc
CXX= g++
LLVM_CC= llvm-gcc
LLVM_LD= llvm-link
LD= g++
COCOCFLAGS=
#COCOCFLAGS= -DCOCO_DEFAULT_CSTACKSIZE=524288 -DUSE_VALGRIND

#MYCFLAGS= -O0 -ggdb -Wall
#MYCFLAGS= -march=athlon64 -O3 -fomit-frame-pointer -pipe -Wall
MYCFLAGS= -O3 -fomit-frame-pointer -pipe -Wall

OPS_CFLAGS= -O2 -Wall -DLUA_USE_LINUX $(COCOCFLAGS)

LUA_CFLAGS= -I$(LUA_SRC) $(MYCFLAGS) -DLUA_USE_LINUX $(COCOCFLAGS)
LLVM_CFLAGS=`llvm-config --cppflags`
LLVM_LFLAGS=`llvm-config --ldflags --libs core jit native bitreader bitwriter ipo`
CFLAGS= $(MYCFLAGS)
MYLDFLAGS= -Wl,-E
MYLIBS= -lm -ldl
RM= rm -f

# disable debugging of compiled lua code.
MYCFLAGS += -DLUA_NODEBUG
OPS_CFLAGS += -DLUA_NODEBUG

all: default

macosx:
	$(MAKE) default MYLDFLAGS="" MYLIBS="-lm"

default: lua llvm-lua llvm-luac liblua_main.bc

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

liblua_main.bc: lua_core.bc lua_main.bc load_jit_proto.bc
	$(LLVM_LD) -f -o $@ $^

#lua_all.bc: lua_core.bc lua_vm_ops.bc
#	$(LLVM_LD) -o $@ $^

lua_normal.o: lua_normal.c $(LUA_SRC)/*.c $(LUA_SRC)/*.h
	$(CC) -c $(LUA_CFLAGS) -o $@ lua_normal.c

lua: lua_normal.o
	$(LD) -o $@ $^ $(MYLIBS)

llvm-lua: llvm-lua.o llvm-compiler.o lua_core.o lua_interpreter.o lua_vm_ops.bc
	$(LD) $(MYLDFLAGS) -o $@ llvm-lua.o llvm-compiler.o lua_core.o lua_interpreter.o -L$(LUA_SRC) -llua $(LLVM_LFLAGS)

llvm-luac: llvm-luac.o llvm-compiler.o lua_core.o lua_compiler.o lua_vm_ops.bc
	$(LD) $(MYLDFLAGS) -o $@ llvm-luac.o llvm-compiler.o lua_core.o lua_compiler.o -L$(LUA_SRC) -llua $(LLVM_LFLAGS)

clean:
	$(RM) *.bc *.o lua llvm-lua llvm-luac

.PHONY:	default clean
