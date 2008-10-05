/*
  Copyright (c) 2008 Robert G. Jakabosky
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef llvm_compiler_h
#define llvm_compiler_h

#include "lua_core.h"

#define LLVM_LUA_VERSION "llvm-lua 0.3"
#define LLVM_LUA_COPYRIGHT "Copyright (C) 2008 Robert G. Jakabosky"

#ifdef __cplusplus
extern "C" {
#endif

#include "lobject.h"

int llvm_compiler_main(int useJIT, int argc, char ** argv);
void llvm_compiler_cleanup();
void llvm_compiler_optimize(Proto *p, int optimize);
void llvm_compiler_optimize_all(Proto *parent, int optimize);
void llvm_compiler_compile(Proto *p, int optimize);
void llvm_compiler_compile_all(Proto *p, int optimize);
void llvm_compiler_dump(const char *output, Proto *p, int optimize, int stripping);
void llvm_compiler_free(Proto *p);

extern int llvm_precall_jit (lua_State *L, StkId func, int nresults);
extern int llvm_precall_lua (lua_State *L, StkId func, int nresults);


#ifdef __cplusplus
}
#endif

#endif

