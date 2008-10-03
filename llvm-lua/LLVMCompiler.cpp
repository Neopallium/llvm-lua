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

llvm::cl::opt<bool> Fast("fast",
                   llvm::cl::desc("Generate code quickly, "
                            "potentially sacrificing code quality"),
                   llvm::cl::init(false));

llvm::cl::opt<bool> OpCodeStats("opcode-stats",
                   llvm::cl::desc("Generate stats on compiled Lua opcodes."),
                   llvm::cl::init(false));

llvm::cl::opt<bool> CompileLargeFunctions("compile-large-functions",
                   llvm::cl::desc("Compile all Lua functions even really large functions."),
                   llvm::cl::init(false));

llvm::cl::opt<bool> DontInlineOpcodes("do-not-inline-opcodes",
                   llvm::cl::desc("Turn off inlining of opcode functions."),
                   llvm::cl::init(false));

llvm::cl::opt<bool> VerifyFunctions("verify-functions",
                   llvm::cl::desc("Verify each compiled function."),
                   llvm::cl::init(false));


//===----------------------------------------------------------------------===//
// Lua bytecode to LLVM IR compiler
//===----------------------------------------------------------------------===//

const llvm::Type *LLVMCompiler::get_var_type(val_t type) {
	switch(type) {
	case VAR_T_INT:
		return llvm::Type::Int32Ty;
	case VAR_T_ARG_A:
		return llvm::Type::Int32Ty;
	case VAR_T_ARG_C:
		return llvm::Type::Int32Ty;
	case VAR_T_ARG_C_NEXT_INSTRUCTION:
		return llvm::Type::Int32Ty;
	case VAR_T_ARG_B:
		return llvm::Type::Int32Ty;
	case VAR_T_ARG_Bx:
		return llvm::Type::Int32Ty;
	case VAR_T_ARG_sBx:
		return llvm::Type::Int32Ty;
	case VAR_T_PC_OFFSET:
		return llvm::Type::Int32Ty;
	case VAR_T_INSTRUCTION:
		return llvm::Type::Int32Ty;
	case VAR_T_NEXT_INSTRUCTION:
		return llvm::Type::Int32Ty;
	case VAR_T_LUA_STATE_PTR:
		return Ty_lua_State_ptr;
	case VAR_T_K:
		return Ty_TValue_ptr;
	case VAR_T_CL:
		return Ty_LClosure_ptr;
	case VAR_T_VOID:
		return llvm::Type::VoidTy;
	default:
		fprintf(stderr, "Error: not implemented!\n");
		break;
	}
	return NULL;
}

LLVMCompiler::LLVMCompiler(int useJIT) {
	llvm::ModuleProvider *MP = NULL;
	std::string error;
	llvm::Timer load_ops("load_ops");
	llvm::Timer load_jit("load_jit");
	llvm::FunctionType *func_type;
	llvm::Function *func;
	std::vector<const llvm::Type*> func_args;

	// create timers.
	lua_to_llvm = new llvm::Timer("lua_to_llvm");
	codegen = new llvm::Timer("codegen");

	if(llvm::TimePassesIsEnabled) load_ops.startTimer();

	if(OpCodeStats) {
		opcode_stats = new int[NUM_OPCODES];
		for(int i = 0; i < NUM_OPCODES; i++) {
			opcode_stats[i] = 0;
		}
	}

	if(!useJIT) {
		// running as static compiler, so don't use Lazy loading.
		NoLazyCompilation = true;
	}
	// load vm op functions
	MP = load_vm_ops(NoLazyCompilation);
	TheModule = MP->getModule();

	// get important struct types.
	Ty_TValue = TheModule->getTypeByName("struct.TValue");
	Ty_TValue_ptr = llvm::PointerType::getUnqual(Ty_TValue);
	Ty_LClosure = TheModule->getTypeByName("struct.LClosure");
	Ty_LClosure_ptr = llvm::PointerType::getUnqual(Ty_LClosure);
	Ty_lua_State = TheModule->getTypeByName("struct.lua_State");
	Ty_lua_State_ptr = llvm::PointerType::getUnqual(Ty_lua_State);
	// setup argument lists.
	func_args.clear();
	func_args.push_back(Ty_lua_State_ptr);
	lua_func_type = llvm::FunctionType::get(llvm::Type::Int32Ty, func_args, false);
	// define extern vm_next_OP
	vm_next_OP = TheModule->getFunction("vm_next_OP");
	if(vm_next_OP == NULL) {
		func_args.clear();
		func_args.push_back(Ty_lua_State_ptr);
		func_args.push_back(Ty_LClosure_ptr);
		func_type = llvm::FunctionType::get(llvm::Type::VoidTy, func_args, false);
		vm_next_OP = llvm::Function::Create(func_type,
			llvm::Function::ExternalLinkage, "vm_next_OP", TheModule);
	}
	// define extern vm_print_OP
	vm_print_OP = TheModule->getFunction("vm_print_OP");
	if(vm_print_OP == NULL) {
		func_args.clear();
		func_args.push_back(Ty_lua_State_ptr);
		func_args.push_back(Ty_LClosure_ptr);
		func_args.push_back(llvm::Type::Int32Ty);
		func_type = llvm::FunctionType::get(llvm::Type::VoidTy, func_args, false);
		vm_print_OP = llvm::Function::Create(func_type,
			llvm::Function::ExternalLinkage, "vm_print_OP", TheModule);
	}
	// define extern vm_get_current_closure
	vm_get_current_closure = TheModule->getFunction("vm_get_current_closure");
	// define extern vm_get_current_constants
	vm_get_current_constants = TheModule->getFunction("vm_get_current_constants");

	// create prototype for vm_* functions.
	vm_ops = new llvm::Function *[NUM_OPCODES];
	for(int i = 0; i < NUM_OPCODES; i++) {
		vm_ops[i] = NULL;
		func = TheModule->getFunction(vm_op_functions[i].name);
		if(func != NULL) {
			vm_ops[i] = func;
			continue;
		}
		func_args.clear();
		for(int x = 0; vm_op_functions[i].params[x] != VAR_T_VOID; x++) {
			func_args.push_back(get_var_type(vm_op_functions[i].params[x]));
		}
		func_type = llvm::FunctionType::get(get_var_type(vm_op_functions[i].ret_type), func_args, false);
		func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, vm_op_functions[i].name, TheModule);
		vm_ops[i] = func;
	}

	if(llvm::TimePassesIsEnabled) load_ops.stopTimer();

	if(llvm::TimePassesIsEnabled) load_jit.startTimer();
	// Create the JIT.
	if(useJIT) {
		TheExecutionEngine = llvm::ExecutionEngine::create(MP, false, &error, Fast);
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
	
	if(llvm::TimePassesIsEnabled) load_jit.stopTimer();
}

LLVMCompiler::~LLVMCompiler() {
	// print opcode stats.
	if(OpCodeStats) {
		int order[NUM_OPCODES];
		for(int op = 0; op < NUM_OPCODES; op++) {
			order[op] = op;
			for(int n = 0; n < op; n++) {
				if(opcode_stats[op] >= opcode_stats[order[n]]) {
					// insert here.
					memmove(&order[n + 1], &order[n], (op - n) * sizeof(int));
					order[n] = op;
					break;
				}
			}
		}
		// sort by count.
		fprintf(stderr, "===================== Compiled OpCode counts =======================\n");
		int op;
		for(int i = 0; i < NUM_OPCODES; i++) {
			op = order[i];
			if(opcode_stats[op] == 0) continue;
			fprintf(stderr, "%7d: %s(%d)\n", opcode_stats[op], luaP_opnames[op], op);
		}
		delete opcode_stats;
	}

	delete lua_to_llvm;
	delete codegen;
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
	Instruction *code=p->code;
	int code_len=p->sizecode;
	llvm::Function *func;
	llvm::BasicBlock *true_block=NULL;
	llvm::BasicBlock *false_block=NULL;
	llvm::BasicBlock *current_block=NULL;
	llvm::BasicBlock *entry_block=NULL;
	llvm::Value *brcond=NULL;
	llvm::Value *func_L;
	llvm::Value *func_cl;
	llvm::Value *func_k;
	const vm_func_info *func_info;
	std::vector<llvm::Value*> args;
	llvm::CallInst *call=NULL;
	std::vector<llvm::CallInst *> inlineList;
	std::string name;
	char tmp[128];
	int branch;
	int op;
	int i;

	// don't JIT large functions.
	if(code_len >= 800 && TheExecutionEngine != NULL && !CompileLargeFunctions) {
		return;
	}
	if(llvm::TimePassesIsEnabled) lua_to_llvm->startTimer();
	// create function.
	name = getstr(p->source);
	// replace '.' with '_', gdb doesn't like functions with '.' in their name.
	for(size_t n = name.find('.'); n != std::string::npos; n = name.find('.',n)) name[n] = '_';
	snprintf(tmp,128,"_%d_%d",p->linedefined, p->lastlinedefined);
	name += tmp;
	func = llvm::Function::Create(lua_func_type, llvm::Function::ExternalLinkage, name, TheModule);
	// name arg1 = "L"
	func_L = func->arg_begin();
	func_L->setName("L");
	// entry block
	entry_block = llvm::BasicBlock::Create("entry", func);
	Builder.SetInsertPoint(entry_block);
	// get LClosure & constants.
	call=Builder.CreateCall(vm_get_current_closure, func_L);
	inlineList.push_back(call);
	func_cl=call;
	call=Builder.CreateCall(vm_get_current_constants, func_cl);
	inlineList.push_back(call);
	func_k=call;

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
				branch = ++i + 1;
				need_op_block[branch + GETARG_sBx(code[i])] = true; /* inline JMP op. */
				need_op_block[branch] = true;
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
		if(OpCodeStats) {
			opcode_stats[op]++;
		}
		//fprintf(stderr, "%d: '%s' (%d) = 0x%08X\n", i, luaP_opnames[op], op, code[i]);
		//Builder.CreateCall3(vm_print_OP, func_L, func_cl, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
#ifndef LUA_NODEBUG
		/* vm_next_OP function is used to call count/line debug hooks. */
		Builder.CreateCall2(vm_next_OP, func_L, func_cl);
#endif
		// setup arguments for opcode function.
		func_info = &(vm_op_functions[op]);
		if(func_info == NULL) {
			fprintf(stderr, "Error missing vm_OP_* function for opcode: %d\n", op);
			return;
		}
		args.clear();
		for(int x = 0; func_info->params[x] != VAR_T_VOID ; x++) {
			llvm::Value *val=NULL;
			switch(func_info->params[x]) {
			case VAR_T_ARG_A:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_A(code[i])));
				break;
			case VAR_T_ARG_C:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_C(code[i])));
				break;
			case VAR_T_ARG_C_NEXT_INSTRUCTION: {
				int c = GETARG_C(code[i]);
				// if C == 0, then next code value is used as ARG_C.
				if(c == 0) {
					if((i+1) < code_len) {
						c = code[i+1];
					}
				}
				val = llvm::ConstantInt::get(llvm::APInt(32,c));
				break;
			}
			case VAR_T_ARG_B:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_B(code[i])));
				break;
			case VAR_T_ARG_Bx:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_Bx(code[i])));
				break;
			case VAR_T_ARG_sBx:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_sBx(code[i])));
				break;
			case VAR_T_PC_OFFSET:
				val = llvm::ConstantInt::get(llvm::APInt(32,i + 1));
				break;
			case VAR_T_INSTRUCTION:
				val = llvm::ConstantInt::get(llvm::APInt(32,code[i]));
				break;
			case VAR_T_NEXT_INSTRUCTION:
				val = llvm::ConstantInt::get(llvm::APInt(32,code[i+1]));
				break;
			case VAR_T_LUA_STATE_PTR:
				val = func_L;
				break;
			case VAR_T_K:
				val = func_k;
				break;
			case VAR_T_CL:
				val = func_cl;
				break;
			default:
				fprintf(stderr, "Error: not implemented!\n");
				return;
			case VAR_T_VOID:
				fprintf(stderr, "Error: invalid value type!\n");
				return;
			}
			args.push_back(val);
		}
		// create call to opcode function.
		if(func_info->ret_type != VAR_T_VOID) {
			call=Builder.CreateCall(vm_ops[op], args.begin(), args.end(), "retval");
		} else {
			call=Builder.CreateCall(vm_ops[op], args.begin(), args.end());
		}
		// handle retval from opcode function.
		switch (op) {
			case OP_LOADBOOL:
				// check C operand if C!=0 then skip over the next op_block.
				if(GETARG_C(code[i]) != 0) branch += 1;
				else branch = -2;
				inlineList.push_back(call);
				break;
			case OP_LOADK:
			case OP_LOADNIL:
			case OP_GETGLOBAL:
			case OP_GETTABLE:
			case OP_SETGLOBAL:
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
			case OP_GETUPVAL:
			case OP_MOVE:
				inlineList.push_back(call);
				branch = -2;
				break;
			case OP_SETUPVAL:
				//inlineList.push_back(call);
				branch = -2;
				break;
			case OP_VARARG:
			case OP_CALL:
				branch = -2;
				break;
			case OP_TAILCALL:
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
				inlineList.push_back(call);
				break;
			case OP_EQ:
			case OP_LT:
			case OP_LE:
			case OP_TEST:
			case OP_TESTSET:
			case OP_TFORLOOP:
				inlineList.push_back(call);
				brcond=call;
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(llvm::APInt(32,0)), "brcond");
				false_block=op_blocks[branch+1];
				/* inline JMP op. */
				branch = ++i + 1;
				branch += GETARG_sBx(code[i]);
				true_block=op_blocks[branch];
				branch = -1; // do conditional branch
				break;
			case OP_FORLOOP:
				inlineList.push_back(call);
				brcond=call;
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(llvm::APInt(32,0)), "brcond");
				true_block=op_blocks[branch + GETARG_sBx(code[i])];
				false_block=op_blocks[branch];
				branch = -1; // do conditional branch
				break;
			case OP_FORPREP:
				inlineList.push_back(call);
				branch += GETARG_sBx(code[i]);
				break;
			case OP_SETLIST:
				// if C == 0, then next code value is used as ARG_C.
				if(GETARG_C(code[i]) == 0) {
					i++;
				}
				branch = -2;
				break;
			case OP_CLOSURE: {
				Proto *p2 = p->p[GETARG_Bx(code[i])];
				int nups = p2->nups;
				if(nups > 0) {
					i += nups;
				}
				branch = -2;
				break;
			}
			case OP_RETURN: {
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
	if(code_len < 500 && TheExecutionEngine != NULL && opt > 0 && !DontInlineOpcodes) {
		for(std::vector<llvm::CallInst *>::iterator I=inlineList.begin(); I != inlineList.end() ; I++) {
			InlineFunction(*I);
		}
		// Validate the generated code, checking for consistency.
		//if(VerifyFunctions) verifyFunction(*func);
		// Optimize the function.
		TheFPM->run(*func);
	}
	if(llvm::TimePassesIsEnabled) lua_to_llvm->stopTimer();

	if(llvm::TimePassesIsEnabled) codegen->startTimer();
	// finished.
	if(TheExecutionEngine != NULL) {
		p->jit_func = (lua_CFunction)TheExecutionEngine->getPointerToFunction(func);
	} else {
		p->jit_func = NULL;
	}
	p->func_ref = func;
	if(llvm::TimePassesIsEnabled) codegen->stopTimer();
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

