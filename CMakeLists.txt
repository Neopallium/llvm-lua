#
# llvm-lua
#
cmake_minimum_required(VERSION 2.4 FATAL_ERROR)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

project(llvm_lua C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(CustomMacros)
include(CMakeDependentOption)
include(FindLLVM)

enable_testing()

#
# Lua version
#
set(LUA_VERSION_MAJOR 5)
set(LUA_VERSION_MINOR 1)
set(LUA_VERSION_PATCH 4)
set(LUA_VERSION
				"${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}.${LUA_VERSION_PATCH}")
set(LUA_SOVERSION
				"${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}")

#
# llvm-lua version
#
set(LLVM_LUA_VERSION_MAJOR 1)
set(LLVM_LUA_VERSION_MINOR 2)
set(LLVM_LUA_VERSION_PATCH 0)
set(LLVM_LUA_VERSION
	"${LLVM_LUA_VERSION_MAJOR}.${LLVM_LUA_VERSION_MINOR}.${LLVM_LUA_VERSION_PATCH}")
set(LLVM_LUA_SOVERSION
	"${LLVM_LUA_VERSION_MAJOR}.${LLVM_LUA_VERSION_MINOR}")

#
# Lua package info.
#
set(CPACK_PACKAGE_VERSION_MAJOR ${LLVM_LUA_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${LLVM_LUA_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${LLVM_LUA_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYRIGHT")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README")
set(CPACK_PACKAGE_VENDOR "")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
"/\\\\.;/\\\\.git.*/;~$;build/;CMakeFiles/;CMakeCache;Testing/;cmake_install;CPack;Dart;Makefile$")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
	"${CMAKE_PROJECT_NAME}-${LLVM_LUA_VERSION_MAJOR}.${LLVM_LUA_VERSION_MINOR}.${LLVM_LUA_VERSION_PATCH}")
# MUST be after CPACK_* variables.
include(CPack)

set(COMMON_CFLAGS)
set(COMMON_LDFLAGS)
set(LIBS)
#
# Detect system type
#
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(DEFAULT_POSIX TRUE)
	set(DEFAULT_DLOPEN ON)
	set(DEFAULT_READLINE ON)
	set(COMMON_LDFLAGS "${COMMON_LDFLAGS} -Wl,-E")
	set(USE_RPATH TRUE)
	# This is a HACK.  We need to put the flags from `llvm-config --lflags` at
	# the end of the linker command.  If you know a better way to fix this let
	# me know.
	set(CMAKE_CXX_LINK_EXECUTABLE
		"<CMAKE_CXX_COMPILER>  <FLAGS> <CMAKE_CXX_LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES> <LINK_FLAGS>")
	SET(CMAKE_C_LINK_EXECUTABLE
		"<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES> <LINK_FLAGS>")
elseif(APPLE)
	set(DEFAULT_POSIX TRUE)
	set(DEFAULT_DLOPEN ON)
	# use this on Mac OS X 10.3-
	option(LUA_USE_MACOSX "Mac OS X 10.3-" OFF)
	set(OSX_ARCHITECTURES "ppc;i386;ppc64;x86_64" CACHE STRING "Build architectures for OSX")
	CMAKE_DEPENDENT_OPTION(WANT_FRAMEWORK
		"Set to ON to build framework instead of dylib." ON
		"WANT_SHARED_LIBRARY" ON
	)
	CMAKE_DEPENDENT_OPTION(LUA_FRAMEWORK_SYMLINKS
		"Set to ON to create symlinks to lua & luac to CMAKE_PREFIX_PATH/bin." ON
		"WANT_FRAMEWORK;WANT_SHARED_LIBRARY" ON
	)
	set(CMAKE_FRAMEWORK_INSTALL_DIR "/Library/Frameworks" CACHE STRING "Directory to install frameworks to.")
	set(CMAKE_FRAMEWORK_INSTALL_NAME_DIR "@executable_path/../Frameworks" CACHE STRING "install_name path for framework.")
	set(CMAKE_DYLIB_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE STRING "install_name path for dylib.")
	set(LUA_FRAMEWORK_NAME "llvm-lua.framework")
	set(LUA_FRAMEWORK_VERSION_NUMBER "${LLVM_LUA_VERSION_MAJOR}.${LLVM_LUA_VERSION_MINOR}")
	set(LUA_FRAMEWORK_VERSIONED_EXECUTABLE_DIR "Versions/${LUA_FRAMEWORK_VERSION_NUMBER}/MacOS")
	set(LUA_FRAMEWORK_VERSIONED_LIB_DIR "Versions/${LUA_FRAMEWORK_VERSION_NUMBER}/lib")
	set(LUA_FRAMEWORK_CURRENT_EXECUTABLE_DIR "Versions/Current/MacOS")

	# For Apple install_name, is it better to detect if Xcode vs Makefile?
	# Xcode default=1, Makefile=0? Or detect if Framework vs. dylib,
	# Framework=1, dylib=0?
	option(CMAKE_BUILD_WITH_INSTALL_RPATH "Set to YES to set the rpath or install_name on build instead of install." ON)

	set(LUA_SOVERSION
				"${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}.0")
elseif(CYGWIN)
	set(DEFAULT_POSIX TRUE)
	set(USE_RPATH TRUE)
elseif(UNIX)
	set(DEFAULT_POSIX TRUE)
	set(USE_RPATH TRUE)
elseif(WIN32)
	set(LUA_WIN TRUE)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_BUILD_AS_DLL")
else(APPLE)
	set(DEFAULT_ANSI TRUE)
endif(CMAKE_SYSTEM_NAME STREQUAL "Linux")

#
# setup config options with default values.
#
option(WANT_SHARED_LIBRARY "Set to ON to build dynamic library." ON)
if(WIN32)
	set(WANT_STATIC_LIBRARY OFF)
else(WIN32)
	option(WANT_STATIC_LIBRARY "Set to ON to build static library." ON)
endif(WIN32)

if(USE_RPATH)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib" CACHE STRING "rpaths separated by semicolons.")
	option(CMAKE_BUILD_WITH_INSTALL_RPATH "Set to YES to set the rpath or install_name on build instead of install." OFF)
endif(USE_RPATH)

if(DEFAULT_DLOPEN)
	option(LUA_USE_DLOPEN "Enable dlopen support." ON)
else(DEFAULT_DLOPEN)
	option(LUA_USE_DLOPEN "Enable dlopen support." OFF)
endif(DEFAULT_DLOPEN)

if(DEFAULT_POSIX)
	option(LUA_USE_CURSES "Enable Curses support." ON)
	option(LUA_USE_MKSTEMP "Use mkstemp." ON)
	option(LUA_USE_ISATTY "Enable isatty support." ON)
	option(LUA_USE_POPEN "Enable lua_popen support." ON)
	option(LUA_USE_ULONGJMP "Try using _longjmp/_setjmp (more efficient)" ON)
else(DEFAULT_POSIX)
	option(LUA_USE_CURSES "Enable Curses support." OFF)
	option(LUA_USE_MKSTEMP "Use mkstemp." OFF)
	option(LUA_USE_ISATTY "Enable isatty support." OFF)
	option(LUA_USE_POPEN "Enable lua_popen support." OFF)
	option(LUA_USE_ULONGJMP "Try using _longjmp/_setjmp (more efficient)" OFF)
endif(DEFAULT_POSIX)

if(DEFAULT_READLINE)
	option(LUA_USE_READLINE "Enable readline support." ON)
else(DEFAULT_READLINE)
	option(LUA_USE_READLINE "Enable readline support." OFF)
endif(DEFAULT_READLINE)

if(DEFAULT_ANSI)
	option(LUA_ANSI "Disable non-ansi features." ON)
else(DEFAULT_ANSI)
	option(LUA_ANSI "Disable non-ansi features." OFF)
endif(DEFAULT_ANSI)

option(LUA_USE_APICHECK "Enable API checks." OFF)

#
# llvm-lua options.
#
option(LUA_CPP_SUPPORT "Enable c++ support" OFF)
set(LLVM_CC "clang" CACHE STRING "Command to compile C-code to LLVM bitcode (clang or llvm-gcc)")
set(LLVM_LINK "llvm-link" CACHE STRING "Command to link LLVM bitcode files into one bitcode file.")

#
# Cross-compile options.
#
option(CROSS_COMPILE "Build llvm-luac as a cross-compiler" OFF)
set(CROSS_ARCH "arm" CACHE STRING "Cross-compiler target.")
set(CROSS_TRIPLE "arm-apple-darwin9" CACHE STRING "Cross-compiler target triple.")
set(CROSS_CPU "arm1176jzf-s" CACHE STRING "Cross-compiler target cpu name.")
set(CROSS_ISYSTEM "" CACHE STRING "Path to target system includes.")
set(FORCE_ALIGNMENT "4" CACHE STRING "Force byte alignment, for clang-cc hack.")

#
# LuaCoco options
#
option(COCO_USE_SETJMP "Coco: Force use of setjmp (instead of gccasm)" OFF)
option(COCO_USE_UCONTEXT "Coco: Force use of ucontext (instead of gccasm or setjmp)" OFF)
option(COCO_USE_VALGRIND "Enable valgrind debugging support" OFF)
option(COCO_DISABLE "Disable coco" OFF)
set(COCO_DEFAULT_CSTACKSIZE "" CACHE STRING "Coco default cstacksize")

#
# libs & cflags
#
set(COMMON_LDFLAGS "${COMMON_LDFLAGS} -lm ")

# For "Mac OS X 10.3-"
if(LUA_USE_MACOSX)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_MACOSX")
	set(LUA_USE_DLOPEN FALSE)
endif(LUA_USE_MACOSX)

# enable options
if(LUA_USE_DLOPEN)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_DLOPEN")
	if(NOT APPLE)
		set(COMMON_LDFLAGS "${COMMON_LDFLAGS} -ldl ")
	endif(NOT APPLE)
endif(LUA_USE_DLOPEN)
if(LUA_USE_MKSTEMP)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_MKSTEMP")
endif(LUA_USE_MKSTEMP)
if(LUA_USE_ISATTY)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_ISATTY")
endif(LUA_USE_ISATTY)
if(LUA_USE_POPEN)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_POPEN")
endif(LUA_USE_POPEN)
if(LUA_USE_ULONGJMP)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_ULONGJMP")
endif(LUA_USE_ULONGJMP)
if(LUA_USE_APICHECK)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_APICHECK")
endif(LUA_USE_APICHECK)
if(LUA_ANSI)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_ANSI")
endif(LUA_ANSI)

# readline support
if(LUA_USE_READLINE)
	FIND_PATH(READLINE_INCLUDE_DIR readline/readline.h)
	FIND_LIBRARY(READLINE_LIBRARY NAMES readline)
	if(READLINE_LIBRARY)
		set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_READLINE")
		set(COMMON_LDFLAGS "${COMMON_LDFLAGS} -lreadline -lhistory")
		include_directories(${READLINE_INCLUDE_DIR})
	endif(READLINE_LIBRARY)
endif(LUA_USE_READLINE)

# curses
if(LUA_USE_CURSES)
	include(FindCurses)
	if(CURSES_LIBRARY)
		include_directories(${CURSES_INCLUDE_DIR})
		set(LIBS ${LIBS} ${CURSES_LIBRARY})
	endif(CURSES_LIBRARY)
endif(LUA_USE_CURSES)

# LuaCoco flags
if(COCO_DISABLE)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DCOCO_DISABLE")
else(COCO_DISABLE)
	if(COCO_USE_SETJMP)
		set(COMMON_CFLAGS "${COMMON_CFLAGS} -DCOCO_USE_SETJMP")
	elseif(COCO_USE_UCONTEXT)
		set(COMMON_CFLAGS "${COMMON_CFLAGS} -DCOCO_USE_UCONTEXT")
	endif(COCO_USE_SETJMP)

	# check cstacksize variable.
	if(COCO_DEFAULT_CSTACKSIZE)
		set(COMMON_CFLAGS "${COMMON_CFLAGS} -DCOCO_DEFAULT_CSTACKSIZE=${COCO_DEFAULT_CSTACKSIZE}")
	endif(COCO_DEFAULT_CSTACKSIZE)
endif(COCO_DISABLE)

#
# Support for embedding llvm-lua into c++ programs with exception support.
#
if(LUA_CPP_SUPPORT)
	message(STATUS "Lua C++ support enabled, forcing usage of g++ & llvm-g++")
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_CPP_SUPPORT")
	include(CMakeForceCompiler)
	CMAKE_FORCE_C_COMPILER(${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ID})
	set(LLVM_CC "llvm-g++")
endif(LUA_CPP_SUPPORT)

#
# standard flags to use for each build type.
#
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe -Wall -Wshadow -W ")
	set(CMAKE_C_FLAGS_RELEASE        "${CMAKE_C_FLAGS_RELEASE}     -O2")
	set(CMAKE_C_FLAGS_DEBUG          "${CMAKE_C_FLAGS_DEBUG}       -O0 -g")
	set(CMAKE_C_FLAGS_PROFILE        "${CMAKE_C_FLAGS_PROFILE}     -O1 -g")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_WITHDEBINFO} -O2 -g")
endif(CMAKE_COMPILER_IS_GNUCC)

#
# For uninstall (needs cmake_uninstall.cmake.in in the top-level directory)
#
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

#
# sub-folders
#
add_subdirectory(llvm-lua build)

