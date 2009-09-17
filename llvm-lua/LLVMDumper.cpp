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

#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Linker.h"
#include "llvm/TypeSymbolTable.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include <string>
#include <vector>
#include <fstream>
#include <stdint.h>

#include "LLVMCompiler.h"
#include "LLVMDumper.h"
#include "lstate.h"
#include "load_jit_proto.h"
#include "load_liblua_main.h"

static llvm::cl::opt<bool> LuaModule("lua-module",
                   llvm::cl::desc("Generate a Lua Module instead of a standalone exe."),
                   llvm::cl::init(false));

static llvm::cl::opt<bool> NoMain("no-main",
                   llvm::cl::desc("Don't link in liblua_main.bc."),
                   llvm::cl::init(false));

//===----------------------------------------------------------------------===//
// Dump a compilable bitcode module.
//===----------------------------------------------------------------------===//

LLVMDumper::LLVMDumper(LLVMCompiler *compiler) : compiler(compiler) {
	std::vector<const llvm::Type *> fields;
	llvm::TargetData *type_info;
	const llvm::Type *value_type;
	const llvm::ArrayType *pad_type;
	int num_size;
	int ptr_size;
	int max_size=0;
	int pad_size=0;

	M = compiler->getModule();
	// get target size of pointer & double
	type_info = new llvm::TargetData(M);
	num_size = type_info->getTypeStoreSize(llvm::Type::getDoubleTy(getCtx()));
	max_size = num_size;
	ptr_size = type_info->getPointerSize();
	if(ptr_size > max_size) max_size = ptr_size;
	delete type_info;

	lua_func_type = compiler->get_lua_func_type();
	lua_func_type_ptr = llvm::PointerType::get(lua_func_type, 0);
	Ty_str_ptr=llvm::PointerType::get(llvm::IntegerType::get(getCtx(), 8), 0);
	//
	// create constant_type structure type.
	//

	// union.constant_value
		// TODO: handle LUA_NUMBER types other then 'double'.
	fields.push_back(llvm::Type::getDoubleTy(getCtx()));
	Ty_constant_value = llvm::StructType::get(getCtx(), fields, false);
	M->addTypeName("union.constant_value", Ty_constant_value);

	// struct.constant_type
	fields.clear();
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // type
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // length
	fields.push_back(Ty_constant_value);                    // val
	Ty_constant_type = llvm::StructType::get(getCtx(), fields, false);
	M->addTypeName("struct.constant_type", Ty_constant_type);
	Ty_constant_type_ptr = llvm::PointerType::get(Ty_constant_type, 0);

	// struct.constant_num_type
	Ty_constant_num_type = Ty_constant_type;
	M->addTypeName("struct.constant_num_type", Ty_constant_num_type);
	num_padding = NULL;

	// struct.constant_bool_type
	fields.clear();
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // boolean
	pad_size = max_size - 4;
	if(pad_size > 0) {
		pad_type = llvm::ArrayType::get(llvm::IntegerType::get(getCtx(), 8), pad_size);
		bool_padding = llvm::Constant::getNullValue(pad_type);
		fields.push_back(pad_type);                           // padding
	} else {
		bool_padding = NULL;
	}
	value_type = llvm::StructType::get(getCtx(), fields, false);
	fields.clear();
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // type
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // length
	fields.push_back(value_type);                           // val (boolean)
	Ty_constant_bool_type = llvm::StructType::get(getCtx(), fields, false);
	M->addTypeName("struct.constant_bool_type", Ty_constant_bool_type);

	// struct.constant_str_type
	fields.clear();
	fields.push_back(Ty_str_ptr);                           // char *
	pad_size = max_size - ptr_size;
	if(pad_size > 0) {
		pad_type = llvm::ArrayType::get(llvm::IntegerType::get(getCtx(), 8), pad_size);
		str_padding = llvm::Constant::getNullValue(pad_type);
		fields.push_back(pad_type);                           // padding
	} else {
		str_padding = NULL;
	}
	value_type = llvm::StructType::get(getCtx(), fields, false);
	fields.clear();
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // type
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // length
	fields.push_back(value_type);                           // val (char *)
	Ty_constant_str_type = llvm::StructType::get(getCtx(), fields, false);
	M->addTypeName("struct.constant_str_type", Ty_constant_str_type);

	//
	// create jit_LocVar structure type.
	//
	fields.clear();
	fields.push_back(Ty_str_ptr); // varname
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // startpc
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // endpc
	Ty_jit_LocVar = llvm::StructType::get(getCtx(), fields, false);
	M->addTypeName("struct.jit_LocVar", Ty_jit_LocVar);
	Ty_jit_LocVar_ptr = llvm::PointerType::get(Ty_jit_LocVar, 0);

	//
	// create jit_proto structure type.
	//
	fields.clear();
	fields.push_back(Ty_str_ptr); // name
	fields.push_back(lua_func_type_ptr); // jit_func
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // linedefined
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // lastlinedefined
	fields.push_back(llvm::IntegerType::get(getCtx(), 8)); // nups
	fields.push_back(llvm::IntegerType::get(getCtx(), 8)); // numparams
	fields.push_back(llvm::IntegerType::get(getCtx(), 8)); // is_vararg
	fields.push_back(llvm::IntegerType::get(getCtx(), 8)); // maxstacksize
	fields.push_back(llvm::IntegerType::get(getCtx(), 16)); // sizek
	fields.push_back(llvm::IntegerType::get(getCtx(), 16)); // sizelocvars
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // sizeupvalues
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // sizep
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // sizecode
	fields.push_back(llvm::IntegerType::get(getCtx(), 32)); // sizelineinfo
	fields.push_back(Ty_constant_type_ptr); // k
	fields.push_back(Ty_jit_LocVar_ptr); // locvars
	fields.push_back(llvm::PointerType::get(Ty_str_ptr, 0)); // upvalues
	llvm::PATypeHolder jit_proto_fwd = llvm::OpaqueType::get(getCtx());
	fields.push_back(llvm::PointerType::get(jit_proto_fwd, 0)); // p
	fields.push_back(llvm::PointerType::get(llvm::IntegerType::get(getCtx(), 32), 0)); // code
	fields.push_back(llvm::PointerType::get(llvm::IntegerType::get(getCtx(), 32), 0)); // lineinfo
	Ty_jit_proto = llvm::StructType::get(getCtx(), fields, false);
	M->addTypeName("struct.jit_proto", Ty_jit_proto);
	llvm::cast<llvm::OpaqueType>(jit_proto_fwd.get())->refineAbstractTypeTo(Ty_jit_proto);
	Ty_jit_proto = llvm::cast<llvm::StructType>(jit_proto_fwd.get());
	Ty_jit_proto_ptr = llvm::PointerType::get(Ty_jit_proto, 0);
}

void LLVMDumper::dump(const char *output, lua_State *L, Proto *p, int stripping) {
	std::ofstream OS(output, std::ios_base::out|std::ios::trunc|std::ios::binary);
	llvm::ModuleProvider *MP = NULL;
	llvm::Module *liblua_main = NULL;
	std::string error;

	if(!OS.fail()) {
		compiler->setStripCode(stripping);
		// Internalize all opcode functions.
		for (llvm::Module::iterator I = M->begin(), E = M->end(); I != E; ++I) {
			llvm::Function *Fn = &*I;
			if (!Fn->isDeclaration())
				Fn->setLinkage(llvm::GlobalValue::getLinkOnceLinkage(true));
		}
		// Compile all Lua prototypes to LLVM IR
		compiler->compileAll(L, p);
		//M->dump();
		if(LuaModule) {
			// Dump proto info to static variable and create 'luaopen_<mod_name>' function.
			dump_lua_module(p, output);
		} else {
			// Dump proto info to global for standalone exe.
			dump_standalone(p);
			//M->dump();
			// link with liblua_main.bc
			if(!NoMain) {
				MP = load_liblua_main(getCtx(), true);
				liblua_main = MP->getModule();
				if(llvm::Linker::LinkModules(M, liblua_main, &error)) {
					fprintf(stderr, "Failed to link compiled Lua script with embedded 'liblua_main.bc': %s",
						error.c_str());
					exit(1);
				}
			}
		}
		//M->dump();
		llvm::verifyModule(*M);
		llvm::WriteBitcodeToFile(M, OS);
	}
}

llvm::Constant *LLVMDumper::get_ptr(llvm::Constant *val) {
	std::vector<llvm::Constant *> idxList;
	idxList.push_back(llvm::Constant::getNullValue(llvm::IntegerType::get(getCtx(), 32)));
	idxList.push_back(llvm::Constant::getNullValue(llvm::IntegerType::get(getCtx(), 32)));
	return llvm::ConstantExpr::getGetElementPtr(val, &idxList[0], 2);
}

llvm::Constant *LLVMDumper::get_global_str(const char *str) {
	llvm::Constant *str_const = llvm::ConstantArray::get(getCtx(), str, true);
	llvm::GlobalVariable *var_str = new llvm::GlobalVariable(*M, str_const->getType(), true,
		llvm::GlobalValue::InternalLinkage, str_const, ".str");
	return get_ptr(var_str);
}

llvm::GlobalVariable *LLVMDumper::dump_constants(Proto *p) {
	llvm::GlobalVariable *constant;
	llvm::Constant *array_struct;
	std::vector<llvm::Constant *> array_struct_fields;

	for(int i = 0; i < p->sizek; i++) {
		int const_type = 0;
		int const_length = 0;
		const llvm::StructType *type;
		std::vector<llvm::Constant *> tmp_struct;
		llvm::Constant *value;
		TValue *tval = &(p->k[i]);
		const_length = 0;
		switch(ttype(tval)) {
			case LUA_TSTRING:
				const_type = TYPE_STRING;
				const_length = tsvalue(tval)->len;
				type = Ty_constant_str_type;
				tmp_struct.push_back(get_global_str(svalue(tval)));
				if(str_padding != NULL) {
					tmp_struct.push_back(str_padding);
				}
				value = llvm::ConstantStruct::get(getCtx(), tmp_struct, false);
				break;
			case LUA_TBOOLEAN:
				const_type = TYPE_BOOLEAN;
				type = Ty_constant_bool_type;
				tmp_struct.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32, !l_isfalse(tval))));
				if(bool_padding != NULL) {
					tmp_struct.push_back(bool_padding);
				}
				value = llvm::ConstantStruct::get(getCtx(), tmp_struct, false);
				break;
			case LUA_TNUMBER:
				const_type = TYPE_NUMBER;
				type = Ty_constant_num_type;
				tmp_struct.push_back(llvm::ConstantFP::get(getCtx(), llvm::APFloat(nvalue(tval))));
				if(num_padding != NULL) {
					tmp_struct.push_back(num_padding);
				}
				value = llvm::ConstantStruct::get(getCtx(), tmp_struct, false);
				break;
			case LUA_TNIL:
			default:
				const_type = TYPE_NIL;
				type = Ty_constant_bool_type;
				tmp_struct.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32, 0)));
				if(bool_padding != NULL) {
					tmp_struct.push_back(bool_padding);
				}
				value = llvm::ConstantStruct::get(getCtx(), tmp_struct, false);
				break;
		}
		tmp_struct.clear();
		tmp_struct.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32, const_type)));
		tmp_struct.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32, const_length)));
		tmp_struct.push_back(value);
		array_struct_fields.push_back(llvm::ConstantStruct::get(type, tmp_struct));
	}

	array_struct = llvm::ConstantStruct::get(getCtx(), array_struct_fields, false);
	constant = new llvm::GlobalVariable(*M, array_struct->getType(), true,
		llvm::GlobalValue::InternalLinkage, array_struct, ".constants");
	//constant->setAlignment(32);
	return constant;
}

llvm::GlobalVariable *LLVMDumper::dump_locvars(Proto *p) {
	llvm::GlobalVariable *constant;
	llvm::Constant *array_struct;
	std::vector<llvm::Constant *> array_struct_fields;
	std::vector<llvm::Constant *> tmp_struct;

	for(int i = 0; i < p->sizelocvars; i++) {
		LocVar *locvar = &(p->locvars[i]);
		tmp_struct.clear();
		tmp_struct.push_back(get_global_str(getstr(locvar->varname)));
		tmp_struct.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32, locvar->startpc)));
		tmp_struct.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32, locvar->endpc)));
		array_struct_fields.push_back(llvm::ConstantStruct::get(Ty_jit_LocVar, tmp_struct));
	}

	array_struct = llvm::ConstantStruct::get(getCtx(), array_struct_fields, false);
	constant = new llvm::GlobalVariable(*M, array_struct->getType(), true,
		llvm::GlobalValue::InternalLinkage, array_struct, ".locvars");
	//constant->setAlignment(32);
	return constant;
}

llvm::GlobalVariable *LLVMDumper::dump_upvalues(Proto *p) {
	llvm::GlobalVariable *constant;
	llvm::Constant *array_struct;
	std::vector<llvm::Constant *> array_struct_fields;

	for(int i = 0; i < p->sizeupvalues; i++) {
		array_struct_fields.push_back(get_global_str(getstr(p->upvalues[i])));
	}

	array_struct = llvm::ConstantStruct::get(getCtx(), array_struct_fields, false);
	constant = new llvm::GlobalVariable(*M, array_struct->getType(), true,
		llvm::GlobalValue::InternalLinkage, array_struct, ".upvalues");
	//constant->setAlignment(32);
	return constant;
}

llvm::Constant *LLVMDumper::dump_proto(Proto *p) {
	std::vector<llvm::Constant *> jit_proto_fields;
	std::vector<llvm::Constant *> tmp_array;
	llvm::Function *func = (llvm::Function *)p->func_ref;
	llvm::GlobalVariable *tmp_global;
	llvm::Constant *tmp_constant;

	// name
	jit_proto_fields.push_back(get_global_str(getstr(p->source)));
	// jit_func
	if(func) {
		jit_proto_fields.push_back(func);
	} else {
		jit_proto_fields.push_back(llvm::Constant::getNullValue(lua_func_type_ptr));
	}
	// linedefined
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32,p->linedefined)));
	// lastlinedefined
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32,p->lastlinedefined)));
	// nups
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(8,p->nups)));
	// numparams
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(8,p->numparams)));
	// is_vararg
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(8,p->is_vararg)));
	// maxstacksize
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(8,p->maxstacksize)));
	// sizek
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(16,p->sizek)));
	// sizelocvars
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(16,p->sizelocvars)));
	// sizeupvalues
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32,p->sizeupvalues)));
	// sizep
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32,p->sizep)));
	// sizecode
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32,p->sizecode)));
	// sizelineinfo
	jit_proto_fields.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32,p->sizelineinfo)));
	// k
	jit_proto_fields.push_back(
		llvm::ConstantExpr::getCast(llvm::Instruction::BitCast,
			dump_constants(p), Ty_constant_type_ptr));
	// locvars
	jit_proto_fields.push_back(
		llvm::ConstantExpr::getCast(llvm::Instruction::BitCast,
			dump_locvars(p), Ty_jit_LocVar_ptr));
	// upvalues
	jit_proto_fields.push_back(
		llvm::ConstantExpr::getCast(llvm::Instruction::BitCast,
			dump_upvalues(p), llvm::PointerType::get(Ty_str_ptr, 0)));
	// p
	if(p->sizep > 0) {
		tmp_array.clear();
		for(int i = 0; i < p->sizep; i++) {
			tmp_array.push_back(dump_proto(p->p[i]));
		}
		tmp_constant = llvm::ConstantArray::get(llvm::ArrayType::get(Ty_jit_proto,p->sizep),tmp_array);
		tmp_global = new llvm::GlobalVariable(*M, tmp_constant->getType(), false,
			llvm::GlobalValue::InternalLinkage, tmp_constant, ".sub_protos");
		jit_proto_fields.push_back(get_ptr(tmp_global));
	} else {
		jit_proto_fields.push_back(llvm::Constant::getNullValue(Ty_jit_proto_ptr));
	}
	// code
	if(p->sizecode > 0) {
		tmp_array.clear();
		for(int i = 0; i < p->sizecode; i++) {
			tmp_array.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32,p->code[i])));
		}
		tmp_constant = llvm::ConstantArray::get(llvm::ArrayType::get(llvm::IntegerType::get(getCtx(), 32),p->sizecode),tmp_array);
		tmp_global = new llvm::GlobalVariable(*M, tmp_constant->getType(), false,
			llvm::GlobalValue::InternalLinkage, tmp_constant, ".proto_code");
		jit_proto_fields.push_back(get_ptr(tmp_global));
	} else {
		jit_proto_fields.push_back(llvm::Constant::getNullValue(llvm::PointerType::get(llvm::IntegerType::get(getCtx(), 32), 0)));
	}
	// lineinfo
	if(p->sizelineinfo > 0) {
		tmp_array.clear();
		for(int i = 0; i < p->sizelineinfo; i++) {
			tmp_array.push_back(llvm::ConstantInt::get(getCtx(), llvm::APInt(32,p->lineinfo[i])));
		}
		tmp_constant = llvm::ConstantArray::get(llvm::ArrayType::get(llvm::IntegerType::get(getCtx(), 32),p->sizelineinfo),tmp_array);
		tmp_global = new llvm::GlobalVariable(*M, tmp_constant->getType(), false,
			llvm::GlobalValue::InternalLinkage, tmp_constant, ".proto_lineinfo");
		jit_proto_fields.push_back(get_ptr(tmp_global));
	} else {
		jit_proto_fields.push_back(llvm::Constant::getNullValue(llvm::PointerType::get(llvm::IntegerType::get(getCtx(), 32), 0)));
	}

	return llvm::ConstantStruct::get(Ty_jit_proto, jit_proto_fields);
}

void LLVMDumper::dump_standalone(Proto *p) {
	//
	// dump protos to a global variable for re-loading.
	//
	llvm::Constant *jit_proto = dump_proto(p);
	//llvm::GlobalVariable *gjit_proto_init = 
	new llvm::GlobalVariable(*M, Ty_jit_proto, false,
		llvm::GlobalValue::ExternalLinkage, jit_proto, "jit_proto_init");
	//gjit_proto_init->setAlignment(32);
}

void LLVMDumper::dump_lua_module(Proto *p, std::string mod_name) {
	llvm::IRBuilder<> Builder(getCtx());
	llvm::Function *func;
	llvm::Function *load_compiled_module_func;
	llvm::BasicBlock *block=NULL;
	llvm::Value *func_L;
	llvm::CallInst *call=NULL;
	std::vector<const llvm::Type*> func_args;
	llvm::FunctionType *func_type;
	std::string name = "luaopen_";
	std::string tmp;
	size_t n;

	//
	// normalize mod_name.
	//

	// remove '.bc' from end of mod_name.
	n = mod_name.size()-3;
	if(n > 0) {
		tmp = mod_name.substr(n, 3);
		if(tmp[0] == '.') {
			if(tmp[1] == 'b' || tmp[1] == 'B') {
				if(tmp[2] == 'c' || tmp[2] == 'C') {
					mod_name = mod_name.substr(0, n);
				}
			}
		}
	}
	// convert non-alphanum chars to '_'
	for(n = 0; n < mod_name.size(); n++) {
		char c = mod_name[n];
		if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) continue;
		if(c == '\n' || c == '\r') {
			mod_name = mod_name.substr(0,n);
			break;
		}
		mod_name[n] = '_';
	}

	//
	// dump protos to a static variable for re-loading.
	//
	llvm::Constant *jit_proto = dump_proto(p);
	llvm::GlobalVariable *gjit_proto_init = new llvm::GlobalVariable(*M, Ty_jit_proto, false,
		llvm::GlobalValue::InternalLinkage, jit_proto, "jit_proto_init");
	//gjit_proto_init->setAlignment(32);

	//
	// dump 'luaopen_<mod_name>' for loading the module.
	//
	name.append(mod_name);
	func = llvm::Function::Create(lua_func_type, llvm::Function::ExternalLinkage, name, M);
	// name arg1 = "L"
	func_L = func->arg_begin();
	func_L->setName("L");
	// entry block
	block = llvm::BasicBlock::Create(getCtx(), "entry", func);
	Builder.SetInsertPoint(block);
	// call 'load_compiled_module'
	load_compiled_module_func = M->getFunction("load_compiled_module");
	if(load_compiled_module_func == NULL) {
		func_args.clear();
		func_args.push_back(func_L->getType());
		func_args.push_back(Ty_jit_proto_ptr);
		func_type = llvm::FunctionType::get(llvm::IntegerType::get(getCtx(), 32), func_args, false);
		load_compiled_module_func = llvm::Function::Create(func_type,
			llvm::Function::ExternalLinkage, "load_compiled_module", M);
	}
	call=Builder.CreateCall2(load_compiled_module_func, func_L, gjit_proto_init);
	call->setTailCall(true);
	Builder.CreateRet(call);

	//func->dump();
}

