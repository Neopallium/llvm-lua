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

#include "llvm-lua.h"
#include "llvm_compiler.h"
#include "lua_interpreter.h"
#include "llvm/Support/CommandLine.h"

namespace {
  llvm::cl::opt<std::string>
  InputFile(llvm::cl::Positional, llvm::cl::desc("<script>"), llvm::cl::init("-"));

  llvm::cl::list<std::string>
  InputArgv(llvm::cl::ConsumeAfter, llvm::cl::desc("<script arguments>..."));

  llvm::cl::opt<std::string>
  Execute("e",
            llvm::cl::desc("execuate string 'stat'"),
              llvm::cl::value_desc("stat"));

  llvm::cl::opt<std::string>
  Library("l",
            llvm::cl::desc("require library 'name'"),
              llvm::cl::value_desc("name"));

  llvm::cl::opt<std::string>
  MemLimit("m",
            llvm::cl::desc("set memory limit. (units ar in Kbytes)"),
              llvm::cl::value_desc("limit"));

  llvm::cl::opt<bool>
  Interactive("i",
            llvm::cl::desc("enter interactive mode after executing 'script'"));

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
	if(!Execute.empty()) {
		arg_list.push_back("-e");
		arg_list.push_back(Execute);
	}
	if(!Library.empty()) {
		arg_list.push_back("-l");
		arg_list.push_back(Library);
	}
	if(!MemLimit.empty()) {
		arg_list.push_back("-m");
		arg_list.push_back(MemLimit);
	}
	if(Interactive) {
		arg_list.push_back("-i");
	}
	arg_list.push_back(InputFile);
	arg_list.insert(arg_list.end(),InputArgv.begin(), InputArgv.end());
	for(std::vector<std::string>::iterator I=arg_list.begin(); I != arg_list.end(); I++) {
		if(new_argc == argc) break;
		argv[new_argc] = (char *)(*I).c_str();
		new_argc++;
	}
	argv[new_argc] = NULL;

	// initialize the Lua to LLVM compiler.
	ret = llvm_compiler_main(1);
	// Run the main "interpreter loop" now.
	ret = lua_main(new_argc, argv);
	return ret;
}

