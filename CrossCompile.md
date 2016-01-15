# Introduction #

llvm-lua now has support for native cross-compiling support.

# Required software #
  * [Download & compile the latest LLVM & Clang release](http://llvm.org/releases/download.html#3.1)
  * [CMake](http://www.cmake.org/), used to build llvm-lua

See [this page](http://clang.llvm.org/get_started.html) for how to compile LLVM & Clang.

# Compiling llvm-luac as a cross-compiler #
1. Setup build folder for cmake:
  * git clone https://code.google.com/p/llvm-lua/
  * cd llvm-lua
  * mkdir build-cross
  * cd build-cross
  * cmake ..

2. Set the following cmake variables using the "ccmake" GUI:
  * CROSS\_COMPILE=ON
  * CROSS\_CPU=arm926ej-s
  * CROSS\_ISYSTEM=/usr/lib/gcc/arm-linux-gnueabi/4.6.3/include:/usr/lib/gcc/arm-linux-gnueabi/4.6.3/include-fixed:/usr/arm-linux-gnueabi/usr/include/
  * CROSS\_TRIPLE=arm-linux-gnueabi
  * LLVM\_CC=clang

For good performance it is important to select the correct CROSS\_CPU that matches the target cpu you will be compiling for.  You can change this later in the "lua-cross-compiler" script, in case you need to compile for different arm cpus.

3. Compile llvm-luac
  * make
  * make install

You should now have the following files for llvm-luac:
/usr/local/bin/arm-linux-gnueabi-llvm-luac
/usr/local/bin/lua-cross-compiler

# Cross compiling a Lua script. #
To native compile a single Lua script into a standalone execute for the target architecture:
  * lua-cross-compiler script.lua

To native compile sub-modules into the same standalone execute:
  * lua-cross-compiler -L sub1.lua -L sub2 -L modules/sub3.lua script.lua