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

#include <stdlib.h>
#include <stdio.h>

#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/LLVMContext.h"

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"

#include <string>

#include "load_embedded_bc.h"

llvm::ModuleProvider *load_embedded_bc(llvm::LLVMContext &context,
	const char *name, const unsigned char *start, size_t len, bool NoLazyCompilation)
{
	llvm::ModuleProvider *MP = NULL;
	llvm::Module *module = NULL;
	const char *end = (const char *)start + len - 1;
	std::string error;

	// Load in the bitcode file containing the functions for each
	// bytecode operation.

	llvm::MemoryBuffer* buffer;
	buffer= llvm::MemoryBuffer::getMemBuffer((const char *)start, end, name);
	if(buffer != NULL) {
		MP = llvm::getBitcodeModuleProvider(buffer, context, &error);
		if(!MP) delete buffer;
	}
	if(!MP) {
		printf("Failed to parse embedded '%s' file: %s\n", name, error.c_str());
		exit(1);
	}
	// Get Module from ModuleProvider.
	if(NoLazyCompilation) {
		module = MP->materializeModule(&error);
		if(!module) {
			printf("Failed to materialize embedded '%s' file: %s\n", name, error.c_str());
			exit(1);
		}
	}

	return MP;
}

