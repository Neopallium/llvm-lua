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

#include <stdio.h>

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/Config/config.h"
#include "llvm/LinkAllVMCore.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"

#include "llvm_compiler.h"
#include "lua_compiler.h"

namespace {
  llvm::cl::list<std::string>
  InputFiles(llvm::cl::Positional, llvm::cl::OneOrMore, llvm::cl::desc("[filenames]"));

  llvm::cl::opt<bool>
  Bitcode("bc",
            llvm::cl::desc("output LLVM bitcode"));

  llvm::cl::list<std::string>
  Libraries("L",
            llvm::cl::desc("preload Lua library 'name'"),
              llvm::cl::value_desc("name"),
              llvm::cl::ZeroOrMore,
              llvm::cl::Prefix);

  llvm::cl::list<bool>
  ListOpcodes("l",
            llvm::cl::desc("list opcodes"),
              llvm::cl::ZeroOrMore);

  llvm::cl::opt<std::string>
  Output("o",
            llvm::cl::desc("output to file 'name' (default is \"luac.out\")"),
              llvm::cl::value_desc("name"));

  llvm::cl::opt<bool>
  ParseOnly("p",
            llvm::cl::desc("parse only"));

  llvm::cl::opt<bool>
  StripDebug("s",
            llvm::cl::desc("strip debug information"));

  llvm::cl::opt<bool>
  ShowVersion("v",
            llvm::cl::desc("show version information"));

}

void print_version() {
	printf(LLVM_LUA_VERSION " " LLVM_LUA_COPYRIGHT "\n");
	printf(LUA_RELEASE "  " LUA_COPYRIGHT "\n");
	llvm::cl::PrintVersionMessage();
}

/*
 *
 */
int main(int argc, char ** argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc, argv);
	std::vector<std::string> arg_list;
	llvm::llvm_shutdown_obj Y;   // Call llvm_shutdown() on exit.
	std::string tmp;
	char **luac_argv;
	int luac_argc=0;
	int new_argc=0;
	int pos;
	int ret;

  // Initialize targets first.
  llvm::InitializeAllTargets();
  llvm::InitializeAllAsmPrinters();

	llvm::cl::SetVersionPrinter(print_version);
	llvm::cl::ParseCommandLineOptions(argc, argv, 0, true);
	// Show version?
	if(ShowVersion) {
		print_version();
		return 0;
	}
	// recreate arg list.
	arg_list.push_back(argv[0]);
	for(std::vector<std::string>::iterator I=Libraries.begin(); I != Libraries.end(); I++) {
		pos = Libraries.getPosition(I - Libraries.begin());
		// keep same format -llibrary or -l library
		if(argv[pos][0] == '-' && argv[pos][1] == 'L') {
			tmp = "-L";
			tmp.append(*I);
			arg_list.push_back(tmp);
		} else {
			arg_list.push_back("-L");
			arg_list.push_back(*I);
		}
	}
	if(Bitcode) {
		arg_list.push_back("-bc");
	}
	for(std::vector<bool>::iterator I=ListOpcodes.begin(); I != ListOpcodes.end(); I++) {
		arg_list.push_back("-l");
	}
	if(!Output.empty()) {
		arg_list.push_back("-o");
		arg_list.push_back(Output);
	}
	if(ParseOnly) {
		arg_list.push_back("-p");
	}
	if(StripDebug) {
		arg_list.push_back("-s");
	}
	arg_list.insert(arg_list.end(),InputFiles.begin(), InputFiles.end());
	/* construct luac_argc, luac_argv. */
	new_argc = arg_list.size() + 1;
	luac_argv = (char **)calloc(new_argc, sizeof(char *));
	for(std::vector<std::string>::iterator I=arg_list.begin(); I != arg_list.end(); I++) {
		if(luac_argc == new_argc) break;
		luac_argv[luac_argc] = (char *)(*I).c_str();
		luac_argc++;
	}
	luac_argv[luac_argc] = NULL;

	// initialize the Lua to LLVM compiler.
	ret = llvm_compiler_main(0);
	// Run the main Lua compiler
	ret = luac_main(luac_argc, luac_argv);
	return ret;
}

