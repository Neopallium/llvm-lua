/*

  Copyright (c) 2009 Robert G. Jakabosky
  
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


#ifdef __cplusplus
extern "C" {
#endif

#include "lua_core.h"

#include "lua.h"
#include "lobject.h"

#include "lauxlib.h"
#include "lualib.h"

#define ENABLE_PARSER_HOOK 1
#include "hook_parser.c"

#ifndef UNUSED
#define UNUSED(x) ((void)x)
#endif

/*
 * link against this file to use the Lua VM core without LLVM.
 * This will disable JIT support, but still allow loading static compiled Lua scripts.
 */
void llvm_new_compiler(lua_State *L) {UNUSED(L);}
void llvm_free_compiler(lua_State *L) {UNUSED(L);}
void llvm_compiler_compile(lua_State *L, Proto *p) {UNUSED(L);UNUSED(p);}
void llvm_compiler_compile_all(lua_State *L, Proto *p) {UNUSED(L);UNUSED(p);}
void llvm_compiler_free(lua_State *L, Proto *p) {UNUSED(L);UNUSED(p);}

void llvm_dumper_dump(const char *output, lua_State *L, Proto *p, int stripping) {UNUSED(L);UNUSED(p);UNUSED(output);UNUSED(stripping);}

#ifdef __cplusplus
}
#endif

