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

#ifndef LLVMCOMPILER_h
#define LLVMCOMPILER_h

#include "llvm/Support/IRBuilder.h"

#include "lua_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "lobject.h"

#include "lua_vm_ops.h"

#ifdef __cplusplus
}
#endif

namespace llvm {
class FunctionPassManager;
class ExecutionEngine;
class Timer;
}

class LLVMCompiler {
private:
	llvm::Module *TheModule;
	llvm::IRBuilder<> Builder;
	llvm::FunctionPassManager *TheFPM;
	llvm::ExecutionEngine *TheExecutionEngine;

	// struct types.
	const llvm::Type *Ty_lua_State;
	const llvm::Type *Ty_lua_State_ptr;
	const llvm::Type *Ty_func_state;
	const llvm::Type *Ty_func_state_ptr;
	// common function types.
	llvm::FunctionType *lua_func_type;
	// function for inializing func_state.
	llvm::Function *vm_func_state_init;
	// function for print each executed op.
	llvm::Function *vm_print_OP;
	// function for handling count/line debug hooks.
	llvm::Function *vm_next_OP;
	// list of op functions.
	llvm::Function **vm_ops;

	// timers
	llvm::Timer *lua_to_llvm;
	llvm::Timer *codegen;
public:
	LLVMCompiler(int useJIT, int argc, char ** argv);
	~LLVMCompiler();

	/*
	 * return the module.
	 */
	llvm::Module *getModule() {
		return TheModule;
	}

	llvm::FunctionType *get_lua_func_type() {
		return lua_func_type;
	}
	
	const llvm::Type *get_var_type(var_type type);
	
	void optimize(Proto *p, int opt);
	
	/*
	 * Optimize all jitted functions.
	 */
	void optimizeAll(Proto *parent, int opt);
	
	/*
	 * Pre-Compile all loaded functions.
	 */
	void compileAll(Proto *parent, int opt);

	void compile(Proto *p, int opt);

	void free(Proto *p);
};

#endif

