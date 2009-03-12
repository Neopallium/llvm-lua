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

#include "llvm-luac.h"
#include "llvm_compiler.h"
#include "lua_compiler.h"
#include "llvm/Support/CommandLine.h"

namespace {
  llvm::cl::list<std::string>
  InputFiles(llvm::cl::Positional, llvm::cl::OneOrMore, llvm::cl::desc("[filenames]"));

  llvm::cl::opt<bool>
  Bitcode("bc",
            llvm::cl::desc("output LLVM bitcode"));

  llvm::cl::opt<bool>
  ListOpcodes("l",
            llvm::cl::desc("list opcodes"));

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
	std::vector<std::string> arg_list;
	int new_argc=0;
	int ret;

	llvm::cl::SetVersionPrinter(print_version);
	llvm::cl::ParseCommandLineOptions(argc, argv, 0, true);
	// Show version?
	if(ShowVersion) {
		print_version();
		return 0;
	}
	// recreate arg list.
	arg_list.push_back(argv[0]);
	if(Bitcode) {
		arg_list.push_back("-bc");
	}
	if(ListOpcodes) {
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
	for(std::vector<std::string>::iterator I=arg_list.begin(); I != arg_list.end(); I++) {
		if(new_argc == argc) break;
		argv[new_argc] = (char *)(*I).c_str();
		new_argc++;
	}
	argv[new_argc] = NULL;

	// initialize the Lua to LLVM compiler.
	ret = llvm_compiler_main(0);
	// Run the main Lua compiler
	ret = luac_main(new_argc, argv);
	return ret;
}

