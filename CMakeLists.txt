#
# llvm-lua 1.0
#
cmake_minimum_required(VERSION 2.4 FATAL_ERROR)

project(llvm-lua C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(CustomMacros)
include(FindLLVM)

enable_testing()

option(BUILD_STATIC "build static library" ON)

option(LUA_USE_DLOPEN "Enable dlopen support.")
option(LUA_USE_ISATTY "Enable isatty support.")
option(LUA_USE_READLINE "Enable readline support.")
option(LUA_USE_POPEN "Enable lua_popen support.")
option(LUA_USE_ULONGJMP "Try using _longjmp/_setjmp (more efficient)")
option(LUA_USE_APICHECK "Enable API checks.")
option(LUA_USE_MKSTEMP "Use mkstemp.")
option(LUA_ANSI "Disable non-ansi features." OFF)

#
# llvm-lua options.
#
option(LUA_CPP_SUPPORT "Enable c++ support" OFF)

#
# LuaCoco options
#
option(COCO_USE_SETJMP "Coco: Force use of setjmp (instead of gccasm)" OFF)
option(COCO_USE_UCONTEXT "Coco: Force use of ucontext (instead of gccasm or setjmp)" OFF)
option(COCO_DISABLE "Disable coco" OFF)
#option(COCO_DEFAULT_CSTACKSIZE "Coco default cstacksize")


#
# Lua version
#
set(LUA_VERSION_MAJOR 5)
set(LUA_VERSION_MINOR 1)
set(LUA_VERSION_PATCH 4)
set(LUA_VERSION
				"${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}.${LUA_VERSION_PATCH}")
set(LUA_SOVERSION
				"${LUA_VERSION_MAJOR}")

#
# llvm-lua version
#
set(LLVM_LUA_VERSION_MAJOR 1)
set(LLVM_LUA_VERSION_MINOR 1)
set(LLVM_LUA_VERSION_PATCH 0)
set(LLVM_LUA_VERSION
	"${LLVM_LUA_VERSION_MAJOR}.${LLVM_LUA_VERSION_MINOR}.${LLVM_LUA_VERSION_PATCH}")
set(LLVM_LUA_SOVERSION
	"${LLVM_LUA_VERSION_MAJOR}")

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
set(COMMON_LDFLAGS " -lm ")

#
# Detect system type
#
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(LUA_USE_LINUX TRUE)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_LINUX")
	set(COMMON_LDFLAGS "${COMMON_LDFLAGS} -Wl,-E -ldl -lreadline -lhistory -lncurses")
else(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	if(APPLE)
		set(LUA_USE_MACOSX TRUE)
		set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_LINUX")
		set(COMMON_LDFLAGS "${COMMON_LDFLAGS} -lreadline ")
		# use this on Mac OS X 10.3-
		#set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_MACOSX")
	elseif(WIN32)
		set(LUA_WIN TRUE)
		set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_BUILD_AS_DLL")
	elseif(UNIX)
		set(LUA_USE_POSIX TRUE)
		set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_POSIX")
	else(APPLE)
		set(LUA_ANSI TRUE)
		set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_ANSI")
	endif(APPLE)
endif(CMAKE_SYSTEM_NAME STREQUAL "Linux")

#
# standard flags to use for each build type.
#
if(CMAKE_COMPILER_IS_GNUCC)
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe -Wall -Wextra -Wshadow -W -pedantic -std=gnu99 -fgnu89-inline")
	set(CMAKE_C_FLAGS_RELEASE        "${CMAKE_C_FLAGS_RELEASE}     -O2")
	set(CMAKE_C_FLAGS_DEBUG          "${CMAKE_C_FLAGS_DEBUG}       -O0 -g")
	set(CMAKE_C_FLAGS_PROFILE        "${CMAKE_C_FLAGS_PROFILE}     -O1 -g")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_WITHDEBINFO} -O2 -g")
endif(CMAKE_COMPILER_IS_GNUCC)

#
# sub-folders
#
ADD_SUBDIRECTORY(llvm-lua build)
ADD_SUBDIRECTORY(test)

