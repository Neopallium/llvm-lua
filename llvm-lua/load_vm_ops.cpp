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

#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"

#ifdef USE_BITCODE_FILE
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#endif

#include <string>

using namespace llvm;

#include "load_vm_ops.h"

#ifndef USE_BITCODE_FILE
#include "lua_vm_ops_module.h"
#endif

ModuleProvider *load_vm_ops(bool NoLazyCompilation) {
	ModuleProvider *MP = NULL;
	Module *theModule = NULL;
	std::string error;

#ifdef USE_BITCODE_FILE
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
	if(NoLazyCompilation) {
		theModule = MP->materializeModule(&error);
		if(!theModule) {
			printf("Failed to read %s file: %s\n", ops_file, error.c_str());
			exit(1);
		}
	}
#else
	theModule = make_lua_vm_ops();
	MP = new llvm::ExistingModuleProvider(theModule);
#endif

	return MP;
}

