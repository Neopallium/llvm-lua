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
#include "llvm/TypeSymbolTable.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include <cstdio>
#include <string>
#include <map>
#include <vector>
#include <fstream>

#include "llvm-compiler.h"
#include "lopcodes.h"
#include "lobject.h"
#include "lstate.h"
#include "load_jit_proto.h"

void dumpProto(llvm::Function *func) {
	for (llvm::Function::arg_iterator I = func->arg_begin(), E = func->arg_end(); I != E; ++I) {
		llvm::Argument *arg = &*I;
		fprintf(stderr,"dump arg: type=%p :", arg->getType());
		arg->dump();
	}
}

void dumpStructType(const llvm::StructType *type) {
	for (llvm::StructType::element_iterator I = type->element_begin(), E = type->element_end(); I != E; ++I) {
		llvm::Type *elem = (&*I)->get();
		fprintf(stderr,"dump element: type=%p: ", elem);
		elem->dump();
	}
}

void dumpConstantType(const std::vector<llvm::Constant *> &V) {
	for (std::vector<llvm::Constant *>::const_iterator I = V.begin(), E = V.end(); I != E; ++I) {
		llvm::Constant *C = *I;
		fprintf(stderr,"dump constant: type=%p: ", C->getType());
		C->getType()->dump();
	}
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static llvm::Module *TheModule;
static llvm::IRBuilder Builder;
static std::map<std::string, llvm::Value*> NamedValues;
static llvm::FunctionPassManager *TheFPM;
static llvm::PassManager *ThePM;

// struct types.
static const llvm::Type *Ty_lua_State;
static const llvm::Type *Ty_lua_State_ptr;
static const llvm::Type *Ty_func_state;
static const llvm::Type *Ty_func_state_ptr;
// common function types.
static llvm::FunctionType *lua_func_type;
// function for inializing func_state.
static llvm::Function *vm_func_state_init;
// function for print each executed op.
static llvm::Function *vm_print_OP;
// list of op functions.
static llvm::Function **vm_ops;

//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

static llvm::ExecutionEngine *TheExecutionEngine;

extern "C" void llvm_compiler_optimize(Proto *p, int optimize)
{
	llvm::Function *func=(llvm::Function *)p->func_ref;
	if(func == NULL) return;
	if(optimize >= 1) {
#if 0
		fprintf(stderr, "after Function Pass:\n");
		if(p->source) {
			printf("optimize: %s\n", getstr(p->source));
		} else {
			printf("optimize: %p\n", p);
		}
		TheFPM->run(*func);
		func->dump();
#else
		TheFPM->run(*func);
#endif
	}
}

/*
 * Optimize all jitted functions.
 */
extern "C" void llvm_compiler_optimize_all(Proto *parent, int optimize) {
	if(optimize >= 3) {
#if 0
		fprintf(stderr, "Run full Module Pass:\n");
		ThePM->run(*TheModule);
		//TheModule->dump();
#else
		ThePM->run(*TheModule);
#endif
	}
#if 1
	/* optimize parent */
	llvm_compiler_optimize(parent, 2);
	/* optimize all children */
	for(int i = 0; i < parent->sizep; i++) {
		llvm_compiler_optimize_all(parent->p[i], 2);
	}
#endif
}

/*
 * Pre-Compile all loaded functions.
 */
extern "C" void llvm_compiler_compile_all(Proto *parent, int optimize) {
	int i;
	/* pre-compile parent */
#if 0
	if(parent->source) {
		printf("pre-compile: %s\n", getstr(parent->source));
	} else {
		printf("pre-compile: %p\n", parent);
	}
#endif
	llvm_compiler_compile(parent, 0);
	/* pre-compile all children */
	for(i = 0; i < parent->sizep; i++) {
		llvm_compiler_compile_all(parent->p[i], 0);
	}
	if(optimize > 2) {
		fprintf(stderr, "Run full Module Pass:\n");
		ThePM->run(*TheModule);
		TheModule->dump();
	}
}

extern "C" void llvm_compiler_compile(Proto *p, int optimize)
{
	LuaInstruction *code=p->code;
	int code_len=p->sizecode;
	llvm::Function *func;
	llvm::BasicBlock *true_block=NULL;
	llvm::BasicBlock *false_block=NULL;
	llvm::Value *brcond=NULL;
	std::string name;
	char tmp[128];
	int branch;
	int op;
	int i;

	// create function.
	name = getstr(p->source);
	snprintf(tmp,128,"_%d:%d",p->linedefined, p->lastlinedefined);
	name += tmp;
	func = llvm::Function::Create(lua_func_type, llvm::Function::ExternalLinkage, name, TheModule);
	// name arg1 = "L"
	func->arg_begin()->setName("L");
	// entry block
	llvm::BasicBlock *entry_block = llvm::BasicBlock::Create("entry", func);
	Builder.SetInsertPoint(entry_block);
	// setup func_state structure on stack.
	llvm::Value *fs = Builder.CreateAlloca(Ty_func_state, 0, "fs");
	// create local for return value.
	llvm::Value *retval = Builder.CreateAlloca(llvm::Type::Int32Ty, 0, "retval");
	// call vm_func_state_init to initialize func_state.
	Builder.CreateCall2(vm_func_state_init, func->arg_begin(), fs);

	// pre-create basic blocks.
	llvm::BasicBlock *op_blocks[code_len];
	for(i = 0; i < code_len; i++) {
		op_blocks[i] = llvm::BasicBlock::Create("op_block", func);
	}
	// branch "entry" to first block.
	Builder.CreateBr(op_blocks[0]);
	// create return_block
	llvm::BasicBlock *return_block = llvm::BasicBlock::Create("return", func);
	Builder.SetInsertPoint(return_block);
	Builder.CreateRet(Builder.CreateLoad(retval, "retval"));
	// gen op calls.
	for(i = 0; i < code_len; i++) {
		Builder.SetInsertPoint(op_blocks[i]);
		branch = i+1;
		op = GET_OPCODE(code[i]);
		//fprintf(stderr, "'%s' (%d) = 0x%08X\n", luaP_opnames[op], op, code[i]);
		//Builder.CreateCall2(vm_print_OP, fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
		switch (op) {
			case OP_LOADBOOL:
				// check C operand if C!=0 then skip over the next op_block.
				if(GETARG_C(code[i]) != 0) branch += 1;
				// fall through.
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
			case OP_CALL:
			case OP_TAILCALL:
			case OP_CLOSE:
			case OP_VARARG:
				Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
				break;
			case OP_JMP:
				// always branch to the offset stored in operand sBx
				branch += GETARG_sBx(code[i]);
				// call vm_OP_JMP just in case luai_threadyield is defined.
				Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
				break;
			case OP_EQ:
			case OP_LT:
			case OP_LE:
			case OP_TEST:
			case OP_TESTSET:
			case OP_TFORLOOP:
				brcond=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])),"ret");
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(llvm::APInt(32,0)), "brcond");
				true_block=op_blocks[branch];
				false_block=op_blocks[branch+1];
				branch = -1; // do conditional branch
				break;
			case OP_FORLOOP:
				brcond=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])),"ret");
				brcond=Builder.CreateICmpNE(brcond, llvm::ConstantInt::get(llvm::APInt(32,0)), "brcond");
				true_block=op_blocks[branch + GETARG_sBx(code[i])];
				false_block=op_blocks[branch];
				branch = -1; // do conditional branch
				break;
			case OP_FORPREP:
				Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])));
				branch += GETARG_sBx(code[i]);
				break;
			case OP_SETLIST: {
				int a = GETARG_A(code[i]);
				int b = GETARG_B(code[i]);
				int c = GETARG_C(code[i]);
				// if C == 0, then next code value is count value.
				if(c == 0) {
					i++;
					if(i < code_len) {
						c = code[i];
					}
				}
				Builder.CreateCall4(vm_ops[op],
					fs,
					llvm::ConstantInt::get(llvm::APInt(32,a)),
					llvm::ConstantInt::get(llvm::APInt(32,b)),
					llvm::ConstantInt::get(llvm::APInt(32,c))
				);
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
					branch = i + nups;
					// skip pseudo MOVE/GETUPVAL ops.
					while(i < branch) {
						i++;
						op_blocks[i]->eraseFromParent();
						op_blocks[i] = NULL;
					}
					branch++;
				}
				break;
			}
			case OP_RETURN: {
				llvm::Value *tmp;
				tmp=Builder.CreateCall2(vm_ops[op], fs, llvm::ConstantInt::get(llvm::APInt(32,code[i])),"tmp");
				Builder.CreateStore(tmp,retval);
				branch = code_len;// branch to return_block.
				break;
			}
			default:
				fprintf(stderr, "Bad opcode: opcode=%d\n", op);
				break;
		}
		if(branch >= 0 && branch < code_len) {
			Builder.CreateBr(op_blocks[branch]);
		} else if(branch == code_len) {
			// last op_block branch to return_block
			Builder.CreateBr(return_block);
		} else if(branch == -1) {
			Builder.CreateCondBr(brcond, true_block, false_block);
		} else {
			fprintf(stderr, "Bad branch out-of-range: branch=%d\n", branch);
		}
	}
	//func->dump();
	// Validate the generated code, checking for consistency.
	//verifyFunction(*func);
	// Optimize the function.
	if(optimize > 0) {
		llvm_compiler_optimize(p, optimize);
	}

	// finished.
	if(TheExecutionEngine != NULL) {
		p->jit_func = (lua_CFunction)TheExecutionEngine->getPointerToFunction(func);
	} else {
		p->jit_func = NULL;
	}
	p->func_ref = func;
}

extern "C" void llvm_compiler_free(Proto *p)
{
	llvm::Function *func;

	if(TheExecutionEngine == NULL) return;

	func=(llvm::Function *)TheExecutionEngine->getGlobalValueAtAddress((void *)p->jit_func);
	//fprintf(stderr, "free llvm function ref: %p\n", func);
	if(func != NULL) {
		TheExecutionEngine->freeMachineCodeForFunction(func);
	}
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//
static const bool NoLazyCompilation = true;

int llvm_compiler_main(int useJIT, int argc, char ** argv) {
	llvm::ModuleProvider *MP = NULL;
	std::string error;
	const char *ops_file="lua_vm_ops.bc";

	// Load in the bitcode file containing the functions for each
	// bytecode operation.
	if(llvm::MemoryBuffer* buffer = llvm::MemoryBuffer::getFile(ops_file, &error)) {
		MP = llvm::getBitcodeModuleProvider(buffer, &error);
		if(!MP) delete buffer;
	}
	if(!MP) {
		printf("Failed to parse %s file: %s\n", ops_file, error.c_str());
		exit(1);
	}
	// Get Module from ModuleProvider.
	TheModule = NoLazyCompilation ? MP->materializeModule(&error) : MP->getModule();
	if(!TheModule) {
		printf("Failed to read %s file: %s\n", ops_file, error.c_str());
		exit(1);
	}
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

	// Create the JIT.
	if(useJIT) {
		TheExecutionEngine = llvm::ExecutionEngine::create(MP);
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

#if 1
	TheFPM = new llvm::FunctionPassManager(MP);
	ThePM = new llvm::PassManager();
	
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
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	TheFPM->add(llvm::createInstructionCombiningPass());
	// Reassociate expressions.
	TheFPM->add(llvm::createReassociatePass());
	// Eliminate Common SubExpressions.
	TheFPM->add(llvm::createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	TheFPM->add(llvm::createCFGSimplificationPass());
	/*
	 * Pass Manager.
	 */
	if(useJIT) {
		ThePM->add(new llvm::TargetData(*TheExecutionEngine->getTargetData()));
	} else {
		ThePM->add(new llvm::TargetData(TheModule));
	}
	//ThePM->add(llvm::createVerifierPass());
	ThePM->add(llvm::createLowerSetJmpPass());
	ThePM->add(llvm::createRaiseAllocationsPass());
	ThePM->add(llvm::createCFGSimplificationPass());
	ThePM->add(llvm::createPromoteMemoryToRegisterPass());
	ThePM->add(llvm::createGlobalOptimizerPass());
	ThePM->add(llvm::createGlobalDCEPass());
	ThePM->add(llvm::createFunctionInliningPass());
#else
	TheFPM = new llvm::FunctionPassManager(MP);
	ThePM = new llvm::PassManager();
#endif
	
	return 0;
}

static const llvm::Type *Ty_str_ptr;
static const llvm::StructType *Ty_constant_num_type;
static const llvm::StructType *Ty_constant_bool_type;
static const llvm::StructType *Ty_constant_str_type;
static const llvm::Type *Ty_constant_type_ptr;
static const llvm::StructType *Ty_jit_proto;
static const llvm::Type *Ty_jit_proto_ptr;
static const llvm::Type *lua_func_type_ptr;

llvm::Constant *get_ptr(llvm::Constant *val) {
	std::vector<llvm::Constant *> idxList;
	idxList.push_back(llvm::Constant::getNullValue(llvm::Type::Int32Ty));
	idxList.push_back(llvm::Constant::getNullValue(llvm::Type::Int32Ty));
	return llvm::ConstantExpr::getGetElementPtr(val, &idxList[0], 2);
}

llvm::Constant *get_global_str(const char *str) {
	llvm::Constant *str_const = llvm::ConstantArray::get(str, true);
	llvm::GlobalVariable *var_str = new llvm::GlobalVariable(str_const->getType(), true,
		llvm::GlobalValue::InternalLinkage, str_const, ".str", TheModule);
	return get_ptr(var_str);
}

llvm::GlobalVariable *llvm_compiler_dump_constants(Proto *p) {
	llvm::GlobalVariable *constant;
	llvm::Constant *array_struct;
	std::vector<llvm::Constant *> array_struct_fields;

	for(int i = 0; i < p->sizek; i++) {
		int type_length = 0;
		const llvm::StructType *type;
		std::vector<llvm::Constant *> tmp_struct;
		llvm::Constant *value;
		TValue *tval = &(p->k[i]);
		switch(ttype(tval)) {
			case LUA_TSTRING:
				type_length = constant_type_len(TYPE_STRING, tsvalue(tval)->len);
				type = Ty_constant_str_type;
				tmp_struct.push_back(get_global_str(svalue(tval)));
				value = llvm::ConstantStruct::get(tmp_struct, false);
				break;
			case LUA_TBOOLEAN:
				type_length = constant_type_len(TYPE_BOOLEAN, 0);
				type = Ty_constant_bool_type;
				tmp_struct.push_back(llvm::ConstantInt::get(llvm::APInt(8, !l_isfalse(tval))));
				value = llvm::ConstantStruct::get(tmp_struct, false);
				break;
			case LUA_TNUMBER:
				type_length = constant_type_len(TYPE_NUMBER, 0);
				type = Ty_constant_num_type;
				tmp_struct.push_back(llvm::ConstantFP::get(llvm::APFloat(nvalue(tval))));
				value = llvm::ConstantStruct::get(tmp_struct, false);
				break;
			case LUA_TNIL:
			default:
				type_length = constant_type_len(TYPE_NIL, 0);
				type = Ty_constant_bool_type;
				tmp_struct.push_back(llvm::ConstantInt::get(llvm::APInt(8, 0)));
				value = llvm::ConstantStruct::get(tmp_struct, false);
				break;
		}
		tmp_struct.clear();
		tmp_struct.push_back(llvm::ConstantInt::get(llvm::APInt(32, type_length)));
		tmp_struct.push_back(value);
		array_struct_fields.push_back(llvm::ConstantStruct::get(type, tmp_struct));
	}

	array_struct = llvm::ConstantStruct::get(array_struct_fields, false);
	constant = new llvm::GlobalVariable(array_struct->getType(), true,
		llvm::GlobalValue::InternalLinkage, array_struct, ".constants", TheModule);
	constant->setAlignment(32);
	return constant;
}

llvm::Constant *llvm_compiler_dump_proto(Proto *p) {
	std::vector<llvm::Constant *> jit_proto_fields;
	std::vector<llvm::Constant *> tmp_array;
	llvm::Function *func = (llvm::Function *)p->func_ref;
	llvm::GlobalVariable *tmp_global;
	llvm::Constant *tmp_constant;

	// name
	jit_proto_fields.push_back(get_global_str(getstr(p->source)));
	// jit_func
	jit_proto_fields.push_back(func);
	// nups
	jit_proto_fields.push_back(llvm::ConstantInt::get(llvm::APInt(8,p->nups)));
	// numparams
	jit_proto_fields.push_back(llvm::ConstantInt::get(llvm::APInt(8,p->numparams)));
	// is_vararg
	jit_proto_fields.push_back(llvm::ConstantInt::get(llvm::APInt(8,p->is_vararg)));
	// maxstacksize
	jit_proto_fields.push_back(llvm::ConstantInt::get(llvm::APInt(8,p->maxstacksize)));
	// sizek
	jit_proto_fields.push_back(llvm::ConstantInt::get(llvm::APInt(32,p->sizek)));
	// k
	jit_proto_fields.push_back(
		llvm::ConstantExpr::getCast(llvm::Instruction::BitCast,
			llvm_compiler_dump_constants(p), Ty_constant_type_ptr));
	// sizep
	jit_proto_fields.push_back(llvm::ConstantInt::get(llvm::APInt(32,p->sizep)));
	// p
	if(p->sizep > 0) {
		tmp_array.clear();
		for(int i = 0; i < p->sizep; i++) {
			tmp_array.push_back(llvm_compiler_dump_proto(p->p[i]));
		}
		tmp_constant = llvm::ConstantArray::get(llvm::ArrayType::get(Ty_jit_proto,p->sizep),tmp_array);
		tmp_global = new llvm::GlobalVariable(tmp_constant->getType(), false,
			llvm::GlobalValue::InternalLinkage, tmp_constant, ".sub_protos", TheModule);
		jit_proto_fields.push_back(get_ptr(tmp_global));
	} else {
		jit_proto_fields.push_back(llvm::Constant::getNullValue(Ty_jit_proto_ptr));
	}
	// sizecode
	jit_proto_fields.push_back(llvm::ConstantInt::get(llvm::APInt(32,p->sizecode)));
	// code
	if(p->sizecode > 0) {
		tmp_array.clear();
		for(int i = 0; i < p->sizecode; i++) {
			tmp_array.push_back(llvm::ConstantInt::get(llvm::APInt(32,p->code[i])));
		}
		tmp_constant = llvm::ConstantArray::get(llvm::ArrayType::get(llvm::Type::Int32Ty,p->sizecode),tmp_array);
		tmp_global = new llvm::GlobalVariable(tmp_constant->getType(), false,
			llvm::GlobalValue::InternalLinkage, tmp_constant, ".proto_code", TheModule);
		jit_proto_fields.push_back(get_ptr(tmp_global));
	} else {
		jit_proto_fields.push_back(llvm::Constant::getNullValue(llvm::PointerType::get(llvm::Type::Int32Ty, 0)));
	}

	//dumpConstantType(jit_proto_fields);
	//dumpStructType(Ty_jit_proto);
	return llvm::ConstantStruct::get(Ty_jit_proto, jit_proto_fields);
}

void llvm_compiler_dump_protos(Proto *p) {
	//
	// create constant_type structure type.
	//
	Ty_str_ptr=llvm::PointerType::get(llvm::Type::Int8Ty, 0);
	const llvm::Type *value_type;
	value_type = llvm::StructType::get(llvm::Type::DoubleTy, NULL);
	Ty_constant_num_type = llvm::StructType::get(llvm::Type::Int32Ty, value_type, NULL);
	TheModule->addTypeName("struct.constant_num_type", Ty_constant_num_type);
	value_type = llvm::StructType::get(llvm::Type::Int8Ty,
		llvm::ArrayType::get(llvm::Type::Int8Ty, 7) , NULL);
	Ty_constant_bool_type = llvm::StructType::get(llvm::Type::Int32Ty, value_type, NULL);
	TheModule->addTypeName("struct.constant_bool_type", Ty_constant_bool_type);
	value_type = llvm::StructType::get(Ty_str_ptr, NULL);
	Ty_constant_str_type = llvm::StructType::get(llvm::Type::Int32Ty, value_type, NULL);
	TheModule->addTypeName("struct.constant_str_type", Ty_constant_str_type);
	lua_func_type_ptr = llvm::PointerType::get(lua_func_type, 0);
	Ty_constant_type_ptr = llvm::PointerType::get(Ty_constant_num_type, 0);
	//
	// create jit_proto structure type.
	//
	std::vector<const llvm::Type *> jit_proto_fields;
	jit_proto_fields.push_back(Ty_str_ptr); // name
	jit_proto_fields.push_back(lua_func_type_ptr); // jit_func
	jit_proto_fields.push_back(llvm::Type::Int8Ty); // nups
	jit_proto_fields.push_back(llvm::Type::Int8Ty); // numparams
	jit_proto_fields.push_back(llvm::Type::Int8Ty); // is_vararg
	jit_proto_fields.push_back(llvm::Type::Int8Ty); // maxstacksize
	jit_proto_fields.push_back(llvm::Type::Int32Ty); // sizek
	jit_proto_fields.push_back(Ty_constant_type_ptr); // k
	jit_proto_fields.push_back(llvm::Type::Int32Ty); // sizep
	llvm::PATypeHolder jit_proto_fwd = llvm::OpaqueType::get();
	jit_proto_fields.push_back(llvm::PointerType::get(jit_proto_fwd, 0)); // p
	jit_proto_fields.push_back(llvm::Type::Int32Ty); // sizecode
	jit_proto_fields.push_back(llvm::PointerType::get(llvm::Type::Int32Ty, 0)); // code
	Ty_jit_proto = llvm::StructType::get(jit_proto_fields, false);
	TheModule->addTypeName("struct.jit_proto", Ty_jit_proto);
	llvm::cast<llvm::OpaqueType>(jit_proto_fwd.get())->refineAbstractTypeTo(Ty_jit_proto);
	Ty_jit_proto = llvm::cast<llvm::StructType>(jit_proto_fwd.get());
	Ty_jit_proto_ptr = llvm::PointerType::get(Ty_jit_proto, 0);
	//
	// dump protos to a global variable for re-loading.
	//
	llvm::Constant *jit_proto = llvm_compiler_dump_proto(p);
	llvm::GlobalVariable *gjit_proto_init = new llvm::GlobalVariable(Ty_jit_proto, false,
		llvm::GlobalValue::InternalLinkage, jit_proto, "jit_proto_init", TheModule);
	gjit_proto_init->setAlignment(32);
	gjit_proto_init->setLinkage(llvm::GlobalVariable::ExternalLinkage);
}

void llvm_compiler_dump(const char *output, Proto *p, int optimize, int stripping) {
	std::ofstream OS(output, std::ios_base::out|std::ios::trunc|std::ios::binary);
	std::string error;
	if(!OS.fail()) {
		// Internalize all opcode functions.
		for (llvm::Module::iterator I = TheModule->begin(), E = TheModule->end(); I != E; ++I) {
			llvm::Function *Fn = &*I;
			if (!Fn->isDeclaration())
				Fn->setLinkage(llvm::Function::LinkOnceLinkage);
		}
		// Compile all Lua prototypes to LLVM IR
		llvm_compiler_compile_all(p,0);
		//TheModule->dump();
		// Run optimization passes on the whole module and each function.
		llvm_compiler_optimize_all(p,optimize);
		// Dump proto info to global for reloading.
		llvm_compiler_dump_protos(p);
		//TheModule->dump();
		verifyModule(*TheModule);
		WriteBitcodeToFile(TheModule, OS);
	}
}

void llvm_compiler_cleanup() {
	delete TheFPM;
	delete ThePM;

	TheFPM = NULL;
	ThePM = NULL;

	// Print out all of the generated code.
	//TheModule->dump();

	if(TheExecutionEngine) {
		TheExecutionEngine->runStaticConstructorsDestructors(true);
	}
}

