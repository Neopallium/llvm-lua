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

#include "llvm/DerivedTypes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/CommandLine.h"
#include <cstdio>
#include <string>
#include <vector>

#include "LLVMCompiler.h"
#include "lopcodes.h"
#include "lobject.h"
#include "lstate.h"
#include "load_vm_ops.h"

/*
 * Using lazing compilation requires large 512K c-stacks for each coroutine.
 */
static bool NoLazyCompilation = true;

static const bool use_timers = false;

//===----------------------------------------------------------------------===//
// Lua bytecode to LLVM IR compiler
//===----------------------------------------------------------------------===//

LLVMCompiler::LLVMCompiler(int useJIT, int argc, char ** argv) {
	llvm::ModuleProvider *MP = NULL;
	std::string error;
	llvm::Timer load_ops("load_ops");
	llvm::Timer load_jit("load_jit");

	if(use_timers) load_ops.startTimer();

	if(!useJIT) {
		// running as static compiler, so don't use Lazy loading.
		NoLazyCompilation = true;
	}
	// load vm op functions
	MP = load_vm_ops(NoLazyCompilation);
	TheModule = MP->getModule();

	// get reference to vm_OP_* functions.
	vm_ops = new llvm::Function *[64];
	std::string func_prefix = "vm_OP_";
	for(int i = 0; i < 64; i++) {
		vm_ops[i] = NULL;
	}
	for(int i = 0; i < 64; i++) {
		const char *opname = luaP_opnames[i];
		if(opname == NULL) {
			break;
		}
		vm_ops[i] = TheModule->getFunction(func_prefix + opname);
		//printf("'%s' = op function = %p\n", opname, vm_ops[i]);
	}
	vm_func_state_init = TheModule->getFunction("vm_func_state_init");
	vm_print_OP = TheModule->getFunction("vm_print_OP");
	// get important struct types.
	Ty_lua_State = TheModule->getTypeByName("struct.lua_State");
	Ty_lua_State_ptr = llvm::PointerType::getUnqual(Ty_lua_State);
	Ty_func_state = TheModule->getTypeByName("struct.func_state");
	Ty_func_state_ptr = llvm::PointerType::getUnqual(Ty_func_state);
	{
		// setup argument lists.
		std::vector<const llvm::Type*> args;
		args.push_back(Ty_lua_State_ptr);
		lua_func_type = llvm::FunctionType::get(llvm::Type::Int32Ty, args, false);
	}
	if(use_timers) load_ops.stopTimer();

	if(use_timers) load_jit.startTimer();
	// Create the JIT.
	if(useJIT) {
		TheExecutionEngine = llvm::ExecutionEngine::create(MP, false, &error, true);
		if(!TheExecutionEngine && !error.empty()) {
			printf("Error creating JIT engine: %s\n", error.c_str());
		}
		if (NoLazyCompilation)
			TheExecutionEngine->DisableLazyCompilation();

		TheExecutionEngine->runStaticConstructorsDestructors(false);

		if (NoLazyCompilation) {
			for (llvm::Module::iterator I = TheModule->begin(), E = TheModule->end(); I != E; ++I) {
				llvm::Function *Fn = &*I;
				if (!Fn->isDeclaration())
					TheExecutionEngine->getPointerToFunction(Fn);
			}
		}
	} else {
		TheExecutionEngine = NULL;
	}

	TheFPM = new llvm::FunctionPassManager(MP);
	
	/*
	 * Function Pass Manager.
	 */
	// Set up the optimizer pipeline.  Start with registering info about how the
	// target lays out data structures.
	if(useJIT) {
		TheFPM->add(new llvm::TargetData(*TheExecutionEngine->getTargetData()));
	} else {
		TheFPM->add(new llvm::TargetData(TheModule));
	}
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	TheFPM->add(llvm::createCFGSimplificationPass());
	// mem2reg
	TheFPM->add(llvm::createPromoteMemoryToRegisterPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	TheFPM->add(llvm::createInstructionCombiningPass());

	// TailDuplication
	//TheFPM->add(llvm::createTailDuplicationPass());
	// BlockPlacement
	//TheFPM->add(llvm::createBlockPlacementPass());
	// Reassociate expressions.
	//TheFPM->add(llvm::createReassociatePass());
	// Eliminate Common SubExpressions.
	//TheFPM->add(llvm::createGVNPass());
	// Dead code Elimination
	//TheFPM->add(llvm::createDeadCodeEliminationPass());
	
	if(use_timers) load_jit.stopTimer();
}

LLVMCompiler::~LLVMCompiler() {
	delete TheFPM;

	TheFPM = NULL;

	// Print out all of the generated code.
	//TheModule->dump();

	if(TheExecutionEngine) {
		TheExecutionEngine->runStaticConstructorsDestructors(true);
	}
}

void LLVMCompiler::optimize(Proto *p, int opt)
{
	llvm::Function *func=(llvm::Function *)p->func_ref;
	if(func == NULL) return;
	if(opt >= 1) {
		TheFPM->run(*func);
	}
}

/*
 * Optimize all jitted functions.
 */
void LLVMCompiler::optimizeAll(Proto *parent, int opt) {
	/* optimize parent */
	optimize(parent, 2);
	/* optimize all children */
	for(int i = 0; i < parent->sizep; i++) {
		optimizeAll(parent->p[i], 2);
	}
}

/*
 * Pre-Compile all loaded functions.
 */
void LLVMCompiler::compileAll(Proto *parent, int opt) {
	int i;
	/* pre-compile parent */
	compile(parent, opt);
	/* pre-compile all children */
	for(i = 0; i < parent->sizep; i++) {
		compileAll(parent->p[i], opt);
	}
}

void LLVMCompiler::compile(Proto *p, int opt)
{
	LuaInstruction *code=p->code;
	int code_len=p->sizecode;
	llvm::Function *func;
	llvm::BasicBlock *true_block=NULL;
	llvm::BasicBlock *false_block=NULL;
	llvm::BasicBlock *current_block=NULL;
	llvm::BasicBlock *entry_block=NULL;
	llvm::Value *brcond=NULL;
	llvm::Value *fs;
	llvm::CallInst *call=NULL;
	std::vector<llvm::CallInst *> inlineList;
	std::string name;
	char tmp[128];
	int branch;
	int op;
	int i;

	// don't JIT large functions.
	if(code_len >= 800 && TheExecutionEngine != NULL) {
		return;
	}
	// create function.
	name = getstr(p->source);
	// replace '.' with '_', gdb doesn't like functions with '.' in their name.
	for(size_t n = name.find('.'); n != std::string::npos; n = name.find('.',n)) name[n] = '_';
	snprintf(tmp,128,"_%d_%d",p->linedefined, p->lastlinedefined);
	name += tmp;
	func = llvm::Function::Create(lua_func_type, llvm::Function::ExternalLinkage, name, TheModule);
	// name arg1 = "L"
	func->arg_begin()->setName("L");
	// entry block
	entry_block = llvm::BasicBlock::Create("entry", func);
	Builder.SetInsertPoint(entry_block);
	// setup func_state structure on stack.
	fs = Builder.CreateAlloca(Ty_func_state, 0, "fs");
	// call vm_func_state_init to initialize func_state.
	call=Builder.CreateCall2(vm_func_state_init, func->arg_begin(), fs);
	inlineList.push_back(call);

	// pre-create basic blocks.
	llvm::BasicBlock *op_blocks[code_len];
	bool need_op_block[code_len];
	for(i = 0; i < code_len; i++) {
		need_op_block[i] = false;
	}
	// find all jump/branch destinations and create a new basic block at that point.
	for(i = 0; i < code_len; i++) {
		branch = -1;
		op = GET_OPCODE(code[i]);
		switch (op) {
			case OP_LOADBOOL:
				// check C operand if C!=0 then skip over the next op_block.
				if(GETARG_C(code[i]) != 0)
					branch = i+2;
				need_op_block[branch] = true;
				break;
			case OP_JMP:
				// always branch to the offset stored in operand sBx
				branch = i + 1 + GETARG_sBx(code[i]);
				need_op_block[branch] = true;
				break;
			case OP_EQ:
			case OP_LT:
			case OP_LE:
			case OP_TEST:
			case OP_TESTSET:
			case OP_TFORLOOP:
				branch = i+1;
				need_op_block[branch] = true;
				need_op_block[branch+1] = true;
				break;
			case OP_FORLOOP:
				branch = i+1;
				need_op_block[branch] = true;
				branch += GETARG_sBx(code[i]);
				need_op_block[branch] = true;
				break;
			case OP_FORPREP:
				branch = i + 1 + GETARG_sBx(code[i]);
				need_op_block[branch] = true;
				break;
			case OP_SETLIST:
				// if C == 0, then next code value is count value.
				if(GETARG_C(code[i]) == 0) {
					i++;
				}
				break;
			default:
				break;
		}
	}
	name = "op_block";
	for(i = 0; i < code_len; i++) {
		if(need_op_block[i]) {
			snprintf(tmp,128,"_%d",i);
			op_blocks[i] = llvm::BasicBlock::Create(name + tmp, func);
		} else {
			op_blocks[i] = NULL;
		}
	}
	// branch "entry" to first block.
	if(need_op_block[0]) {
		Builder.CreateBr(op_blocks[0]);
	} else {
		current_block = entry_block;
	}
	// gen op calls.
	for(i = 0; i < code_len; i++) {
		if(op_blocks[i] != NULL) {
			if(current_block) {
				// add branch to new block.
				Builder.CreateBr(op_blocks[i]);
			}
			Builder.SetInsertPoint(op_blocks[i]);
			current_block = op_blocks[i];
		}
		// skip dead unreachable code.
		if(current_block == NULL) {
			continue;
		}
		branch = i+1;
		op = GET_OPCODE(code[i]);
		//fprintf(stderr, "%d: '%s' (%d) = 0x%08X\n", i, luaP_opnames[op], op, code[i]);
		//Builder.CreateCall2(vm_print_OP, fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
		switch (op) {
			case OP_LOADBOOL:
				// check C operand if C!=0 then skip over the next op_block.
				if(GETARG_C(code[i]) != 0) branch += 1;
				else branch = -2;
				call=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
				inlineList.push_back(call);
				break;
			case OP_MOVE:
			case OP_LOADK:
			case OP_LOADNIL:
			case OP_GETUPVAL:
			case OP_GETGLOBAL:
			case OP_GETTABLE:
			case OP_SETGLOBAL:
			case OP_SETUPVAL:
			case OP_SETTABLE:
			case OP_NEWTABLE:
			case OP_SELF:
			case OP_ADD:
			case OP_SUB:
			case OP_MUL:
			case OP_DIV:
			case OP_MOD:
			case OP_POW:
			case OP_UNM:
			case OP_NOT:
			case OP_LEN:
			case OP_CONCAT:
			case OP_CLOSE:
				call=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
				inlineList.push_back(call);
				branch = -2;
				break;
			case OP_VARARG:
			case OP_CALL:
				Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
				branch = -2;
				break;
			case OP_TAILCALL:
				call=Builder.CreateCall3(vm_ops[op], fs,
					llvm::ConstantInt::get(llvm::APInt(32,code[i])),
					llvm::ConstantInt::get(llvm::APInt(32,code[i+1])),
					"retval");
				i++;
				call->setTailCall(true);
				Builder.CreateRet(call);
				branch = -2;
				current_block = NULL; // have terminator
				break;
			case OP_JMP:
				// always branch to the offset stored in operand sBx
				branch += GETARG_sBx(code[i]);
				// call vm_OP_JMP just in case luai_threadyield is defined.
				call=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
				inlineList.push_back(call);
				break;
			case OP_EQ:
			case OP_LT:
			case OP_LE:
			case OP_TEST:
			case OP_TESTSET:
			case OP_TFORLOOP:
				call=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])),"ret");
				inlineList.push_back(call);
				brcond=call;
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(llvm::APInt(32,0)), "brcond");
				true_block=op_blocks[branch];
				false_block=op_blocks[branch+1];
				branch = -1; // do conditional branch
				break;
			case OP_FORLOOP:
				call=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])),"ret");
				inlineList.push_back(call);
				brcond=call;
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(llvm::APInt(32,0)), "brcond");
				true_block=op_blocks[branch + GETARG_sBx(code[i])];
				false_block=op_blocks[branch];
				branch = -1; // do conditional branch
				break;
			case OP_FORPREP:
				call=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
				inlineList.push_back(call);
				branch += GETARG_sBx(code[i]);
				break;
			case OP_SETLIST: {
				int c = GETARG_C(code[i]);
				// if C == 0, then next code value is count value.
				if(c == 0) {
					i++;
					if(i < code_len) {
						c = code[i];
					}
				}
				Builder.CreateCall3(vm_ops[op],
					fs,
					llvm::ConstantInt::get(llvm::APInt(32,code[i])),
					llvm::ConstantInt::get(llvm::APInt(32,c))
				);
				branch = -2;
				break;
			}
			case OP_CLOSURE: {
				Proto *p2 = p->p[GETARG_Bx(code[i])];
				int nups = p2->nups;
				Builder.CreateCall3(vm_ops[op],
					fs,
					llvm::ConstantInt::get(llvm::APInt(32,code[i])),
					llvm::ConstantInt::get(llvm::APInt(32, i + 1))
					);
				if(nups > 0) {
					i += nups;
				}
				branch = -2;
				break;
			}
			case OP_RETURN: {
				call=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])),"retval");
				call->setTailCall(true);
				Builder.CreateRet(call);
				branch = -2;
				current_block = NULL; // have terminator
				break;
			}
			default:
				fprintf(stderr, "Bad opcode: opcode=%d\n", op);
				break;
		}
		// branch to next block.
		if(branch >= 0 && branch < code_len) {
			Builder.CreateBr(op_blocks[branch]);
			current_block = NULL; // have terminator
		} else if(branch == -1) {
			Builder.CreateCondBr(brcond, true_block, false_block);
			current_block = NULL; // have terminator
		}
	}
	//func->dump();
	// only run function inliner & optimization passes on same functions.
	if(code_len < 500 && TheExecutionEngine != NULL && opt > 0) {
		for(std::vector<llvm::CallInst *>::iterator I=inlineList.begin(); I != inlineList.end() ; I++) {
			InlineFunction(*I);
		}
		// Validate the generated code, checking for consistency.
		//verifyFunction(*func);
		// Optimize the function.
		TheFPM->run(*func);
	}

	// finished.
	if(TheExecutionEngine != NULL) {
		p->jit_func = (lua_CFunction)TheExecutionEngine->getPointerToFunction(func);
	} else {
		p->jit_func = NULL;
	}
	p->func_ref = func;
}

void LLVMCompiler::free(Proto *p)
{
	llvm::Function *func;

	if(TheExecutionEngine == NULL) return;

	func=(llvm::Function *)TheExecutionEngine->getGlobalValueAtAddress((void *)p->jit_func);
	//fprintf(stderr, "free llvm function ref: %p\n", func);
	if(func != NULL) {
		TheExecutionEngine->freeMachineCodeForFunction(func);
	}
}

