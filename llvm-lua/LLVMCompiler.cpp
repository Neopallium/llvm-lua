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

#include "llvm/LLVMContext.h"
#include "llvm/DerivedTypes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h" 
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/CommandLine.h"
#include <cstdio>
#include <string>
#include <vector>
#include <math.h>

#include "LLVMCompiler.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "lopcodes.h"
#include "lobject.h"
#include "lstate.h"
#include "ldo.h"
#include "lmem.h"
#ifdef __cplusplus
}
#endif
#include "load_vm_ops.h"

/*
 * Using lazing compilation requires large 512K c-stacks for each coroutine.
 */
static bool NoLazyCompilation = true;
static unsigned int OptLevel = 3;

static llvm::cl::opt<bool> Fast("fast",
                   llvm::cl::desc("Generate code quickly, "
                            "potentially sacrificing code quality"),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> OpCodeStats("opcode-stats",
                   llvm::cl::desc("Generate stats on compiled Lua opcodes."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> RunOpCodeStats("runtime-opcode-stats",
                   llvm::cl::desc("Generate stats on executed Lua opcodes."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> PrintRunOpCodes("print-runtime-opcodes",
                   llvm::cl::desc("Print each opcode before executing it."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> CompileLargeFunctions("compile-large-functions",
                   llvm::cl::desc("Compile all Lua functions even really large functions."),
                   llvm::cl::init(false));

static llvm::cl::opt<int> MaxFunctionSize("max-func-size",
                   llvm::cl::desc("Functions larger then this will not be compiled."),
                   llvm::cl::value_desc("int"),
                   llvm::cl::init(200));

static llvm::cl::opt<bool> DontInlineOpcodes("do-not-inline-opcodes",
                   llvm::cl::desc("Turn off inlining of opcode functions."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> VerifyFunctions("verify-functions",
                   llvm::cl::desc("Verify each compiled function."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> DumpFunctions("dump-functions",
                   llvm::cl::desc("Dump LLVM IR for each function."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> DebugOpCodes("g",
                   llvm::cl::desc("Allow debugging of Lua code."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> DisableOpt("O0",
                   llvm::cl::desc("Disable optimizations."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> OptLevelO1("O1",
                   llvm::cl::desc("Optimization level 1."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> OptLevelO2("O2",
                   llvm::cl::desc("Optimization level 2."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> OptLevelO3("O3",
                   llvm::cl::desc("Optimization level 3."),
                   llvm::cl::init(false));

#define BRANCH_COND -1
#define BRANCH_NONE -2

class OPValues {
private:
	int len;
	llvm::Value **values;

public:
	OPValues(int len) : len(len), values(new llvm::Value *[len]) {
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

//===----------------------------------------------------------------------===//
// Lua bytecode to LLVM IR compiler
//===----------------------------------------------------------------------===//

const llvm::Type *LLVMCompiler::get_var_type(val_t type, hint_t hints) {
	switch(type) {
	case VAR_T_VOID:
		return llvm::Type::getVoidTy(getCtx());
	case VAR_T_INT:
	case VAR_T_ARG_A:
	case VAR_T_ARG_B:
	case VAR_T_ARG_BK:
	case VAR_T_ARG_Bx:
	case VAR_T_ARG_Bx_NUM_CONSTANT:
	case VAR_T_ARG_B_FB2INT:
	case VAR_T_ARG_sBx:
	case VAR_T_ARG_C:
	case VAR_T_ARG_CK:
	case VAR_T_ARG_C_NUM_CONSTANT:
	case VAR_T_ARG_C_NEXT_INSTRUCTION:
	case VAR_T_ARG_C_FB2INT:
	case VAR_T_PC_OFFSET:
	case VAR_T_INSTRUCTION:
	case VAR_T_NEXT_INSTRUCTION:
		return llvm::Type::getInt32Ty(getCtx());
	case VAR_T_LUA_STATE_PTR:
		return Ty_lua_State_ptr;
	case VAR_T_K:
		return Ty_TValue_ptr;
	case VAR_T_CL:
		return Ty_LClosure_ptr;
	case VAR_T_OP_VALUE_0:
	case VAR_T_OP_VALUE_1:
	case VAR_T_OP_VALUE_2:
		if(hints & HINT_USE_LONG) {
			return llvm::Type::getInt64Ty(getCtx());
		}
		return llvm::Type::getDoubleTy(getCtx());
	default:
		fprintf(stderr, "Error: missing var_type=%d\n", type);
		exit(1);
		break;
	}
	return NULL;
}

llvm::Value *LLVMCompiler::get_proto_constant(TValue *constant) {
	llvm::Value *val = NULL;
	switch(ttype(constant)) {
	case LUA_TBOOLEAN:
		val = llvm::ConstantInt::get(getCtx(), llvm::APInt(sizeof(LUA_NUMBER), !l_isfalse(constant)));
		break;
	case LUA_TNUMBER:
		val = llvm::ConstantFP::get(getCtx(), llvm::APFloat(nvalue(constant)));
		break;
	case LUA_TSTRING:
		break;
	case LUA_TNIL:
	default:
		break;
	}
	return val;
}

LLVMCompiler::LLVMCompiler(int useJIT) {
	std::string error;
	llvm::Timer load_ops("load_ops");
	llvm::Timer load_jit("load_jit");
	llvm::FunctionType *func_type;
	llvm::Function *func;
	std::vector<const llvm::Type*> func_args;
	const vm_func_info *func_info;
	int opcode;

	// set OptLevel
	if(OptLevelO1) OptLevel = 1;
	if(OptLevelO2) OptLevel = 2;
	if(OptLevelO3) OptLevel = 3;
	if(DisableOpt) OptLevel = 0;
	// create timers.
	lua_to_llvm = new llvm::Timer("lua_to_llvm");
	codegen = new llvm::Timer("codegen");
	strip_code = false;

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
	MP = load_vm_ops(getCtx(), NoLazyCompilation);
	M = MP->getModule();

	// get important struct types.
	Ty_TValue = M->getTypeByName("struct.lua_TValue");
	if(Ty_TValue == NULL) {
		Ty_TValue = M->getTypeByName("struct.TValue");
	}
	Ty_TValue_ptr = llvm::PointerType::getUnqual(Ty_TValue);
	Ty_LClosure = M->getTypeByName("struct.LClosure");
	Ty_LClosure_ptr = llvm::PointerType::getUnqual(Ty_LClosure);
	Ty_lua_State = M->getTypeByName("struct.lua_State");
	Ty_lua_State_ptr = llvm::PointerType::getUnqual(Ty_lua_State);
	// setup argument lists.
	func_args.clear();
	func_args.push_back(Ty_lua_State_ptr);
	lua_func_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(getCtx()), func_args, false);
	// define extern vm_next_OP
	vm_next_OP = M->getFunction("vm_next_OP");
	if(vm_next_OP == NULL) {
		func_args.clear();
		func_args.push_back(Ty_lua_State_ptr);
		func_args.push_back(Ty_LClosure_ptr);
		func_args.push_back(llvm::Type::getInt32Ty(getCtx()));
		func_type = llvm::FunctionType::get(llvm::Type::getVoidTy(getCtx()), func_args, false);
		vm_next_OP = llvm::Function::Create(func_type,
			llvm::Function::ExternalLinkage, "vm_next_OP", M);
	}
	// define extern vm_print_OP
	vm_print_OP = M->getFunction("vm_print_OP");
	if(vm_print_OP == NULL) {
		func_args.clear();
		func_args.push_back(Ty_lua_State_ptr);
		func_args.push_back(Ty_LClosure_ptr);
		func_args.push_back(llvm::Type::getInt32Ty(getCtx()));
		func_args.push_back(llvm::Type::getInt32Ty(getCtx()));
		func_type = llvm::FunctionType::get(llvm::Type::getVoidTy(getCtx()), func_args, false);
		vm_print_OP = llvm::Function::Create(func_type,
			llvm::Function::ExternalLinkage, "vm_print_OP", M);
	}
	// function for counting each executed op.
	if(RunOpCodeStats) {
		vm_count_OP = M->getFunction("vm_count_OP");
		if(vm_count_OP == NULL) {
			func_args.clear();
			func_args.push_back(llvm::Type::getInt32Ty(getCtx()));
			func_type = llvm::FunctionType::get(llvm::Type::getVoidTy(getCtx()), func_args, false);
			vm_count_OP = llvm::Function::Create(func_type,
				llvm::Function::ExternalLinkage, "vm_count_OP", M);
		}
		for(int i = 0; i < NUM_OPCODES; i++) {
			vm_op_run_count[i] = 0;
		}
	}
	// define extern vm_mini_vm
	vm_mini_vm = M->getFunction("vm_mini_vm");
	if(vm_mini_vm == NULL) {
		func_args.clear();
		func_args.push_back(Ty_lua_State_ptr);
		func_args.push_back(Ty_LClosure_ptr);
		func_args.push_back(llvm::Type::getInt32Ty(getCtx()));
		func_args.push_back(llvm::Type::getInt32Ty(getCtx()));
		func_type = llvm::FunctionType::get(llvm::Type::getVoidTy(getCtx()), func_args, false);
		vm_mini_vm = llvm::Function::Create(func_type,
			llvm::Function::ExternalLinkage, "vm_mini_vm", M);
	}
	// define extern vm_get_current_closure
	vm_get_current_closure = M->getFunction("vm_get_current_closure");
	// define extern vm_get_current_constants
	vm_get_current_constants = M->getFunction("vm_get_current_constants");
	// define extern vm_get_number
	vm_get_number = M->getFunction("vm_get_number");
	// define extern vm_get_long
	vm_get_long = M->getFunction("vm_get_long");
	// define extern vm_set_number
	vm_set_number = M->getFunction("vm_set_number");
	// define extern vm_set_long
	vm_set_long = M->getFunction("vm_set_long");


	// create prototype for vm_* functions.
	vm_op_funcs = new OPFunc *[NUM_OPCODES];
	for(int i = 0; i < NUM_OPCODES; i++) vm_op_funcs[i] = NULL; // clear list.
	for(int i = 0; true; i++) {
		func_info = &vm_op_functions[i];
		opcode = func_info->opcode;
		if(opcode < 0) break;
		vm_op_funcs[opcode] = new OPFunc(func_info, vm_op_funcs[opcode]);
		func = M->getFunction(func_info->name);
		if(func != NULL) {
			vm_op_funcs[opcode]->func = func;
			vm_op_funcs[opcode]->compiled = !useJIT; // lazy compile ops when JIT is enabled.
			continue;
		}
		func_args.clear();
		for(int x = 0; func_info->params[x] != VAR_T_VOID; x++) {
			func_args.push_back(get_var_type(func_info->params[x], func_info->hint));
		}
		func_type = llvm::FunctionType::get(
			get_var_type(func_info->ret_type, func_info->hint), func_args, false);
		func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
							func_info->name, M);
		vm_op_funcs[opcode]->func = func;
		vm_op_funcs[opcode]->compiled = true; // built-in op function.
	}

	if(llvm::TimePassesIsEnabled) load_ops.stopTimer();

	if(llvm::TimePassesIsEnabled) load_jit.startTimer();
	// Create the JIT.
	if(useJIT) {
		llvm::EngineBuilder engine(MP);
		llvm::CodeGenOpt::Level optLevel = llvm::CodeGenOpt::Aggressive;
		if(Fast) {
			optLevel = llvm::CodeGenOpt::Default;
		}
#ifdef LUA_CPP_SUPPORT
		llvm::ExceptionHandling = true; 
#endif
		engine.setErrorStr(&error);
		engine.setOptLevel(optLevel);
		TheExecutionEngine = engine.create();
		if(!TheExecutionEngine) {
			printf("Error creating JIT engine: %s\n", error.c_str());
		}
		if (NoLazyCompilation)
			TheExecutionEngine->DisableLazyCompilation();

		TheExecutionEngine->runStaticConstructorsDestructors(false);

		if (NoLazyCompilation) {
			TheExecutionEngine->getPointerToFunction(vm_get_current_closure);
			TheExecutionEngine->getPointerToFunction(vm_get_current_constants);
			TheExecutionEngine->getPointerToFunction(vm_get_number);
			TheExecutionEngine->getPointerToFunction(vm_get_long);
			TheExecutionEngine->getPointerToFunction(vm_set_number);
			TheExecutionEngine->getPointerToFunction(vm_set_long);
		}
	} else {
		TheExecutionEngine = NULL;
	}

	if(OptLevel > 1) {
		TheFPM = new llvm::FunctionPassManager(MP);
		
		/*
		 * Function Pass Manager.
		 */
		// Set up the optimizer pipeline.  Start with registering info about how the
		// target lays out data structures.
		if(useJIT) {
			TheFPM->add(new llvm::TargetData(*TheExecutionEngine->getTargetData()));
		} else {
			TheFPM->add(new llvm::TargetData(M));
		}
		// mem2reg
		TheFPM->add(llvm::createPromoteMemoryToRegisterPass());
		// Do simple "peephole" optimizations and bit-twiddling optzns.
		TheFPM->add(llvm::createInstructionCombiningPass());
		// Dead code Elimination
		TheFPM->add(llvm::createDeadCodeEliminationPass());
		if(OptLevel > 2) {
			// TailDuplication
			TheFPM->add(llvm::createTailDuplicationPass());
			// BlockPlacement
			TheFPM->add(llvm::createBlockPlacementPass());
			// Reassociate expressions.
			TheFPM->add(llvm::createReassociatePass());
			// Simplify the control flow graph (deleting unreachable blocks, etc).
			TheFPM->add(llvm::createCFGSimplificationPass());
		}
	} else {
		TheFPM = NULL;
	}
	
	if(llvm::TimePassesIsEnabled) load_jit.stopTimer();
}

void print_opcode_stats(int *stats, const char *stats_name) {
	int order[NUM_OPCODES];
	int max=0;
	int width=1;
	for(int opcode = 0; opcode < NUM_OPCODES; opcode++) {
		order[opcode] = opcode;
		if(max < stats[opcode]) max = stats[opcode];
		for(int n = 0; n < opcode; n++) {
			if(stats[opcode] >= stats[order[n]]) {
				// insert here.
				memmove(&order[n + 1], &order[n], (opcode - n) * sizeof(int));
				order[n] = opcode;
				break;
			}
		}
	}
	// calc width.
	while(max >= 10) { width++; max /= 10; }
	// sort by count.
	fprintf(stderr, "===================== %s =======================\n", stats_name);
	int opcode;
	for(int i = 0; i < NUM_OPCODES; i++) {
		opcode = order[i];
		if(stats[opcode] == 0) continue;
		fprintf(stderr, "%*d: %s(%d)\n", width, stats[opcode], luaP_opnames[opcode], opcode);
	}
}

LLVMCompiler::~LLVMCompiler() {
	llvm::Module *mod = NULL;
	std::string error;
	// print opcode stats.
	if(OpCodeStats) {
		print_opcode_stats(opcode_stats, "Compiled OpCode counts");
		delete opcode_stats;
	}
	if(RunOpCodeStats) {
		print_opcode_stats(vm_op_run_count, "Compiled OpCode counts");
	}

	delete lua_to_llvm;
	delete codegen;
	if(TheFPM) delete TheFPM;
	TheFPM = NULL;

	for(int i = 0; i < NUM_OPCODES; i++) {
		if(vm_op_funcs[i]) delete vm_op_funcs[i];
	}
	delete[] vm_op_funcs;

	// Print out all of the generated code.
	//M->dump();

	if(TheExecutionEngine) {
		TheExecutionEngine->freeMachineCodeForFunction(vm_get_current_closure);
		TheExecutionEngine->freeMachineCodeForFunction(vm_get_current_constants);
		TheExecutionEngine->freeMachineCodeForFunction(vm_get_number);
		TheExecutionEngine->freeMachineCodeForFunction(vm_get_long);
		TheExecutionEngine->freeMachineCodeForFunction(vm_set_number);
		TheExecutionEngine->freeMachineCodeForFunction(vm_set_long);
		TheExecutionEngine->runStaticConstructorsDestructors(true);
		mod = TheExecutionEngine->removeModuleProvider(MP, &error);
		if(!mod) {
			printf("Failed cleanup ModuleProvider: %s\n", error.c_str());
			exit(1);
		}
		delete TheExecutionEngine;
	} else if(MP) {
		mod = MP->releaseModule(&error);
		if(!mod) {
			printf("Failed cleanup ModuleProvider: %s\n", error.c_str());
			exit(1);
		}
		delete MP;
		MP = NULL;
	}
	if(mod) {
		delete mod;
	}
}

/*
 * Pre-Compile all loaded functions.
 */
void LLVMCompiler::compileAll(lua_State *L, Proto *parent) {
	int i;
	/* pre-compile parent */
	compile(L, parent);
	/* pre-compile all children */
	for(i = 0; i < parent->sizep; i++) {
		compileAll(L, parent->p[i]);
	}
}

void LLVMCompiler::compile(lua_State *L, Proto *p)
{
	Instruction *code=p->code;
	TValue *k=p->k;
	int code_len=p->sizecode;
	OPFunc *opfunc;
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
	//char locals[LUAI_MAXVARS];
	bool inline_call=false;
	int strip_ops=0;
	int branch;
	Instruction op_intr;
	int opcode;
	int mini_op_repeat=0;
	int i;
	llvm::IRBuilder<> Builder(getCtx());

	// don't JIT large functions.
	if(code_len >= MaxFunctionSize && TheExecutionEngine != NULL && !CompileLargeFunctions) {
		return;
	}

	if(llvm::TimePassesIsEnabled) lua_to_llvm->startTimer();
	// create function.
	name = getstr(p->source);
	if(name.size() > 32) {
		name = name.substr(0,32);
	}
	// replace non-alphanum characters with '_'
	for(size_t n = 0; n < name.size(); n++) {
		char c = name[n];
		if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
			continue;
		}
		if(c == '\n' || c == '\r') {
			name = name.substr(0,n);
			break;
		}
		name[n] = '_';
	}
	snprintf(tmp,128,"_%d_%d",p->linedefined, p->lastlinedefined);
	name += tmp;
	func = llvm::Function::Create(lua_func_type, llvm::Function::ExternalLinkage, name, M);
	// name arg1 = "L"
	func_L = func->arg_begin();
	func_L->setName("L");
	// entry block
	entry_block = llvm::BasicBlock::Create(getCtx(),"entry", func);
	Builder.SetInsertPoint(entry_block);
	// get LClosure & constants.
	call=Builder.CreateCall(vm_get_current_closure, func_L);
	inlineList.push_back(call);
	func_cl=call;
	call=Builder.CreateCall(vm_get_current_constants, func_cl);
	inlineList.push_back(call);
	func_k=call;

	// pre-create basic blocks.
	hint_t op_hints[code_len];
	OPValues *op_values[code_len];
	llvm::BasicBlock *op_blocks[code_len];
	bool need_op_block[code_len];
	for(i = 0; i < code_len; i++) {
		need_op_block[i] = false;
		op_values[i] = NULL;
		op_hints[i] = HINT_NONE;
	}
	// find all jump/branch destinations and create a new basic block at that opcode.
	// also build hints for some opcodes.
	for(i = 0; i < code_len; i++) {
		op_intr=code[i];
		opcode = GET_OPCODE(op_intr);
		// combind simple ops into one function call.
		if(is_mini_vm_op(opcode)) {
			mini_op_repeat++;
		} else {
			if(mini_op_repeat >= 3 && OptLevel > 1) {
				op_hints[i - mini_op_repeat] |= HINT_MINI_VM;
			}
			mini_op_repeat = 0;
		}
		switch (opcode) {
			case OP_LOADBOOL:
				branch = i+1;
				// check C operand if C!=0 then skip over the next op_block.
				if(GETARG_C(op_intr) != 0)
					++branch;
				need_op_block[branch] = true;
				break;
			case OP_LOADK: {
				// check if arg Bx is a number constant.
				TValue *rb = k + INDEXK(GETARG_Bx(op_intr));
				if(ttisnumber(rb)) op_hints[i] |= HINT_Bx_NUM_CONSTANT;
				break;
			}
			case OP_JMP:
				// always branch to the offset stored in operand sBx
				branch = i + 1 + GETARG_sBx(op_intr);
				need_op_block[branch] = true;
				break;
			case OP_TAILCALL:
				branch = i+1;
				need_op_block[0] = true; /* branch to start of function if this is a recursive tail-call. */
				need_op_block[branch] = true; /* branch to return instruction if not recursive. */
				break;
			case OP_EQ:
			case OP_LT:
			case OP_LE:
				// check if arg C is a number constant.
				if(ISK(GETARG_C(op_intr))) {
					TValue *rc = k + INDEXK(GETARG_C(op_intr));
					if(ttisnumber(rc)) op_hints[i] |= HINT_C_NUM_CONSTANT;
				}
				if(GETARG_A(op_intr) == 1) {
					op_hints[i] |= HINT_NOT;
				}
				// fall-through
			case OP_TEST:
			case OP_TESTSET:
			case OP_TFORLOOP:
				branch = ++i + 1;
				op_intr=code[i];
				need_op_block[branch + GETARG_sBx(op_intr)] = true; /* inline JMP op. */
				need_op_block[branch] = true;
				break;
			case OP_FORLOOP:
				branch = i+1;
				need_op_block[branch] = true;
				branch += GETARG_sBx(op_intr);
				need_op_block[branch] = true;
				break;
			case OP_FORPREP:
				branch = i + 1 + GETARG_sBx(op_intr);
				need_op_block[branch] = true;
				need_op_block[branch + 1] = true;
				// test if init/plimit/pstep are number constants.
				if(OptLevel > 1 && i >= 3) {
					lua_Number nums[3];
					bool found_val[3] = { false, false , false };
					bool is_const_num[3] = { false, false, false };
					bool all_longs=true;
					int found=0;
					OPValues *vals = new OPValues(4);
					int forprep_ra = GETARG_A(op_intr);
					bool no_jmp_end_point = true; // don't process ops after finding a jmp end point.
					// find & load constants for init/plimit/pstep
					for(int x = 1; x < 6 && found < 3 && no_jmp_end_point; ++x) {
						const TValue *tmp;
						Instruction op_intr2;
						int ra;

						if((i - x) < 0) break;
						op_intr2 = code[i - x];
						// get 'a' register.
						ra = GETARG_A(op_intr2);
						ra -= forprep_ra;
						// check for jmp end-point.
						no_jmp_end_point = !(need_op_block[i - x]);
						// check that the 'a' register is for one of the value we are interested in.
						if(ra < 0 || ra > 2) continue;
						// only process this opcode if we haven't seen this value before.
						if(found_val[ra]) continue;
						found_val[ra] = true;
						found++;
						if(GET_OPCODE(op_intr2) == OP_LOADK) {
							tmp = k + GETARG_Bx(op_intr2);
							if(ttisnumber(tmp)) {
								lua_Number num=nvalue(tmp);
								nums[ra] = num;
								// test if number is a whole number
								all_longs &= (floor(num) == num);
								vals->set(ra,llvm::ConstantFP::get(getCtx(), llvm::APFloat(num)));
								is_const_num[ra] = true;
								op_hints[i - x] |= HINT_SKIP_OP;
								continue;
							}
						}
						all_longs = false;
					}
					all_longs &= (found == 3);
					// create for_idx OP_FORPREP will inialize it.
					op_hints[branch] = HINT_FOR_N_N_N;
					if(all_longs) {
						vals->set(3, Builder.CreateAlloca(llvm::Type::getInt64Ty(getCtx()), 0, "for_idx"));
						op_hints[branch] |= HINT_USE_LONG;
					} else {
						vals->set(3, Builder.CreateAlloca(llvm::Type::getDoubleTy(getCtx()), 0, "for_idx"));
					}
					op_values[branch] = vals;
					// check if step, init, limit are constants
					if(is_const_num[2]) {
						// step is a constant
						if(is_const_num[0]) {
							// init & step are constants.
							if(is_const_num[1]) {
								// all are constants.
								op_hints[i] = HINT_FOR_N_N_N;
							} else {
								// limit is variable.
								op_hints[i] = HINT_FOR_N_M_N;
							}
							op_values[i] = new OPValues(3);
							op_values[i]->set(0, vals->get(0));
							op_values[i]->set(2, vals->get(2));
						} else if(is_const_num[1]) {
							// init is variable, limit & step are constants.
							op_hints[i] = HINT_FOR_M_N_N;
							op_values[i] = new OPValues(3);
							op_values[i]->set(1, vals->get(1));
							op_values[i]->set(2, vals->get(2));
						}
						// check the direct of step.
						if(nums[2] > 0) {
							op_hints[branch] |= HINT_UP;
						} else {
							op_hints[branch] |= HINT_DOWN;
						}
					}
					if(op_hints[i] == HINT_NONE) {
						// don't skip LOADK ops, since we are not inlining them.
						for(int x=i-3; x < i; x++) {
							if(op_hints[x] & HINT_SKIP_OP) op_hints[x] &= ~(HINT_SKIP_OP);
						}
					}
					if(all_longs) {
						for(int x = 0; x < 3; ++x) {
							vals->set(x,llvm::ConstantInt::get(getCtx(), llvm::APInt(64,(lua_Long)nums[x])));
						}
					}
					// make sure OP_FORPREP doesn't subtract 'step' from 'init'
					op_hints[i] |= HINT_NO_SUB;
				}
				break;
			case OP_SETLIST:
				// if C == 0, then next code value is count value.
				if(GETARG_C(op_intr) == 0) {
					i++;
				}
				break;
			case OP_ADD:
			case OP_SUB:
			case OP_MUL:
			case OP_DIV:
			case OP_MOD:
			case OP_POW:
				// check if arg C is a number constant.
				if(ISK(GETARG_C(op_intr))) {
					TValue *rc = k + INDEXK(GETARG_C(op_intr));
					if(ttisnumber(rc)) op_hints[i] |= HINT_C_NUM_CONSTANT;
				}
				break;
			default:
				break;
		}
		// update local variable type hints.
		//vm_op_hint_locals(locals, p->maxstacksize, k, op_intr);
	}
	for(i = 0; i < code_len; i++) {
		if(need_op_block[i]) {
			op_intr=code[i];
			opcode = GET_OPCODE(op_intr);
			snprintf(tmp,128,"op_block_%s_%d",luaP_opnames[opcode],i);
			op_blocks[i] = llvm::BasicBlock::Create(getCtx(),tmp, func);
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
			if(strip_code) strip_ops++;
			continue;
		}
		branch = i+1;
		op_intr=code[i];
		opcode = GET_OPCODE(op_intr);
		opfunc = vm_op_funcs[opcode];
		// combind multiple simple ops into one call.
		if(op_hints[i] & HINT_MINI_VM) {
			int op_count = 1;
			// count mini ops and check for any branch end-points.
			while(is_mini_vm_op(GET_OPCODE(code[i + op_count])) &&
					(op_hints[i + op_count] & HINT_SKIP_OP) == 0) {
				// branch end-point in middle of mini ops block.
				if(need_op_block[i + op_count]) {
					op_hints[i + op_count] |= HINT_MINI_VM; // mark start of new mini vm ops.
					break;
				}
				op_count++;
			}
			if(op_count >= 3) {
				// large block of mini ops add function call to vm_mini_vm()
				Builder.CreateCall4(vm_mini_vm, func_L, func_cl,
					llvm::ConstantInt::get(getCtx(), llvm::APInt(32,op_count)),
					llvm::ConstantInt::get(getCtx(), llvm::APInt(32,i - strip_ops)));
				if(strip_code && strip_ops > 0) {
					while(op_count > 0) {
						code[i - strip_ops] = code[i];
						i++;
						op_count--;
					}
				} else {
					i += op_count;
				}
				i--;
				continue;
			} else {
				// mini ops block too small.
				op_hints[i] &= ~(HINT_MINI_VM);
			}
		}
		// find op function with matching hint.
		while(opfunc->next != NULL && (opfunc->info->hint & op_hints[i]) != opfunc->info->hint) {
			opfunc = opfunc->next;
		}
		if(OpCodeStats) {
			opcode_stats[opcode]++;
		}
		//fprintf(stderr, "%d: '%s' (%d) = 0x%08X, hint=0x%X\n", i, luaP_opnames[opcode], opcode, op_intr, op_hints[i]);
		//fprintf(stderr, "%d: func: '%s', func hints=0x%X\n", i, opfunc->info->name,opfunc->info->hint);
		if(PrintRunOpCodes) {
			Builder.CreateCall4(vm_print_OP, func_L, func_cl,
				llvm::ConstantInt::get(getCtx(), llvm::APInt(32,op_intr)),
				llvm::ConstantInt::get(getCtx(), llvm::APInt(32,i)));
		}
		if(RunOpCodeStats) {
			Builder.CreateCall(vm_count_OP, llvm::ConstantInt::get(getCtx(), llvm::APInt(32,op_intr)));
		}
		if(DebugOpCodes) {
			/* vm_next_OP function is used to call count/line debug hooks. */
			Builder.CreateCall3(vm_next_OP, func_L, func_cl, llvm::ConstantInt::get(getCtx(), llvm::APInt(32,i)));
		}
		if(op_hints[i] & HINT_SKIP_OP) {
			if(strip_code) strip_ops++;
			continue;
		}
		if(strip_code) {
			// strip all opcodes.
			strip_ops++;
			if(strip_ops > 0 && strip_ops < (i+1)) {
				// move opcodes we want to keep to new position.
				code[(i+1) - strip_ops] = op_intr;
			}
		}
		// setup arguments for opcode function.
		func_info = opfunc->info;
		if(func_info == NULL) {
			fprintf(stderr, "Error missing vm_OP_* function for opcode: %d\n", opcode);
			return;
		}
		// special handling of OP_FORLOOP
		if(opcode == OP_FORLOOP) {
			llvm::BasicBlock *loop_test;
			llvm::BasicBlock *prep_block;
			llvm::BasicBlock *incr_block;
			llvm::Value *init,*step,*idx_var,*cur_idx,*next_idx;
			llvm::PHINode *PN;
			OPValues *vals;

			vals=op_values[i];
			if(vals != NULL) {
				// get init value from forprep block
				init = vals->get(0);
				// get for loop 'idx' variable.
				step = vals->get(2);
				idx_var = vals->get(3);
				assert(idx_var != NULL);
				incr_block = current_block;
				cur_idx = Builder.CreateLoad(idx_var);
				next_idx = Builder.CreateAdd(cur_idx, step, "next_idx");
				Builder.CreateStore(next_idx, idx_var); // store 'for_init' value.
				// create extra BasicBlock for vm_OP_FORLOOP_*
				snprintf(tmp,128,"op_block_%s_%d_loop_test",luaP_opnames[opcode],i);
				loop_test = llvm::BasicBlock::Create(getCtx(),tmp, func);
				// create unconditional jmp from current block to loop test block
				Builder.CreateBr(loop_test);
				// create unconditional jmp from forprep block to loop test block
				prep_block = op_blocks[branch + GETARG_sBx(op_intr) - 1];
				Builder.SetInsertPoint(prep_block);
				Builder.CreateBr(loop_test);
				// set current_block to loop_test block
				current_block = loop_test;
				Builder.SetInsertPoint(current_block);
				// Emit merge block
				if(op_hints[i] & HINT_USE_LONG) {
					PN = Builder.CreatePHI(llvm::Type::getInt64Ty(getCtx()), "idx");
				} else {
					PN = Builder.CreatePHI(llvm::Type::getDoubleTy(getCtx()), "idx");
				}
				PN->addIncoming(init, prep_block);
				PN->addIncoming(next_idx, incr_block);
				vals->set(0, PN);
			}
		}
		args.clear();
		for(int x = 0; func_info->params[x] != VAR_T_VOID ; x++) {
			llvm::Value *val=NULL;
			switch(func_info->params[x]) {
			case VAR_T_ARG_A:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,GETARG_A(op_intr)));
				break;
			case VAR_T_ARG_C:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,GETARG_C(op_intr)));
				break;
			case VAR_T_ARG_C_FB2INT:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,luaO_fb2int(GETARG_C(op_intr))));
				break;
			case VAR_T_ARG_Bx_NUM_CONSTANT:
				val = get_proto_constant(k + INDEXK(GETARG_Bx(op_intr)));
				break;
			case VAR_T_ARG_C_NUM_CONSTANT:
				val = get_proto_constant(k + INDEXK(GETARG_C(op_intr)));
				break;
			case VAR_T_ARG_C_NEXT_INSTRUCTION: {
				int c = GETARG_C(op_intr);
				// if C == 0, then next code value is used as ARG_C.
				if(c == 0) {
					if((i+1) < code_len) {
						c = code[i+1];
						if(strip_code) strip_ops++;
					}
				}
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,c));
				break;
			}
			case VAR_T_ARG_B:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,GETARG_B(op_intr)));
				break;
			case VAR_T_ARG_B_FB2INT:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,luaO_fb2int(GETARG_B(op_intr))));
				break;
			case VAR_T_ARG_Bx:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,GETARG_Bx(op_intr)));
				break;
			case VAR_T_ARG_sBx:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,GETARG_sBx(op_intr)));
				break;
			case VAR_T_PC_OFFSET:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,i + 1 - strip_ops));
				break;
			case VAR_T_INSTRUCTION:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,op_intr));
				break;
			case VAR_T_NEXT_INSTRUCTION:
				val = llvm::ConstantInt::get(getCtx(), llvm::APInt(32,code[i+1]));
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
			case VAR_T_OP_VALUE_0:
				if(op_values[i] != NULL) val = op_values[i]->get(0);
				break;
			case VAR_T_OP_VALUE_1:
				if(op_values[i] != NULL) val = op_values[i]->get(1);
				break;
			case VAR_T_OP_VALUE_2:
				if(op_values[i] != NULL) val = op_values[i]->get(2);
				break;
			default:
				fprintf(stderr, "Error: not implemented!\n");
				return;
			case VAR_T_VOID:
				fprintf(stderr, "Error: invalid value type!\n");
				return;
			}
			if(val == NULL) {
				fprintf(stderr, "Error: Missing parameter '%d' for this opcode(%d) function=%s!\n", x,
					opcode, func_info->name);
				exit(1);
			}
			args.push_back(val);
		}
		// create call to opcode function.
		if(func_info->ret_type != VAR_T_VOID) {
			call=Builder.CreateCall(opfunc->func, args.begin(), args.end(), "retval");
		} else {
			call=Builder.CreateCall(opfunc->func, args.begin(), args.end());
		}
		inline_call = false;
		// handle retval from opcode function.
		switch (opcode) {
			case OP_LOADBOOL:
				// check C operand if C!=0 then skip over the next op_block.
				if(GETARG_C(op_intr) != 0) branch += 1;
				else branch = BRANCH_NONE;
				inline_call = true;
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
			case OP_GETUPVAL:
			case OP_MOVE:
				inline_call = true;
				branch = BRANCH_NONE;
				break;
			case OP_CLOSE:
			case OP_SETUPVAL:
				inline_call = false;
				branch = BRANCH_NONE;
				break;
			case OP_VARARG:
			case OP_CALL:
				branch = BRANCH_NONE;
				break;
			case OP_TAILCALL:
				//call->setTailCall(true);
				brcond=call;
				brcond=Builder.CreateICmpNE(brcond,
						llvm::ConstantInt::get(getCtx(), llvm::APInt(32, PCRTAILRECUR)), "brcond");
				i++; // skip return opcode.
				false_block = op_blocks[0]; // branch to start of function if this is a recursive tail-call.
				true_block = op_blocks[i]; // branch to return instruction if not recursive.
				Builder.CreateCondBr(brcond, true_block, false_block);
				Builder.SetInsertPoint(op_blocks[i]);
				Builder.CreateRet(call);
				current_block = NULL; // have terminator
				branch = BRANCH_NONE;
				break;
			case OP_JMP:
				// always branch to the offset stored in operand sBx
				branch += GETARG_sBx(op_intr);
				// call vm_OP_JMP just in case luai_threadyield is defined.
				inline_call = true;
				break;
			case OP_EQ:
			case OP_LT:
			case OP_LE:
			case OP_TEST:
			case OP_TESTSET:
				inline_call = true;
			case OP_TFORLOOP:
				brcond=call;
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(getCtx(), llvm::APInt(32,0)), "brcond");
				false_block=op_blocks[branch+1];
				/* inlined JMP op. */
				branch = ++i + 1;
				if(strip_code) {
					strip_ops++;
					if(strip_ops > 0 && strip_ops < (i+1)) {
						// move opcodes we want to keep to new position.
						code[(i+1) - strip_ops] = code[i];
					}
				}
				op_intr=code[i];
				branch += GETARG_sBx(op_intr);
				true_block=op_blocks[branch];
				branch = BRANCH_COND; // do conditional branch
				break;
			case OP_FORLOOP: {
				llvm::Function *set_func=vm_set_number;
				llvm::CallInst *call2;
				OPValues *vals;

				inline_call = true;
				brcond=call;
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(getCtx(), llvm::APInt(32,0)), "brcond");
				true_block=op_blocks[branch + GETARG_sBx(op_intr)];
				false_block=op_blocks[branch];
				branch = BRANCH_COND; // do conditional branch

				// update external index if needed.
				vals=op_values[i];
				if(vals != NULL) {
					llvm::BasicBlock *idx_block;
					if(op_hints[i] & HINT_USE_LONG) {
						set_func = vm_set_long;
					}
					// create extra BasicBlock
					snprintf(tmp,128,"op_block_%s_%d_set_for_idx",luaP_opnames[opcode],i);
					idx_block = llvm::BasicBlock::Create(getCtx(),tmp, func);
					Builder.SetInsertPoint(idx_block);
					// copy idx value to Lua-stack.
					call2=Builder.CreateCall3(set_func,func_L,
						llvm::ConstantInt::get(getCtx(), llvm::APInt(32,(GETARG_A(op_intr) + 3))), vals->get(0));
					inlineList.push_back(call2);
					// create jmp to true_block
					Builder.CreateBr(true_block);
					true_block = idx_block;
					Builder.SetInsertPoint(current_block);
				}
				break;
			}
			case OP_FORPREP: {
				llvm::Function *get_func=vm_get_number;
				llvm::Value *idx_var,*init;
				llvm::CallInst *call2;
				OPValues *vals;

				//inline_call = true;
				op_blocks[i] = current_block;
				branch += GETARG_sBx(op_intr);
				vals=op_values[branch];
				// if no saved value, then use slow method.
				if(vals == NULL) break;
				if(op_hints[branch] & HINT_USE_LONG) {
					get_func = vm_get_long;
				}
				// get non-constant init from Lua stack.
				if(vals->get(0) == NULL) {
					call2=Builder.CreateCall2(get_func,func_L,
						llvm::ConstantInt::get(getCtx(), llvm::APInt(32,(GETARG_A(op_intr) + 0))), "for_init");
					inlineList.push_back(call2);
					vals->set(0, call2);
				}
				init = vals->get(0);
				// get non-constant limit from Lua stack.
				if(vals->get(1) == NULL) {
					call2=Builder.CreateCall2(get_func,func_L,
						llvm::ConstantInt::get(getCtx(), llvm::APInt(32,(GETARG_A(op_intr) + 1))), "for_limit");
					inlineList.push_back(call2);
					vals->set(1, call2);
				}
				// get non-constant step from Lua stack.
				if(vals->get(2) == NULL) {
					call2=Builder.CreateCall2(get_func,func_L,
						llvm::ConstantInt::get(getCtx(), llvm::APInt(32,(GETARG_A(op_intr) + 2))), "for_step");
					inlineList.push_back(call2);
					vals->set(2, call2);
				}
				// get for loop 'idx' variable.
				assert(vals->get(3) != NULL);
				idx_var = vals->get(3);
				Builder.CreateStore(init, idx_var); // store 'for_init' value.
				vals->set(0, init);
				current_block = NULL; // have terminator
				branch = BRANCH_NONE;
				break;
			}
			case OP_SETLIST:
				// if C == 0, then next code value is used as ARG_C.
				if(GETARG_C(op_intr) == 0) {
					i++;
				}
				branch = BRANCH_NONE;
				break;
			case OP_CLOSURE: {
				Proto *p2 = p->p[GETARG_Bx(op_intr)];
				int nups = p2->nups;
				if(strip_code && strip_ops > 0) {
					while(nups > 0) {
						i++;
						code[i - strip_ops] = code[i];
						nups--;
					}
				} else {
					i += nups;
				}
				branch = BRANCH_NONE;
				break;
			}
			case OP_RETURN: {
				call->setTailCall(true);
				Builder.CreateRet(call);
				branch = BRANCH_NONE;
				current_block = NULL; // have terminator
				break;
			}
			default:
				fprintf(stderr, "Bad opcode: opcode=%d\n", opcode);
				break;
		}
		if(OptLevel > 0 && inline_call && !DontInlineOpcodes) {
			inlineList.push_back(call);
		} else if(!opfunc->compiled) {
			// only compile opcode functions that are not inlined.
			opfunc->compiled = true;
			TheExecutionEngine->getPointerToFunction(opfunc->func);
		}
		// branch to next block.
		if(branch >= 0 && branch < code_len) {
			Builder.CreateBr(op_blocks[branch]);
			current_block = NULL; // have terminator
		} else if(branch == BRANCH_COND) {
			Builder.CreateCondBr(brcond, true_block, false_block);
			current_block = NULL; // have terminator
		}
	}
	// strip Lua bytecode and debug info.
	if(strip_code && strip_ops > 0) {
		code_len -= strip_ops;
		luaM_reallocvector(L, p->code, p->sizecode, code_len, Instruction);
		p->sizecode = code_len;
		luaM_reallocvector(L, p->lineinfo, p->sizelineinfo, 0, int);
		p->sizelineinfo = 0;
		luaM_reallocvector(L, p->locvars, p->sizelocvars, 0, LocVar);
		p->sizelocvars = 0;
		luaM_reallocvector(L, p->upvalues, p->sizeupvalues, 0, TString *);
		p->sizeupvalues = 0;
	}
	if(DumpFunctions) func->dump();
	// only run function inliner & optimization passes on same functions.
	if(OptLevel > 0 && !DontInlineOpcodes) {
		for(std::vector<llvm::CallInst *>::iterator I=inlineList.begin(); I != inlineList.end() ; I++) {
			InlineFunction(*I);
		}
		// Validate the generated code, checking for consistency.
		if(VerifyFunctions) verifyFunction(*func);
		// Optimize the function.
		if(TheFPM) TheFPM->run(*func);
	}
	for(i = 0; i < code_len; i++) {
		if(op_values[i]) {
			delete op_values[i];
			op_values[i] = NULL;
		}
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

void LLVMCompiler::free(lua_State *L, Proto *p)
{
	llvm::Function *func;

	if(TheExecutionEngine == NULL) return;

	func=(llvm::Function *)TheExecutionEngine->getGlobalValueAtAddress((void *)p->jit_func);
	if(func != NULL) {
		TheExecutionEngine->freeMachineCodeForFunction(func);
		func->removeFromParent();
		delete func;
	}
}

