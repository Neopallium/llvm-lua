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
#include "llvm/Module.h"
#include "llvm/TypeSymbolTable.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include <string>
#include <vector>
#include <fstream>

#include "LLVMCompiler.h"
#include "LLVMDumper.h"
#include "lstate.h"
#include "load_jit_proto.h"

//===----------------------------------------------------------------------===//
// Dump a compilable bitcode module.
//===----------------------------------------------------------------------===//

LLVMDumper::LLVMDumper(LLVMCompiler *compiler) : compiler(compiler) {
	max_alignment = (sizeof(void *) < sizeof(LUA_NUMBER)) ? sizeof(LUA_NUMBER) : sizeof(void *);
	max_alignment *= 8;

	TheModule = compiler->getModule();
	lua_func_type = compiler->get_lua_func_type();
	//
	// create constant_type structure type.
	//
	Ty_str_ptr=llvm::PointerType::get(llvm::Type::Int8Ty, 0);
	const llvm::Type *value_type;
	// TODO: handle LUA_NUMBER types other then 'double'.
	value_type = llvm::StructType::get(llvm::Type::DoubleTy, NULL);
	Ty_constant_num_type = llvm::StructType::get(llvm::Type::Int32Ty, value_type, NULL);
	TheModule->addTypeName("struct.constant_num_type", Ty_constant_num_type);
	value_type = llvm::StructType::get(llvm::IntegerType::get(max_alignment), NULL);
	Ty_constant_bool_type = llvm::StructType::get(llvm::Type::Int32Ty, value_type, NULL);
	TheModule->addTypeName("struct.constant_bool_type", Ty_constant_bool_type);
	value_type = llvm::StructType::get(Ty_str_ptr, NULL);
	if(sizeof(void *) < sizeof(LUA_NUMBER)) {
		const llvm::ArrayType *pad_type;
		pad_type = llvm::ArrayType::get(llvm::Type::Int8Ty, sizeof(LUA_NUMBER) - sizeof(void *));
		padding_constant = llvm::Constant::getNullValue(pad_type);
		value_type = llvm::StructType::get(Ty_str_ptr, pad_type, NULL);
	} else {
		padding_constant = NULL;
		value_type = llvm::StructType::get(Ty_str_ptr, NULL);
	}
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
}

void LLVMDumper::dump(const char *output, Proto *p, int optimize, int stripping) {
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
		compiler->compileAll(p,2);
		//TheModule->dump();
		// Run optimization passes on the whole module and each function.
		compiler->optimizeAll(p,optimize);
		// Dump proto info to global for reloading.
		dump_protos(p);
		//TheModule->dump();
		llvm::verifyModule(*TheModule);
		llvm::WriteBitcodeToFile(TheModule, OS);
	}
}

llvm::Constant *LLVMDumper::get_ptr(llvm::Constant *val) {
	std::vector<llvm::Constant *> idxList;
	idxList.push_back(llvm::Constant::getNullValue(llvm::Type::Int32Ty));
	idxList.push_back(llvm::Constant::getNullValue(llvm::Type::Int32Ty));
	return llvm::ConstantExpr::getGetElementPtr(val, &idxList[0], 2);
}

llvm::Constant *LLVMDumper::get_global_str(const char *str) {
	llvm::Constant *str_const = llvm::ConstantArray::get(str, true);
	llvm::GlobalVariable *var_str = new llvm::GlobalVariable(str_const->getType(), true,
		llvm::GlobalValue::InternalLinkage, str_const, ".str", TheModule);
	return get_ptr(var_str);
}

llvm::GlobalVariable *LLVMDumper::dump_constants(Proto *p) {
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
				if(padding_constant != NULL) {
					tmp_struct.push_back(padding_constant);
				}
				value = llvm::ConstantStruct::get(tmp_struct, false);
				break;
			case LUA_TBOOLEAN:
				type_length = constant_type_len(TYPE_BOOLEAN, 0);
				type = Ty_constant_bool_type;
				tmp_struct.push_back(llvm::ConstantInt::get(llvm::APInt(max_alignment, !l_isfalse(tval))));
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
				tmp_struct.push_back(llvm::ConstantInt::get(llvm::APInt(max_alignment, 0)));
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
			dump_constants(p), Ty_constant_type_ptr));
	// sizep
	jit_proto_fields.push_back(llvm::ConstantInt::get(llvm::APInt(32,p->sizep)));
	// p
	if(p->sizep > 0) {
		tmp_array.clear();
		for(int i = 0; i < p->sizep; i++) {
			tmp_array.push_back(dump_proto(p->p[i]));
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

void LLVMDumper::dump_protos(Proto *p) {
	//
	// dump protos to a global variable for re-loading.
	//
	llvm::Constant *jit_proto = dump_proto(p);
	llvm::GlobalVariable *gjit_proto_init = new llvm::GlobalVariable(Ty_jit_proto, false,
		llvm::GlobalValue::ExternalLinkage, jit_proto, "jit_proto_init", TheModule);
	gjit_proto_init->setAlignment(32);
}

