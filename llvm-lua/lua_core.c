/*
  lua_core.c -- Lua core, libraries and JIT hooks compiled into a single file

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

#include "lua_core.h"
#include "lobject.h"
#include "llvm_compiler.h"

void llvm_newproto (lua_State *L, Proto *f);
void llvm_freeproto (lua_State *L, Proto *f);

/* functions */
#define JIT_NEWPROTO(L,f) llvm_newproto(L,f)
#define JIT_FREEPROTO(L,f) llvm_freeproto(L,f)
#define JIT_PRECALL llvm_precall_lua

#include "lapi.c"
#include "lcode.c"
#include "ldebug.c"
#include "ldump.c"
#include "lfunc.c"
#include "lgc.c"
#include "llex.c"
#include "lmem.c"
#include "lobject.c"
#include "lopcodes.c"
#include "lparser.c"
#include "lstate.c"
#include "lstring.c"
#include "ltable.c"
#include "ltm.c"
#include "lundump.c"
#include "lvm.c"
#include "lzio.c"

#include "lbaselib.c"
#include "lcoco.c"
#include "ldblib.c"
#include "liolib.c"
#include "linit.c"
#include "llvm_lmathlib.c"
#include "loadlib.c"
#include "loslib.c"
#include "lstrlib.c"
#include "ltablib.c"

#include "lauxlib.c"

void llvm_newproto (lua_State *L, Proto *f) {
	f->jit_func = NULL;
	f->func_ref = NULL;
}

void llvm_freeproto (lua_State *L, Proto *f) {
	llvm_compiler_free(L, f);
}

