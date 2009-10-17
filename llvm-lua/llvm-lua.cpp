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
#include "llvm/Support/ManagedStatic.h"

namespace {
  llvm::cl::opt<std::string>
  InputFile(llvm::cl::Positional, llvm::cl::desc("<script>"));

  llvm::cl::list<std::string>
  InputArgv(llvm::cl::ConsumeAfter, llvm::cl::desc("<script arguments>..."));

  llvm::cl::list<std::string>
  Executes("e",
            llvm::cl::desc("execuate string 'stat'"),
              llvm::cl::value_desc("stat"),
              llvm::cl::ZeroOrMore,
              llvm::cl::Prefix);

  llvm::cl::list<std::string>
  Libraries("l",
            llvm::cl::desc("require library 'name'"),
              llvm::cl::value_desc("name"),
              llvm::cl::ZeroOrMore,
              llvm::cl::Prefix);

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
	llvm::llvm_shutdown_obj Y;   // Call llvm_shutdown() on exit.
	std::string tmp;
	char **lua_argv;
	int lua_argc=0;
	int new_argc=0;
	int pos;
	int ret;

	// check for '--' and '-' options to cut-off parsing at that point.
	for(new_argc=0; new_argc < argc; new_argc++) {
		if(argv[new_argc][0] == '-') {
			if(argv[new_argc][1] == '\0') {
				break;
			} else if(argv[new_argc][1] == '-' && argv[new_argc][2] == '\0') {
				break;
			}
		}
	}

	llvm::cl::SetVersionPrinter(print_version);
	llvm::cl::ParseCommandLineOptions(new_argc, argv, 0, true);
	// Show version?
	if(ShowVersion) {
		print_version();
		return 0;
	}
	// recreate arg list.
	arg_list.push_back(argv[0]);
	for(std::vector<std::string>::iterator I=Executes.begin(); I != Executes.end(); I++) {
		pos = Executes.getPosition(I - Executes.begin());
		// keep same format -e'statement' or -e 'statement'
		if(argv[pos][0] == '-' && argv[pos][1] == 'e') {
			tmp = "-e";
			tmp.append(*I);
			arg_list.push_back(tmp);
		} else {
			arg_list.push_back("-e");
			arg_list.push_back(*I);
		}
	}
	for(std::vector<std::string>::iterator I=Libraries.begin(); I != Libraries.end(); I++) {
		pos = Libraries.getPosition(I - Libraries.begin());
		// keep same format -llibrary or -l library
		if(argv[pos][0] == '-' && argv[pos][1] == 'l') {
			tmp = "-l";
			tmp.append(*I);
			arg_list.push_back(tmp);
		} else {
			arg_list.push_back("-l");
			arg_list.push_back(*I);
		}
	}
	if(!MemLimit.empty()) {
		arg_list.push_back("-m");
		arg_list.push_back(MemLimit);
	}
	if(Interactive) {
		arg_list.push_back("-i");
	}
	if(!InputFile.empty()) {
		arg_list.push_back(InputFile);
	}
	// append options from cut-off point.
	for(;new_argc < argc; new_argc++) {
		arg_list.push_back(argv[new_argc]);
	}
	arg_list.insert(arg_list.end(),InputArgv.begin(), InputArgv.end());
	/* construct lua_argc, lua_argv. */
	new_argc = arg_list.size() + 1;
	lua_argv = (char **)calloc(new_argc, sizeof(char *));
	for(std::vector<std::string>::iterator I=arg_list.begin(); I != arg_list.end(); I++) {
		if(lua_argc == new_argc) break;
		lua_argv[lua_argc] = (char *)(*I).c_str();
		lua_argc++;
	}
	lua_argv[lua_argc] = NULL;

	// initialize the Lua to LLVM compiler.
	ret = llvm_compiler_main(1);
	// Run the main "interpreter loop" now.
	ret = lua_main(lua_argc, lua_argv);
	return ret;
}

