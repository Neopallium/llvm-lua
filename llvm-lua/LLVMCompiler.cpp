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

static llvm::cl::opt<bool> DontInlineOpcodes("do-not-inline-opcodes",
                   llvm::cl::desc("Turn off inlining of opcode functions."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> VerifyFunctions("verify-functions",
                   llvm::cl::desc("Verify each compiled function."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> DumpFunctions("dump-functions",
                   llvm::cl::desc("Dump LLVM IR for each function."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> DisableOpt("O0",
                   llvm::cl::desc("Disable optimizations."));

static llvm::cl::opt<bool> OptLevelO1("O1",
                   llvm::cl::desc("Optimization level 1."));

static llvm::cl::opt<bool> OptLevelO2("O2",
                   llvm::cl::desc("Optimization level 2."));

static llvm::cl::opt<bool> OptLevelO3("O3",
                   llvm::cl::desc("Optimization level 3."));

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

llvm::Value *LLVMCompiler::get_proto_constant(TValue *constant) {
	llvm::Value *val = NULL;
	switch(ttype(constant)) {
	case LUA_TBOOLEAN:
		val = llvm::ConstantInt::get(llvm::APInt(sizeof(LUA_NUMBER), !l_isfalse(constant)));
		break;
	case LUA_TNUMBER:
		val = llvm::ConstantFP::get(llvm::APFloat(nvalue(constant)));
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
	llvm::ModuleProvider *MP = NULL;
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
		func_args.push_back(llvm::Type::Int32Ty);
		func_type = llvm::FunctionType::get(llvm::Type::VoidTy, func_args, false);
		vm_print_OP = llvm::Function::Create(func_type,
			llvm::Function::ExternalLinkage, "vm_print_OP", TheModule);
	}
	// function for counting each executed op.
	if(RunOpCodeStats) {
		vm_count_OP = TheModule->getFunction("vm_count_OP");
		if(vm_count_OP == NULL) {
			func_args.clear();
			func_args.push_back(llvm::Type::Int32Ty);
			func_type = llvm::FunctionType::get(llvm::Type::VoidTy, func_args, false);
			vm_count_OP = llvm::Function::Create(func_type,
				llvm::Function::ExternalLinkage, "vm_count_OP", TheModule);
		}
		for(int i = 0; i < NUM_OPCODES; i++) {
			vm_op_run_count[i] = 0;
		}
	}
	// define extern vm_get_current_closure
	vm_get_current_closure = TheModule->getFunction("vm_get_current_closure");
	// define extern vm_get_current_constants
	vm_get_current_constants = TheModule->getFunction("vm_get_current_constants");
	// define extern vm_get_number
	vm_get_number = TheModule->getFunction("vm_get_number");

	// create prototype for vm_* functions.
	vm_op_funcs = new OPFunc *[NUM_OPCODES];
	for(int i = 0; i < NUM_OPCODES; i++) vm_op_funcs[i] = NULL; // clear list.
	for(int i = 0; true; i++) {
		func_info = &vm_op_functions[i];
		opcode = func_info->opcode;
		if(opcode < 0) break;
		vm_op_funcs[opcode] = new OPFunc(func_info, vm_op_funcs[opcode]);
		func = TheModule->getFunction(func_info->name);
		if(func != NULL) {
			vm_op_funcs[opcode]->func = func;
			vm_op_funcs[opcode]->compiled = !useJIT; // lazy compile ops when JIT is enabled.
			continue;
		}
		func_args.clear();
		for(int x = 0; func_info->params[x] != VAR_T_VOID; x++) {
			func_args.push_back(get_var_type(func_info->params[x]));
		}
		func_type = llvm::FunctionType::get(get_var_type(func_info->ret_type), func_args, false);
		func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_info->name, TheModule);
		vm_op_funcs[opcode]->func = func;
		vm_op_funcs[opcode]->compiled = true; // built-in op function.
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
			TheExecutionEngine->getPointerToFunction(vm_get_current_closure);
			TheExecutionEngine->getPointerToFunction(vm_get_current_constants);
			TheExecutionEngine->getPointerToFunction(vm_get_number);
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
			TheFPM->add(new llvm::TargetData(TheModule));
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
	//TheModule->dump();

	if(TheExecutionEngine) {
		TheExecutionEngine->runStaticConstructorsDestructors(true);
		delete TheExecutionEngine;
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
	bool inline_call=false;
	int strip_ops=0;
	int branch;
	Instruction op_intr;
	int opcode;
	int i;

	// don't JIT large functions.
	if(code_len >= 200 && TheExecutionEngine != NULL && !CompileLargeFunctions) {
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
		switch (opcode) {
			case OP_LOADBOOL:
				branch = i+1;
				// check C operand if C!=0 then skip over the next op_block.
				if(GETARG_C(op_intr) != 0)
					++branch;
				need_op_block[branch] = true;
				break;
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
					TValue *rc = p->k + INDEXK(GETARG_C(op_intr));
					if(ttisnumber(rc)) op_hints[i] = HINT_C_NUM_CONSTANT;
				}
				if(GETARG_A(op_intr) == 1) {
					op_hints[i] += HINT_NOT;
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
				// test if init/plimit/pstep are number constants.
				if(OptLevel > 1 && i >= 3) {
					const TValue *tmp;
					bool is_const_num[3];
					OPValues *vals = new OPValues(4);
					for(int x = 0; x < 3; ++x) {
						is_const_num[x] = false;
						if(GET_OPCODE(code[i-3+x]) == OP_LOADK) {
							tmp = p->k + GETARG_Bx(code[i-3+x]);
							if(ttisnumber(tmp)) {
								vals->set(x,llvm::ConstantFP::get(llvm::APFloat(nvalue(tmp))));
								is_const_num[x] = true;
								op_hints[i-3+x] = HINT_SKIP_OP;
							}
						}
					}
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
					}
					if(op_hints[i] == HINT_NONE) {
						// don't skip LOADK ops, since we are not inlining them.
						for(int x=i-3; x < i; x++) {
							if(op_hints[x] == HINT_SKIP_OP) op_hints[x] = HINT_NONE;
						}
					}
					vals->set(3, Builder.CreateAlloca(llvm::Type::DoubleTy, 0, "for_idx"));
					op_hints[branch] = HINT_FOR_N_N_N;
					op_values[branch] = vals;
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
					TValue *rc = p->k + INDEXK(GETARG_C(op_intr));
					if(ttisnumber(rc)) op_hints[i] = HINT_C_NUM_CONSTANT;
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
			strip_ops++;
			continue;
		}
		branch = i+1;
		op_intr=code[i];
		opcode = GET_OPCODE(op_intr);
		opfunc = vm_op_funcs[opcode];
		// find op function with matching hint.
		while(opfunc->next != NULL && opfunc->info->hint != op_hints[i]) {
			opfunc = opfunc->next;
		}
		if(OpCodeStats) {
			opcode_stats[opcode]++;
		}
		//fprintf(stderr, "%d: '%s' (%d) = 0x%08X, hint=%d\n", i, luaP_opnames[opcode], opcode, op_intr, op_hints[i]);
		if(PrintRunOpCodes) {
			Builder.CreateCall4(vm_print_OP, func_L, func_cl,
				llvm::ConstantInt::get(llvm::APInt(32,op_intr)),
				llvm::ConstantInt::get(llvm::APInt(32,i)));
		}
		if(RunOpCodeStats) {
			Builder.CreateCall(vm_count_OP, llvm::ConstantInt::get(llvm::APInt(32,op_intr)));
		}
		if(op_hints[i] == HINT_SKIP_OP) {
			strip_ops++;
			continue;
		}
#ifndef LUA_NODEBUG
		/* vm_next_OP function is used to call count/line debug hooks. */
		Builder.CreateCall2(vm_next_OP, func_L, func_cl);
#else
		if(strip_code) {
			// strip all opcodes.
			strip_ops++;
		}
#endif
		if(strip_ops > 0 && strip_ops < (i+1)) {
			// move opcodes we want to keep to new position.
			code[(i+1) - strip_ops] = op_intr;
		}
		// setup arguments for opcode function.
		func_info = opfunc->info;
		if(func_info == NULL) {
			fprintf(stderr, "Error missing vm_OP_* function for opcode: %d\n", opcode);
			return;
		}
		args.clear();
		for(int x = 0; func_info->params[x] != VAR_T_VOID ; x++) {
			llvm::Value *val=NULL;
			switch(func_info->params[x]) {
			case VAR_T_ARG_A:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_A(op_intr)));
				break;
			case VAR_T_ARG_C:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_C(op_intr)));
				break;
			case VAR_T_ARG_C_NUM_CONSTANT:
				val = get_proto_constant(p->k + INDEXK(GETARG_C(op_intr)));
				break;
			case VAR_T_ARG_C_NEXT_INSTRUCTION: {
				int c = GETARG_C(op_intr);
				// if C == 0, then next code value is used as ARG_C.
				if(c == 0) {
					if((i+1) < code_len) {
						c = code[i+1];
						strip_ops++;
					}
				}
				val = llvm::ConstantInt::get(llvm::APInt(32,c));
				break;
			}
			case VAR_T_ARG_B:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_B(op_intr)));
				break;
			case VAR_T_ARG_Bx:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_Bx(op_intr)));
				break;
			case VAR_T_ARG_sBx:
				val = llvm::ConstantInt::get(llvm::APInt(32,GETARG_sBx(op_intr)));
				break;
			case VAR_T_PC_OFFSET:
				val = llvm::ConstantInt::get(llvm::APInt(32,i + 1 - strip_ops));
				break;
			case VAR_T_INSTRUCTION:
				val = llvm::ConstantInt::get(llvm::APInt(32,op_intr));
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
				fprintf(stderr, "Error: Missing parameter for this opcode function!\n");
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
						llvm::ConstantInt::get(llvm::APInt(32, PCRTAILRECUR)), "brcond");
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
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(llvm::APInt(32,0)), "brcond");
				false_block=op_blocks[branch+1];
				/* inlined JMP op. */
				branch = ++i + 1;
#ifdef LUA_NODEBUG
				strip_ops++;
#endif
				op_intr=code[i];
				branch += GETARG_sBx(op_intr);
				true_block=op_blocks[branch];
				branch = BRANCH_COND; // do conditional branch
				break;
			case OP_FORLOOP:
				inline_call = true;
				brcond=call;
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(llvm::APInt(32,0)), "brcond");
				true_block=op_blocks[branch + GETARG_sBx(op_intr)];
				false_block=op_blocks[branch];
				branch = BRANCH_COND; // do conditional branch
				break;
			case OP_FORPREP: {
				OPValues *vals;
				//inline_call = true;
				branch += GETARG_sBx(op_intr);
				vals=op_values[branch];
				if(vals) {
					llvm::CallInst *call2;
					// get non-constant limit/step from stack.
					if(vals->get(1) == NULL) {
						call2=Builder.CreateCall2(vm_get_number,func_L,
							llvm::ConstantInt::get(llvm::APInt(32,(GETARG_A(op_intr) + 1))), "for_limit");
						inlineList.push_back(call2);
						vals->set(1, call2);
					}
					if(vals->get(2) == NULL) {
						call2=Builder.CreateCall2(vm_get_number,func_L,
							llvm::ConstantInt::get(llvm::APInt(32,(GETARG_A(op_intr) + 2))), "for_step");
						inlineList.push_back(call2);
						vals->set(2, call2);
					}
					if(vals->get(3) != NULL) {
						call2=Builder.CreateCall2(vm_get_number,func_L,
							llvm::ConstantInt::get(llvm::APInt(32,(GETARG_A(op_intr) + 0))), "for_init");
						inlineList.push_back(call2);
						// create for loop 'idx' variable storage space.
						llvm::Value *idx_var = vals->get(3);
						Builder.CreateStore(call2, idx_var); // store 'for_init' value.
						// add branch to forloop block.
						Builder.CreateBr(op_blocks[branch]);
						Builder.SetInsertPoint(op_blocks[branch]);
						// increment 'for_idx'
						llvm::Value *cur_idx = Builder.CreateLoad(idx_var);
						llvm::Value *next_idx = Builder.CreateAdd(cur_idx, vals->get(2), "next_idx");
						Builder.CreateStore(next_idx, idx_var); // store 'for_init' value.
						// create instructions to load and 
						vals->set(0, next_idx);
						current_block = NULL; // have terminator
						branch = BRANCH_NONE;
					}
				}
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
				if(strip_ops > 0) {
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
	// re-size lua opcode array if we stripped any opcodes.
	if(strip_ops > 0) {
		code_len -= strip_ops;
		luaM_reallocvector(L, p->code, p->sizecode, code_len, Instruction);
		p->sizecode = code_len;
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
		if(DumpFunctions) func->dump();
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
	//fprintf(stderr, "free llvm function ref: %p\n", func);
	if(func != NULL) {
		TheExecutionEngine->freeMachineCodeForFunction(func);
	}
}

