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

#include <stdlib.h>

#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"

#ifdef USE_BITCODE_FILE
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/System/Path.h"
#include "llvm/Bitcode/ReaderWriter.h"
#endif

#include <string>
#include <vector>

#include "load_vm_ops.h"

#ifndef USE_BITCODE_FILE
#include "lua_vm_ops_module.h"
#endif

llvm::ModuleProvider *load_vm_ops(bool NoLazyCompilation) {
	llvm::ModuleProvider *MP = NULL;
	llvm::Module *module = NULL;
	std::string error;

#ifdef USE_BITCODE_FILE
	std::string ops_file="lua_vm_ops.bc";
	std::vector<llvm::sys::Path> paths;
	llvm::sys::Path tmp(ops_file);
	bool found = false;

	// check current directory for ops file first.
	if(!tmp.isBitcodeFile()) {
		// get bitcode library path.
		llvm::sys::Path::GetBitcodeLibraryPaths(paths);
		// search paths for 'lua_vm_ops.bc' file.
		for(std::vector<llvm::sys::Path>::iterator I=paths.begin(); I != paths.end(); I++) {
			tmp = *I;
			tmp.appendComponent(ops_file);
			if(tmp.isBitcodeFile()) {
				ops_file = tmp.toString();
				found = true;
				break;
			}
		}
	} else {
		found = true;
	}
	if(!found) {
		printf("Failed to find '%s' file.\n", ops_file.c_str());
		printf("Please set environment variable 'LLVM_LIB_SEARCH_PATH' to include the path to '%s'\n", ops_file.c_str());
		exit(1);
	}
	// Load in the bitcode file containing the functions for each
	// bytecode operation.
	if(llvm::MemoryBuffer* buffer = llvm::MemoryBuffer::getFile(ops_file.c_str(), &error)) {
		MP = llvm::getBitcodeModuleProvider(buffer, &error);
		if(!MP) delete buffer;
	}
	if(!MP) {
		printf("Failed to parse %s file: %s\n", ops_file.c_str(), error.c_str());
		exit(1);
	}
	// Get Module from ModuleProvider.
	if(NoLazyCompilation) {
		module = MP->materializeModule(&error);
		if(!module) {
			printf("Failed to read %s file: %s\n", ops_file.c_str(), error.c_str());
			exit(1);
		}
	}
#else
	module = make_lua_vm_ops();
	MP = new llvm::ExistingModuleProvider(module);
#endif

	return MP;
}

