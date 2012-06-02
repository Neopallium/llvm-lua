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

#ifndef LLVMCOMPILER_h
#define LLVMCOMPILER_h

#include "llvm/Support/IRBuilder.h"
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"

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
	class OPFunc {
	public:
		const vm_func_info *info;
		bool compiled;
		llvm::Function *func;
		OPFunc *next;

		OPFunc(const vm_func_info *info_, OPFunc *next_) :
				info(info_), compiled(false), func(NULL), next(next_) {}
		~OPFunc() {
			if(next) delete next;
		}
	};
	class OPValues {
	private:
		int len;
		llvm::Value **values;
	
	public:
		OPValues(int len_) : len(len_), values(new llvm::Value *[len_]) {
			for(int i = 0; i < len; ++i) {
				values[i] = NULL;
			}
		}
	
		~OPValues() {
			delete[] values;
		}
		void set(int idx, llvm::Value *val) {
			assert(idx >= 0 && idx < len);
			values[idx] = val;
		}
		llvm::Value *get(int idx) {
			assert(idx >= 0 && idx < len);
			return values[idx];
		}
	};

private:
	llvm::LLVMContext Context;
	llvm::Module *M;
	llvm::FunctionPassManager *TheFPM;
	llvm::ExecutionEngine *TheExecutionEngine;
	bool strip_code;

	// struct types.
	llvm::Type *Ty_TValue;
	llvm::Type *Ty_TValue_ptr;
	llvm::Type *Ty_LClosure;
	llvm::Type *Ty_LClosure_ptr;
	llvm::Type *Ty_lua_State;
	llvm::Type *Ty_lua_State_ptr;
	// common function types.
	llvm::FunctionType *lua_func_type;
	// functions to get LClosure & constants pointer.
	llvm::Function *vm_get_current_closure;
	llvm::Function *vm_get_current_constants;
	llvm::Function *vm_get_number;
	llvm::Function *vm_get_long;
	llvm::Function *vm_set_number;
	llvm::Function *vm_set_long;
	// function for counting each executed op.
	llvm::Function *vm_count_OP;
	// function for print each executed op.
	llvm::Function *vm_print_OP;
	// function for handling count/line debug hooks.
	llvm::Function *vm_next_OP;
	// function for handling a block of simple opcodes.
	llvm::Function *vm_mini_vm;
	// available op function for each opcode.
	OPFunc **vm_op_funcs;
	// count compiled opcodes.
	int *opcode_stats;

	// timers
	llvm::Timer *lua_to_llvm;
	llvm::Timer *codegen;

	// opcode hints/values/blocks/need_block arrays used in compile() method.
	int opcode_data_len; // length of opcode arrays.
	hint_t *op_hints;
	OPValues **op_values;
	llvm::BasicBlock **op_blocks;
	bool *need_op_block;
	// resize the opcode hint data arrays.
	void resize_opcode_data(int code_len);
	// reset/clear the opcode hint data arrays.
	void clear_opcode_data(int code_len);

public:
	LLVMCompiler(int useJIT);
	~LLVMCompiler();

	/*
	 * set code stripping mode.
	 */
	void setStripCode(bool strip) {
		strip_code = strip;
	}

	/*
	 * return the module.
	 */
	llvm::Module *getModule() {
		return M;
	}

	llvm::LLVMContext& getCtx() {
		return Context;
	}

	llvm::FunctionType *get_lua_func_type() {
		return lua_func_type;
	}

	llvm::Type *get_var_type(val_t type, hint_t hints);

	llvm::Value *get_proto_constant(TValue *constant);
	
	/*
	 * Pre-Compile all loaded functions.
	 */
	void compileAll(lua_State *L, Proto *parent);

	void compile(lua_State *L, Proto *p);

	void free(lua_State *L, Proto *p);
};

#endif

