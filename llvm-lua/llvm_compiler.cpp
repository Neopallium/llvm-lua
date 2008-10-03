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

#include "LLVMCompiler.h"
#include "LLVMDumper.h"

extern "C" {

static LLVMCompiler *compiler = NULL;
static LLVMDumper *dumper = NULL;

int llvm_compiler_main(int useJIT, int argc, char ** argv) {
	compiler = new LLVMCompiler(useJIT);
	return 0;
}

void llvm_compiler_cleanup() {
	if(dumper) delete dumper;
	delete compiler;
	dumper = NULL;
	compiler = NULL;
}

void llvm_compiler_optimize(Proto *p, int optimize) {
	compiler->optimize(p, optimize);
}

void llvm_compiler_optimize_all(Proto *p, int optimize) {
	compiler->optimizeAll(p, optimize);
}

void llvm_compiler_compile(Proto *p, int optimize) {
	compiler->compile(p, optimize);
}

void llvm_compiler_compile_all(Proto *p, int optimize) {
	compiler->compileAll(p, optimize);
}

void llvm_compiler_dump(const char *output, Proto *p, int optimize, int stripping) {
	if(dumper == NULL) dumper = new LLVMDumper(compiler);
	dumper->dump(output, p, optimize, stripping);
}

void llvm_compiler_free(Proto *p) {
	compiler->free(p);
}

}// end: extern "C"

