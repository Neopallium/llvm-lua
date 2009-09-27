
#
# llvm-lua source
#
set(NON_LLVM_SRC
	lua_core.c
	lua_vm_ops_static.c
	load_jit_proto.c
)
set(LLVM_COMMON_SRC
	LLVMCompiler.cpp
	llvm_compiler.cpp
	load_embedded_bc.cpp
	load_vm_ops.cpp
	lua_vm_ops_bc.h
	${NON_LLVM_SRC}
)
set(NO_JIT_SRC
	no_jit.c
	${NON_LLVM_SRC}
)
set(EMB_BCS
	lua_vm_ops_bc.h
	liblua_main_bc.h
)
set(LLVM_LUA_SRC
	llvm-lua.cpp
	lua_interpreter.c
	${LLVM_COMMON_SRC}
)
set(LLVM_LUAC_SRC
	llvm-luac.cpp
	lua_compiler.c
	LLVMDumper.cpp
	llvm_dumper.cpp
	load_liblua_main.cpp
	liblua_main_bc.h
	${LLVM_COMMON_SRC}
)
set(LLVM_LUA_LIB_SRC
	lua_interpreter.c
	${LLVM_COMMON_SRC}
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
				${PROJECT_SOURCE_DIR}/src
				${CMAKE_CURRENT_BINARY_DIR})

add_executable(bin2c bin2c.c)

#
# Embedded LLVM bitcode files
#
string(REPLACE " " ";" BC_CFLAGS ${COMMON_CFLAGS})
set(BC_CFLAGS ${BC_CFLAGS} -c -emit-llvm -I../src)
add_custom_command(OUTPUT lua_vm_ops.bc
	COMMAND clang ARGS ${BC_CFLAGS} -o lua_vm_ops.bc lua_vm_ops.c
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS lua_vm_ops.c
)
add_custom_command(OUTPUT liblua_main.bc
	COMMAND clang ARGS ${BC_CFLAGS} -o liblua_main.bc lua_main.c
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS lua_main.c
)
add_custom_command(OUTPUT lua_vm_ops_bc.h
	COMMAND bin2c -z -c lua_vm_ops.bc lua_vm_ops_bc.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS bin2c lua_vm_ops.bc
)
add_custom_command(OUTPUT liblua_main_bc.h
	COMMAND bin2c -z -c liblua_main.bc liblua_main_bc.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS bin2c liblua_main.bc
)

#
# Lua library.
#
if(BUILD_STATIC)
	add_library(llvm-lua STATIC ${LLVM_LUA_LIB_SRC})
	add_target_properties(llvm-lua COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
	add_target_properties(llvm-lua LINK_FLAGS "${LD_FLAGS} ${COMMON_LDFLAGS}")
	set(LLVM_LUA_STATIC_LIB llvm-lua)
	set(LLVM_LUA_LIBS llvm-lua)
else(BUILD_STATIC)
	add_library(llvm-lua_static STATIC ${LLVM_LUA_LIB_SRC})
	add_target_properties(llvm-lua_static COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
	add_target_properties(llvm-lua_static LINK_FLAGS "${LD_FLAGS} ${COMMON_LDFLAGS}")
	set(LLVM_LUA_STATIC_LIB llvm-lua_static)

	add_library(llvm-lua SHARED ${LLVM_LUA_LIB_SRC})
	add_target_properties(llvm-lua COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
	add_target_properties(llvm-lua LINK_FLAGS "${LD_FLAGS} ${COMMON_LDFLAGS}")

	set(LLVM_LUA_LIBS llvm-lua_static llvm-lua)
endif(BUILD_STATIC)

#
# static library liblua_main.a
#
add_library(lua_main STATIC ${LLVM_LUA_LIB_SRC})
add_target_properties(lua_main COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
add_target_properties(lua_main LINK_FLAGS "${LD_FLAGS} ${COMMON_LDFLAGS}")
set(LLVM_LUA_LIBS ${LLVM_LUA_LIBS} lua_main)

#
# set properties on libraries.
#
set_target_properties(${LLVM_LUA_LIBS} PROPERTIES
	VERSION ${LLVM_LUA_VERSION}
	SOVERSION ${LLVM_LUA_SOVERSION}
	CLEAN_DIRECT_OUTPUT 1
)

#
# Lua compiler
#
add_executable(llvm-luac ${LLVM_LUAC_SRC})
add_target_properties(llvm-luac COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
add_target_properties(llvm-luac LINK_FLAGS
	"${LD_FLAGS} ${COMMON_LDFLAGS} ${LLVM_LFLAGS}")
target_link_libraries(llvm-luac ${LLVM_LUA_STATIC_LIB};${LLVM_ALL_LIBS})

#
# Lua stand-alone interpreter
#
add_executable(llvm-lua.bin ${LLVM_LUA_SRC})
add_target_properties(llvm-lua.bin COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
add_target_properties(llvm-lua.bin LINK_FLAGS
	"${LD_FLAGS} ${COMMON_LDFLAGS} ${LLVM_LFLAGS}")
target_link_libraries(llvm-lua.bin llvm-lua;${LLVM_JIT_LIBS})
# rename llvm-lub.bin to llvm-lua
set_target_properties(llvm-lua.bin PROPERTIES OUTPUT_NAME llvm-lua)

#
# install llvm-lua/llvm-luac & library.
#
install(TARGETS llvm-luac llvm-lua.bin ${LLVM_LUA_LIBS}
				RUNTIME DESTINATION bin
				LIBRARY DESTINATION lib
				ARCHIVE DESTINATION lib)

install(FILES
	lua-compiler
	DESTINATION bin)

install(FILES
	${LUA_LIB_HEADERS}
	DESTINATION include)

