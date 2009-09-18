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

#include "LLVMCompiler.h"
#include "LLVMDumper.h"
#include "llvm-c/ExecutionEngine.h"
#include "llvm-c/Target.h"
#include "llvm_compiler.h"
#include "llvm_compiler_private.h"

extern "C" {

#include "lstate.h"

/* only used to turn off JIT for static compiler llvm-luac. */
static int g_useJIT = 1;
static int g_need_init = 1;

int llvm_compiler_main(int useJIT) {
	g_useJIT = useJIT;
	return 0;
}

LLVMCompiler *llvm_get_compiler(lua_State *L) {
	global_State *g = G(L);
	return (LLVMCompiler *)g->llvm_compiler;
}

void llvm_new_compiler(lua_State *L) {
	global_State *g = G(L);
	if(g_need_init) {
		LLVMLinkInJIT();
		LLVMInitializeNativeTarget();
		g_need_init = 0;
	}
	g->llvm_compiler = new LLVMCompiler(g_useJIT);
}

void llvm_free_compiler(lua_State *L) {
	global_State *g = G(L);
	LLVMCompiler *compiler = ((LLVMCompiler *)g->llvm_compiler);
	g->llvm_compiler = NULL;
	delete compiler;
}

void llvm_compiler_compile(lua_State *L, Proto *p) {
	LLVMCompiler *compiler = llvm_get_compiler(L);
	if(compiler == NULL) {
		llvm_compiler_main(1);
	}
	compiler->compile(L, p);
}

void llvm_compiler_compile_all(lua_State *L, Proto *p) {
	LLVMCompiler *compiler = llvm_get_compiler(L);
	if(compiler == NULL) {
		llvm_compiler_main(1);
	}
	compiler->compileAll(L, p);
}

void llvm_compiler_free(lua_State *L, Proto *p) {
	LLVMCompiler *compiler = llvm_get_compiler(L);
	if(compiler != NULL) {
		compiler->free(L, p);
	}
}

}// end: extern "C"

